#include "SamplerEditor.h"
#include <cstring>
#include <cmath>

using namespace inverso_abismo::sampler;
namespace c = soft::colour;

namespace {
constexpr int designW=1320, designH=1000;

const char* const* shortNames(){
 static const char* names[P::Count]={
  "MODE","ROOT NOTE","TUNE","START","REVERSE","LOOP","START","END","CROSSFADE",
  "POSITION","SCAN","SIZE","DENSITY","SPRAY","PITCH RAND.","REVERSE","STEREO",
  "TYPE","CUTOFF","RESONANCE","ENVELOPE","KEYTRACK","DRIVE",
  "VOICE","VOWEL","FORMANT",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY",
  "ATTACK","DECAY","SUSTAIN","RELEASE",
  "GLISSANDO","TIME","DRIFT","BEND",
  "RATE","PITCH","FILTER","VOLUME","POSITION",
  "TENSION","BITS","RATE","DIVISION","DEPTH","CHAOS",
  "MIX","SIZE","DECAY","DARKNESS","FREEZE","WIDTH","OUTPUT",
  "TIMING","TIME","AT KNOB","END","RATE","SATURATION","ON"};
 return names;
}

juce::Path curve(juce::Rectangle<float> r,int points,const std::function<float(float)>& f){
 juce::Path p;
 for(int i=0;i<=points;++i){
  const float t=static_cast<float>(i)/static_cast<float>(points);
  const float y=juce::jlimit(-1.2f,1.2f,f(t));
  const juce::Point<float> pt(r.getX()+t*r.getWidth(),r.getCentreY()-y*r.getHeight()*.5f);
  if(i==0) p.startNewSubPath(pt); else p.lineTo(pt);
 }
 return p;
}
void caption(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& left,const juce::String& right={}){
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left,r.withHeight(12.f),juce::Justification::topLeft);
 if(right.isNotEmpty()) g.drawText(right,r.withHeight(12.f),juce::Justification::topRight);
}
void notice(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& text){
 g.setFont(soft::uiFont(11.f,true));
 g.setColour(c::screenMuted);
 g.drawText(text,r,juce::Justification::centred);
}
juce::Path adsr(juce::Rectangle<float> r,float a,float d,float s,float rel){
 auto w=[](float ms){return std::log10(1.f+ms/4.f);};
 const float total=w(a)+w(d)+1.2f+w(rel);
 const float k=r.getWidth()/total;
 const float x1=r.getX()+w(a)*k,x2=x1+w(d)*k,x3=x2+1.2f*k,x4=x3+w(rel)*k;
 const float top=r.getY(),bottom=r.getBottom(),sy=bottom-s*r.getHeight();
 juce::Path p;
 p.startNewSubPath(r.getX(),bottom);
 p.quadraticTo((r.getX()+x1)*.5f,top+(bottom-top)*.15f,x1,top);
 p.quadraticTo(x1+(x2-x1)*.25f,sy,x2,sy);
 p.lineTo(x3,sy);
 p.quadraticTo(x3+(x4-x3)*.2f,bottom,x4,bottom);
 return p;
}
}

// ---------------------------------------------------------------- sample view

SampleView::SampleView(InversoAbismoSamplerProcessor& p):processor(p){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }

float SampleView::param(const char* id) const { return processor.apvts.getRawParameterValue(id)->load(); }

void SampleView::refresh(){
 peaks.clear();
 const auto sample=processor.getDisplaySample();
 const int columns=std::max(1,getWidth()/2);
 if(!sample || sample->length()==0){repaint();return;}
 const auto& l=sample->left;
 const auto& r=sample->right.empty()?sample->left:sample->right;
 const double per=static_cast<double>(sample->length())/columns;
 peaks.resize(static_cast<std::size_t>(columns));
 for(int col=0;col<columns;++col){
  const auto a=static_cast<std::size_t>(col*per),b=std::max(a+1,static_cast<std::size_t>((col+1)*per));
  float lo=0.f,hi=0.f;
  for(std::size_t i=a;i<b && i<l.size();++i){lo=std::min({lo,l[i],r[i]});hi=std::max({hi,l[i],r[i]});}
  peaks[static_cast<std::size_t>(col)]={lo,hi};
 }
 repaint();
}

void SampleView::paint(juce::Graphics& g){
 flags.clear();
 lines.clear();
 const auto area=getLocalBounds().toFloat().reduced(12.f,8.f);
 const auto wave=area.withTrimmedTop(16.f);
 g.setFont(soft::monoFont(10.f));
 g.setColour(statusIsError?c::red:c::screenMuted);
 g.drawText(status,area.withHeight(12.f),juce::Justification::topLeft);
 const float w=wave.getWidth(),mid=wave.getCentreY(),half=wave.getHeight()*.48f;
 const bool granular=param("MODE")>=.5f;
 const int loop=juce::roundToInt(param("LOOP_MODE"));
 if(peaks.empty()){
  g.setFont(soft::uiFont(12.f,true));
  g.setColour(c::screenMuted);
  g.drawText(juce::String::fromUTF8("NO SAMPLE  \xc2\xb7  DROP A FILE HERE"),wave,juce::Justification::centred);
  return;
 }
 juce::Path shape;
 const float step=w/static_cast<float>(peaks.size());
 // REVERSE plays the sound backwards and START, END and the loop points refer to the reversed
 // sound, so the wave is drawn reversed too: the markers sit where they are heard
 const bool reversed=param("REVERSE")>=.5f;
 for(std::size_t x=0;x<peaks.size();++x){
  const auto [lo,hi]=peaks[reversed?peaks.size()-1-x:x];
  const float px=wave.getX()+static_cast<float>(x)*step;
  shape.addRectangle(px,mid-hi*half,std::max(1.f,step*.7f),std::max(1.f,(hi-lo)*half));
 }
 g.setColour(c::amber.withAlpha(.8f));
 g.fillPath(shape);
 // START..END is the part of the sample a note uses (outside it is shaded); LOOP IN..LOOP OUT,
 // when LOOP is Forward or Ping-pong, is the part that repeats while the key is held. In granular
 // mode SCAN travels the same way and POSITION is where the grain cloud starts.
 const float xs=wave.getX()+param("START")*.01f*w,xe=wave.getX()+param("END")*.01f*w;
 g.setColour(c::screenBottom.withAlpha(.7f));
 g.fillRect(wave.getX(),wave.getY(),std::max(0.f,xs-wave.getX()),wave.getHeight());
 g.fillRect(xe,wave.getY(),std::max(0.f,wave.getRight()-xe),wave.getHeight());
 const float xa=wave.getX()+std::min(param("LOOP_START"),param("LOOP_END"))*.01f*w,xb=wave.getX()+std::max(param("LOOP_START"),param("LOOP_END"))*.01f*w;
 if(loop>0){
  g.setColour(c::amber.withAlpha(.16f));
  g.fillRect(xa,wave.getY(),std::max(1.f,xb-xa),wave.getHeight());
 }
 // each flag goes on the first row where it does not cover another, kept inside the wave
 auto marker=[&](const char* id,float x,juce::Colour col,const juce::String& label,bool flagRight){
  g.setColour(col);
  g.fillRect(x-1.f,wave.getY(),2.f,wave.getHeight());
  lines.push_back({id,x});
  g.setFont(soft::uiFont(9.f,true));
  const float fw=juce::GlyphArrangement::getStringWidth(g.getCurrentFont(),label)+8.f;
  float left=flagRight?x:x-fw;
  left=fw>=wave.getWidth()?wave.getX():juce::jlimit(wave.getX(),wave.getRight()-fw,left);
  juce::Rectangle<float> flag;
  for(int row=0;row<4;++row){
   flag={left,wave.getY()+static_cast<float>(row)*13.f,fw,12.f};
   bool free=true;
   for(const auto& f:flags) if(f.second.intersects(flag)) { free=false; break; }
   if(free) break;
  }
  flags.push_back({id,flag});
  g.setColour(col);
  g.fillRect(flag);
  g.setColour(c::screenBottom);
  g.drawText(label,flag,juce::Justification::centred);
 };
 const bool swapped=loopSwapped();
 marker("START",xs,c::cyan,"START",true);
 marker("END",xe,c::pink,"END",false);
 if(loop>0){
  marker(swapped?"LOOP_END":"LOOP_START",xa,c::amber,"LOOP IN",true);
  marker(swapped?"LOOP_START":"LOOP_END",xb,c::amber,"LOOP OUT",false);
 }
 if(granular){
  // drawn where the cloud starts: a POSITION outside START..END starts at the nearest edge
  const float a=std::min(param("START"),param("END")),b=std::max(param("START"),param("END"));
  const float pos=wave.getX()+juce::jlimit(a,b,param("POSITION"))*.01f*w,spray=param("SPRAY")*.01f;
  const auto sample=processor.getDisplaySample();
  if(sample && sample->length()>0){
   const float sprayPx=static_cast<float>(spray*.5*sample->sampleRate/sample->length())*w;
   g.setColour(c::cyan.withAlpha(.18f));
   g.fillRect(pos-sprayPx,wave.getY(),sprayPx*2.f,wave.getHeight());
  }
  marker("POSITION",pos,juce::Colours::white.withAlpha(.85f),"POSITION",true);
 }
 // what the markers do, in one line under the wave
 g.setFont(soft::monoFont(9.f));
 g.setColour(c::screenMuted);
 const char* hint=granular
  ?(loop==0?"DRAG THE MARKERS  \xc2\xb7  SCAN TRAVELS START..END, ROUND AND ROUND  \xc2\xb7  LOOP: FORWARD OR PING-PONG KEEPS IT BETWEEN LOOP IN AND OUT"
           :"DRAG THE MARKERS  \xc2\xb7  SCAN TRAVELS BETWEEN LOOP IN AND LOOP OUT  \xc2\xb7  SCAN 0 = THE CLOUD STANDS STILL AT POSITION")
  :(loop==0?"DRAG THE MARKERS  \xc2\xb7  A NOTE PLAYS FROM START TO END  \xc2\xb7  SET LOOP TO FORWARD OR PING-PONG TO REPEAT A PART WHILE THE KEY IS HELD"
           :"DRAG THE MARKERS  \xc2\xb7  A NOTE STARTS AT START  \xc2\xb7  WHILE THE KEY IS HELD, LOOP IN..LOOP OUT REPEATS");
 const auto strip=wave.withY(wave.getBottom()-12.f).withHeight(12.f);
 g.setColour(c::screenBottom.withAlpha(.85f));
 g.fillRect(strip);
 g.setColour(c::screenMuted);
 g.drawText(juce::String::fromUTF8(hint),strip.reduced(4.f,0.f),juce::Justification::centredLeft);
 const float head=processor.playhead.load();
 if(head>=0.f){ g.setColour(juce::Colours::white); g.fillRect(wave.getX()+head*w,wave.getY(),1.5f,wave.getHeight()); }
}

// LOOP IN is LOOP START unless the loop was saved the other way round (LOOP START after LOOP END).
bool SampleView::loopSwapped() const { return param("LOOP_START")>param("LOOP_END"); }

// Which marker a click grabs: a flag clicked is that marker; elsewhere the nearest line as drawn,
// POSITION first on a tie.
const char* SampleView::markerAt(juce::Point<float> p) const {
 for(const auto& f:flags) if(f.second.expanded(2.f).contains(p)) return f.first;
 if(lines.empty()) return "START";
 const char* best=nullptr;
 float bestDistance=0.f;
 for(const auto& [id,x]:lines){
  const float d=std::abs(x-p.x);
  const bool position=std::strcmp(id,"POSITION")==0;
  if(best==nullptr || d<bestDistance || (position && d<=bestDistance)){ best=id; bestDistance=d; }
 }
 return best;
}
void SampleView::setFromX(float x){
 if(dragging==nullptr) return;
 auto* p=processor.apvts.getParameter(dragging);
 const auto wave=getLocalBounds().toFloat().reduced(12.f,8.f);
 float value=juce::jlimit(0.f,100.f,(x-wave.getX())/std::max(1.f,wave.getWidth())*100.f);
 // START stays before END
 if(std::strcmp(dragging,"START")==0) value=std::min(value,param("END")-.2f);
 if(std::strcmp(dragging,"END")==0) value=std::max(value,param("START")+.2f);
 if(std::strcmp(dragging,"LOOP_START")==0 || std::strcmp(dragging,"LOOP_END")==0){
  const float other=param(std::strcmp(dragging,"LOOP_START")==0?"LOOP_END":"LOOP_START");
  value=draggingLoopIn?std::min(value,other-.2f):std::max(value,other+.2f); // LOOP IN stays before LOOP OUT
 }
 p->setValueNotifyingHost(p->convertTo0to1(value));
 repaint();
}
void SampleView::mouseDown(const juce::MouseEvent& e){
 dragging=markerAt(e.position); // from the flags as last drawn
 // a loop point drags as LOOP IN or LOOP OUT, whichever it is now (saved loops can be reversed)
 const bool loopPoint=std::strcmp(dragging,"LOOP_START")==0 || std::strcmp(dragging,"LOOP_END")==0;
 draggingLoopIn=loopPoint && (std::strcmp(dragging,loopSwapped()?"LOOP_END":"LOOP_START")==0);
 processor.apvts.getParameter(dragging)->beginChangeGesture();
 setFromX(e.position.x);
}
void SampleView::mouseDrag(const juce::MouseEvent& e){ setFromX(e.position.x); }
void SampleView::mouseUp(const juce::MouseEvent&){
 if(dragging!=nullptr) processor.apvts.getParameter(dragging)->endChangeGesture();
 dragging=nullptr;
}

// ---------------------------------------------------------------- editor

float InversoAbismoSamplerEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoAbismoSamplerEditor::InversoAbismoSamplerEditor(InversoAbismoSamplerProcessor& p)
 :AudioProcessorEditor(&p),processor(p),view(p){
 setLookAndFeel(&look);
 body.title="ABISMO SAMPLER";
 body.subtitle=juce::String::fromUTF8("YOUR SOUNDS, CLASSIC OR GRANULAR   \xc2\xb7   DROP AN AUDIO FILE ON THE WINDOW   \xc2\xb7   MOD WHEEL = TENSION");
 addAndMakeVisible(body);
 presets.getCount=[this]{return processor.getNumPrograms();};
 presets.getCurrent=[this]{return processor.getCurrentProgram();};
 presets.getName=[this](int i){return processor.getProgramName(i);};
 presets.select=[this](int i){processor.setCurrentProgram(i);};
 presets.enableUserPresets(processor.apvts, "ABISMO SAMPLER");
 body.addAndMakeVisible(presets);

 using Page=soft::Module::Page;
 auto add=[&](juce::String title,juce::Colour accent,std::vector<Page> pages,int screenH,std::vector<int> choices)->soft::Module&{
  modules.push_back(std::make_unique<soft::Module>(processor.apvts,specs().data(),shortNames(),title,accent,std::move(pages),screenH,std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 auto& sample=add("SAMPLE",c::amber,{{"Play",{P::Tune,P::RootNote,P::Start,P::End,P::Reverse}},
                                     {"Loop",{P::LoopStart,P::LoopEnd,P::Xfade}}},176,{P::Mode,P::LoopMode});
 add("GRANULAR",c::cyan,{{"Cloud",{P::Position,P::Scan,P::GrainSize,P::Density,P::Spray},{P::Position}},
                         {"Grains",{P::GrainPitch,P::GrainReverse,P::GrainSpread,P::LfoPosition}}},176,{});
 add("FILTER",c::violet,{{"Filter",{P::Cutoff,P::Resonance,P::FilterEnv,P::Keytrack,P::Drive},{P::Cutoff}},
                         {"Envelope",{P::FAttack,P::FDecay,P::FSustain,P::FRelease}}},120,{P::FilterMode});
 add("ENVELOPE",c::green,{{"Volume",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens}}},96,{});
 add("VOICE",c::pink,{{"Voice",{P::VoiceMix,P::Vowel,P::FormantShift}}},96,{});
 add("MOVEMENT",c::blue,{{"Pitch",{P::PitchEnv,P::PitchTime,P::Drift,P::BendRange,P::Tension}},
                          {"LFO",{P::LfoRate,P::LfoPitch,P::LfoFilter,P::LfoAmp}}},96,{});
 add("MUTILATION",c::orange,{{"Chop",{P::StutterMs,P::StutterDepth,P::Chaos}}},104,{P::StutterSync,P::StutterDiv});
 add("SPACE",c::blue.interpolatedWith(c::violet,.5f),
     {{"Reverb",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::Freeze},{P::RevMix}}},104,{});
 // Resampling, in its own module (modules[8]).
 add("RESAMPLE",c::orange.interpolatedWith(c::amber,.5f),{{"Resample",{P::ResampleSat},{P::ResampleSat}}},104,{P::Resample});
 {
  auto& b=resampleOn;
  b.setClickingTogglesState(true);
  b.setTooltip("Switch resampling on or off (its settings stay)");
  b.setColour(juce::TextButton::textColourOnId,c::ink);
  b.setColour(juce::TextButton::textColourOffId,c::muted);
  b.onStateChange=[&b]{ const auto text=b.getToggleState()?"ON":"OFF"; if(b.getButtonText()!=text) b.setButtonText(text); };
  resampleOnAttach=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts,"RESAMPLE_ON",b);
  b.onStateChange();
  modules.back()->addHeaderComponent(b,44);
 }
 master=std::make_unique<soft::Master>(processor.apvts,specs()[P::Output],&specs()[P::Width],juce::String("WIDTH"),c::ink);
 body.addAndMakeVisible(*master);

 // The waveform lies over the SAMPLE screen; the load buttons sit in its header.
 sample.addAndMakeVisible(view);
 sample.addHeaderComponent(builtInButton,118);
 sample.addHeaderComponent(loadButton,104);
 loadButton.setTooltip("Choose a file (WAV, AIFF, FLAC, OGG, MP3)");
 builtInButton.setTooltip("Go back to the built-in ABISMO voice");
 loadButton.onClick=[this]{
  chooser=std::make_unique<juce::FileChooser>("Choose a sample",juce::File(),"*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
  chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
   [this](const juce::FileChooser& fc){ if(fc.getResult()!=juce::File()) load(fc.getResult()); });
 };
 builtInButton.onClick=[this]{ loadError.clear(); processor.useBuiltInSample(); };

 auto& m=modules;
 m[1]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const bool granular=juce::roundToInt(value(P::Mode))==1;
  const float pos=value(P::Position)*.01f,spray=value(P::Spray)*.01f,size=value(P::GrainSize),density=value(P::Density);
  caption(g,r,abismo_ui::formatValue(specs()[P::GrainSize],size),abismo_ui::formatValue(specs()[P::Density],density));
  auto plot=r.withTrimmedTop(14.f);
  if(!granular){ notice(g,plot,juce::String::fromUTF8("CLASSIC MODE  \xc2\xb7  SWITCH MODE ON THE SAMPLE SCREEN")); return; }
  juce::Random rnd(77);
  const int grains=juce::jlimit(6,90,juce::roundToInt(density*1.2f));
  const float drift=std::fmod(animation*.15f*value(P::Scan)*.01f,1.f);
  for(int i=0;i<grains;++i){
   const float x=juce::jlimit(0.f,1.f,pos+drift+(rnd.nextFloat()*2.f-1.f)*spray*.5f);
   const float y=rnd.nextFloat();
   const float rad=2.f+std::sqrt(size)*.35f*rnd.nextFloat();
   const bool back=rnd.nextFloat()<value(P::GrainReverse)*.01f;
   g.setColour((back?c::pink:c::cyan).withAlpha(.25f+.5f*rnd.nextFloat()));
   g.fillEllipse(juce::Rectangle<float>(rad*2.f,rad*2.f).withCentre({plot.getX()+x*plot.getWidth(),plot.getY()+y*plot.getHeight()}));
  }
  g.setColour(c::cyan);
  g.fillRect(plot.getX()+juce::jlimit(0.f,1.f,pos+drift)*plot.getWidth()-1.f,plot.getY(),2.f,plot.getHeight());
 };
 m[2]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   caption(g,area,"FILTER ENVELOPE",juce::String(juce::roundToInt(value(P::FilterEnv)))+"%");
   soft::glowStroke(g,adsr(area.withTrimmedTop(16.f).reduced(2.f,2.f),value(P::FAttack),value(P::FDecay),value(P::FSustain)*.01f,value(P::FRelease)),c::violet);
   return;
  }
  const int mode=juce::roundToInt(value(P::FilterMode));
  const float fc=value(P::Cutoff),k=2.f*(1.f-.97f*value(P::Resonance)*.01f);
  caption(g,area,abismo_ui::formatValue(specs()[P::Cutoff],fc),"DRIVE "+juce::String(juce::roundToInt(value(P::Drive)))+"%");
  auto plot=area.withTrimmedTop(14.f);
  soft::glowStroke(g,curve(plot,180,[&](float t){
   const float f=20.f*std::pow(1000.f,t),w=f/fc;
   const float re=1.f-w*w,im=k*w,den=std::sqrt(re*re+im*im);
   const float mag=mode==0?1.f/den:mode==1?(k*w)/den:(w*w)/den;
   return (juce::jlimit(-42.f,24.f,20.f*std::log10(std::max(1e-6f,mag)))+9.f)/33.f;
  }),c::violet);
 };
 m[3]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  soft::glowStroke(g,adsr(r.reduced(2.f,4.f),value(P::Attack),value(P::Decay),value(P::Sustain)*.01f,value(P::Release)),c::green);
 };
 m[4]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::VoiceMix)*.01f,vowel=value(P::Vowel)*.01f,shift=value(P::FormantShift);
  const char* letters[5]={"A","E","I","O","U"};
  auto row=r.withTrimmedTop(12.f);
  caption(g,r,"FORMANT "+juce::String(shift>0?"+":"")+juce::String(shift,1)+" st");
  for(int i=0;i<5;++i){
   const float x=row.getX()+row.getWidth()*(static_cast<float>(i)+.5f)/5.f;
   const float near=1.f-juce::jlimit(0.f,1.f,std::abs(vowel*4.f-static_cast<float>(i)));
   g.setFont(soft::uiFont(16.f+near*8.f,true));
   g.setColour(near>0.f && mix>0.f?c::pink.withAlpha(.35f+.65f*near):c::screenMuted.withAlpha(.6f));
   g.drawText(letters[i],juce::Rectangle<float>(40.f,row.getHeight()).withCentre({x,row.getCentreY()}),juce::Justification::centred);
  }
  if(mix<=0.f) notice(g,row.withTrimmedTop(row.getHeight()*.62f),"VOICE AT 0%");
 };
 m[5]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  if(page==1){
   const float rate=value(P::LfoRate);
   caption(g,r,abismo_ui::formatValue(specs()[P::LfoRate],rate));
   const float cycles=juce::jlimit(.5f,12.f,rate*2.f),phase=animation*rate;
   soft::glowStroke(g,curve(r.withTrimmedTop(12.f),160,[&](float t){return .8f*std::sin((t*cycles-phase)*6.2831853f);}),c::blue);
   return;
  }
  const float gl=value(P::PitchEnv),drift=value(P::Drift)*.01f;
  caption(g,r,juce::String::fromUTF8("GLISSANDO ")+(gl>0?"+":"")+juce::String(gl,1)+" st","DRIFT "+juce::String(juce::roundToInt(drift*100.f))+"%");
  const float timeShare=juce::jlimit(.1f,1.f,std::log10(1.f+value(P::PitchTime)/10.f)/3.4f);
  soft::glowStroke(g,curve(r.withTrimmedTop(12.f),160,[&](float t){
   return juce::jlimit(0.f,1.f,t/timeShare)*gl/24.f+drift*.12f*std::sin(t*23.f+std::sin(t*7.f)*2.f);
  }),c::blue);
 };
 m[8]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  soft::drawResampleScreen(g,r.withTrimmedBottom(34.f),inverso_abismo::Resampler::rateOf(juce::roundToInt(value(P::Resample))),
                           value(P::ResampleSat)*.01f,value(P::ResampleOn)>=.5f,c::orange.interpolatedWith(c::amber,.5f));
 };
 m[6]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const int div=juce::roundToInt(value(P::StutterDiv));
  const bool free=juce::roundToInt(value(P::StutterSync))==1;
  const float depth=value(P::StutterDepth)*.01f,chaos=value(P::Chaos)*.01f;
  if(div==0){ notice(g,area,"STUTTER OFF"); return; }
  static const char* divs[7]={"","1/4","1/8","1/16","1/32","1/8T","1/16T"};
  caption(g,area,free?abismo_ui::formatValue(specs()[P::StutterMs],value(P::StutterMs)):juce::String(divs[div]));
  auto grid=area.withTrimmedTop(16.f);
  const float w=grid.getWidth()/16.f;
  juce::Random rnd(1234);
  for(int i=0;i<16;++i){
   const float a=rnd.nextFloat();
   auto cell=juce::Rectangle<float>(grid.getX()+w*static_cast<float>(i),grid.getY(),w,grid.getHeight()).reduced(2.f,4.f);
   const bool dropped=a<chaos*.45f,repeated=!dropped && a<chaos*.9f;
   g.setColour(c::screenMuted.withAlpha(.18f));
   g.fillRoundedRectangle(cell,2.f);
   g.setColour(c::orange.withAlpha(dropped?.25f:repeated?.55f:.9f));
   g.fillRoundedRectangle(cell.withWidth(cell.getWidth()*(1.f-.85f*depth)),2.f);
  }
 };
 m[7]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::RevMix)*.01f,decay=value(P::RevDecay),dark=value(P::RevDamp)*.01f;
  const bool frozen=value(P::Freeze)>=.5f;
  caption(g,r,frozen?juce::String("FROZEN"):abismo_ui::formatValue(specs()[P::RevDecay],decay),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
  const auto colour=c::blue.interpolatedWith(c::violet,.5f).interpolatedWith(c::screenMuted,dark*.5f);
  const float span=juce::jlimit(.05f,1.f,std::log10(1.f+decay)/1.5f);
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),160,[&](float t){
   return (frozen?.7f:.8f*std::exp(-t/(span*.35f)))*2.f-1.f;
  }),colour.withAlpha(mix>0.f?1.f:.35f));
 };

 setResizable(true,true);
 soft::sizeWindow(*this,"SAMPLER",designW,designH,0.6f);
 refreshFromProcessor();
 startTimerHz(30);
}

InversoAbismoSamplerEditor::~InversoAbismoSamplerEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoSamplerEditor::load(const juce::File& file){
 if(!processor.loadSampleFile(file)) loadError=juce::String("COULD NOT READ: ")+file.getFileName();
 else loadError.clear();
 shownVersion=-1;
}

bool InversoAbismoSamplerEditor::isInterestedInFileDrag(const juce::StringArray& files){
 for(const auto& f:files) if(processor.isAudioFile(f)) return true;
 return false;
}
void InversoAbismoSamplerEditor::filesDropped(const juce::StringArray& files,int,int){
 dragging=false;repaint();
 for(const auto& f:files) if(processor.isAudioFile(f)){ load(juce::File(f)); break; }
}

// A control whose effect depends on another one is dimmed while it does nothing.
static bool samplerControlActive(const juce::AudioProcessorValueTreeState& apvts,int p){
 auto v=[&](int i){return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load();};
 const bool granular=juce::roundToInt(v(P::Mode))==1, loop=juce::roundToInt(v(P::LoopMode))>0;
 const bool stutter=juce::roundToInt(v(P::StutterDiv))>0, free=juce::roundToInt(v(P::StutterSync))==1;
 switch(p){
  case P::Start: case P::End: case P::LoopMode: return true; // granular: the region and how SCAN travels it
  case P::LoopStart: case P::LoopEnd: return loop;
  case P::Xfade: return !granular && loop;
  case P::Position: case P::Scan: case P::GrainSize: case P::Density: case P::Spray: case P::GrainPitch:
  case P::GrainReverse: case P::GrainSpread: case P::LfoPosition: return granular;
  case P::FAttack: case P::FDecay: case P::FSustain: case P::FRelease: return std::abs(v(P::FilterEnv))>1e-4f;
  case P::PitchTime: return std::abs(v(P::PitchEnv))>1e-4f;
  case P::Decay: return v(P::Sustain)<100.f;
  case P::Vowel: case P::FormantShift: return v(P::VoiceMix)>0.f;
  case P::LfoRate: return v(P::LfoPitch)>0.f || v(P::LfoFilter)>0.f || v(P::LfoAmp)>0.f || (granular && v(P::LfoPosition)>0.f);
  case P::StutterSync: return stutter;
  case P::StutterMs: return free && stutter;
  case P::StutterDepth: case P::Chaos: return stutter;
  case P::RevSize: case P::RevDamp: case P::Freeze: return v(P::RevMix)>0.f;
  case P::RevDecay: return v(P::RevMix)>0.f && v(P::Freeze)<.5f;
  default: return true;
 }
}

void InversoAbismoSamplerEditor::refreshFromProcessor(){
 animation+=1.f/30.f;
 presets.refresh();
 for(auto& m:modules){
  m->dimInactive([this](int p){return samplerControlActive(processor.apvts,p);});
  m->screen.repaint();
 }
 if(shownVersion!=processor.sampleVersion.load()){
  shownVersion=processor.sampleVersion.load();
  const auto missing=processor.getMissingPath();
  view.statusIsError=missing.isNotEmpty() || loadError.isNotEmpty();
  if(loadError.isNotEmpty()) view.status=loadError;
  else if(missing.isNotEmpty()) view.status=juce::String("FILE NOT FOUND: ")+missing+"  (playing the built-in voice)";
  else if(const auto s=processor.getDisplaySample())
   view.status=juce::String::fromUTF8(s->name.c_str())+"   "+juce::String(s->length()/s->sampleRate,2)+" s   "+
               juce::String(s->sampleRate/1000.,1)+" kHz   "+(s->right.empty()?"mono":"stereo");
  view.refresh();
 }
 view.repaint();
 master->setReadout(processor.outputPeak.load(),juce::String("VOICES ")+juce::String(processor.voicesSounding.load())+" / 16");
}

void InversoAbismoSamplerEditor::paintOverChildren(juce::Graphics& g){
 if(dragging){ g.setColour(c::amber); g.drawRect(getLocalBounds(),4); }
}

void InversoAbismoSamplerEditor::resized(){
 body.setBounds(0,0,designW,designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW));
 soft::rememberWindow(*this,"SAMPLER",designW);
 presets.setBounds(designW/2-210,14,420,72);
 const int x0=10,y0=88,mainW=1150,r1=350,r2=280,r3=280;
 auto place=[](juce::Component& comp,int x,int y,int w,int h){comp.setBounds(x,y,w,h);};
 place(*modules[0],x0,y0,640,r1);
 place(*modules[1],x0+640,y0,mainW-640,r1);
 place(*modules[2],x0,y0+r1,350,r2);
 place(*modules[3],x0+350,y0+r1,290,r2);
 place(*modules[4],x0+640,y0+r1,190,r2);
 place(*modules[5],x0+830,y0+r1,mainW-830,r2);
 place(*modules[6],x0,y0+r1+r2,380,r3);
 place(*modules[8],x0+380,y0+r1+r2,220,r3);
 place(*modules[7],x0+600,y0+r1+r2,mainW-600,r3);
 place(*master,x0+mainW,y0,designW-(x0+mainW)-10,r1+r2+r3);
 view.setBounds(modules[0]->screen.getBounds().withTrimmedBottom(34));
}
