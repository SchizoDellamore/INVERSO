#include "PluginEditor.h"
#include "DSP/AbismoEngine.h"
#include <cmath>

using namespace inverso_abismo;
namespace c = soft::colour;

namespace {
// The window is designed at this size and scaled as a whole when resized.
constexpr int designW=1320, designH=1000;

const char* const* shortNames(){
 static const char* names[P::Count]={
  "SHAPE","LEVEL","OCTAVE","UNISON","DETUNE","SPREAD","SUB",
  "CLUSTER","CLUSTER","BREATH","TENSION",
  "MATERIAL A","LEVEL","DECAY","BOW","SCRAPE","STRIKE","NOISE COLOUR",
  "TYPE","CUTOFF","RESONANCE","ENVELOPE","KEYTRACK","DRIVE",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY",
  "ATTACK","DECAY","SUSTAIN","RELEASE",
  "GLISSANDO","TIME","DRIFT","GLIDE","BEND",
  "RATE","PITCH","FILTER","VOLUME",
  "MIX","SIZE","DECAY","DARKNESS","FREEZE","WIDTH","OUTPUT",
  "VOICE","VOWEL","FORMANT",
  "BITS","RATE","DIVISION","DEPTH","CHAOS",
  "MATERIAL B","MORPH","TUNE","KEYTRACK","STRETCH","BRIGHTNESS","STEREO","OSC INPUT","RATTLE",
  "TYPE","RING","RING PITCH","REVERSE","REVERSE VERB","TAPE WARP",
  "TIMING","TIME","AT KNOB"};
 return names;
}

// ---- drawing helpers for the screens
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
float oscWave(float t,float shape){
 t-=std::floor(t);
 const float seg=shape*3.f;
 auto wave=[&](int w){
  switch(w){
   case 0: return std::sin(t*6.2831853f);
   case 1: return 1.f-4.f*std::abs(t-.5f);
   case 2: return 2.f*t-1.f;
   default: return t<.5f?1.f:-1.f;
  }
 };
 const int a=std::clamp(static_cast<int>(seg),0,2);
 const float frac=std::clamp(seg-static_cast<float>(a),0.f,1.f);
 return wave(a)+frac*(wave(a+1)-wave(a));
}
// Same curves as the engine's drive types (screen preview).
float driveCurve(float x,int mode,float drive){
 const float pre=1.f+drive*6.f,post=1.f/(1.f+drive*2.f);
 switch(mode){
  case 1:{ float y=x*pre*1.5f; y=y>0.f?std::min(y,1.f):std::max(y,-.55f)*1.6f; return y*post*1.6f; }
  case 2: return std::sin(x*pre*1.3f)*(.55f+.45f*post);
  case 3: return std::tanh(std::abs(x)*pre)*1.6f*(.5f+post)-.6f;
  case 4:{ const float y=x*pre; const float a=std::min(1.f,std::abs(y)); return (y<0.f?-1.f:1.f)*std::sqrt(std::sqrt(a))*(.45f+.4f*post); }
  default: return std::tanh(x*pre)*post*1.8f;
 }
}
// Envelope outline: attack, decay to sustain, hold, release (times in ms, drawn on a log-ish scale).
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

float InversoAbismoEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoAbismoEditor::InversoAbismoEditor(InversoAbismoProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 body.title="ABISMO";
 body.subtitle=juce::String::fromUTF8("TENSION SYNTH FOR SCORING   \xc2\xb7   MOD WHEEL = TENSION   \xc2\xb7   AFTERTOUCH = BOW + FILTER");
 addAndMakeVisible(body);
 presets.getCount=[this]{return processor.getNumPrograms();};
 presets.getCurrent=[this]{return processor.getCurrentProgram();};
 presets.getName=[this](int i){return processor.getProgramName(i);};
 presets.select=[this](int i){processor.setCurrentProgram(i);};
 body.addAndMakeVisible(presets);

 auto apvts=std::ref(processor.apvts);
 using Page=soft::Module::Page;
 auto add=[&](juce::String title,juce::Colour accent,std::vector<Page> pages,int screenH,std::vector<int> choices)->soft::Module&{
  modules.push_back(std::make_unique<soft::Module>(apvts.get(),specs().data(),shortNames(),title,accent,std::move(pages),screenH,std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 // 0 SOURCE
 add("SOURCE",c::amber,{{"Osc",{P::OscShape,P::OscLevel,P::Detune,P::Spread,P::SubLevel},{P::OscShape}},
                        {"Mix",{P::ClusterLevel,P::NoiseLevel,P::Tension}}},126,{P::Octave,P::Unison,P::ClusterMode});
 // 1 RESONATOR
 add("RESONATOR",c::cyan,{{"Tone",{P::ResLevel,P::ResDecay,P::ResMorph,P::ResTune,P::ResKeytrack},{P::ResLevel}},
                           {"Excite",{P::Bow,P::Scrape,P::Strike,P::NoiseTone,P::ResInput}},
                           {"Body",{P::ResStretch,P::ResBright,P::ResStereo,P::ResRattle}}},126,{P::ResMaterial,P::ResMaterialB});
 // 2 FILTER
 add("FILTER",c::violet,{{"Filter",{P::Cutoff,P::Resonance,P::FilterEnv,P::Keytrack},{P::Cutoff}},
                         {"Envelope",{P::FAttack,P::FDecay,P::FSustain,P::FRelease}}},126,{P::FilterMode});
 // 3 ENVELOPE
 add("ENVELOPE",c::green,{{"Volume",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens}}},96,{});
 // 4 MOVEMENT
 add("MOVEMENT",c::blue,{{"Pitch",{P::PitchEnv,P::PitchTime,P::Drift,P::Glide,P::BendRange}},
                          {"LFO",{P::LfoRate,P::LfoPitch,P::LfoFilter,P::LfoAmp}}},96,{});
 // 5 VOICE
 add("VOICE",c::pink,{{"Voice",{P::VoiceMix,P::Vowel,P::FormantShift}}},96,{});
 // 6 DESTRUCTION
 add("DESTRUCTION",c::red,{{"Drive",{P::Drive,P::RingMix,P::RingPitch,P::Warp},{P::Drive}}},120,{P::DriveMode});
 // 7 MUTILATION
 add("MUTILATION",c::orange,{{"Chop",{P::StutterMs,P::StutterDepth,P::Chaos,P::Reverse}},
                                                              {"Digital",{P::Crush,P::Downsample}}},104,{P::StutterSync,P::StutterDiv});
 // 8 SPACE
 add("SPACE",c::blue.interpolatedWith(c::violet,.5f),
     {{"Reverb",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::RevReverse,P::Freeze},{P::RevMix}}},104,{});
 master=std::make_unique<soft::Master>(processor.apvts,specs()[P::Output],&specs()[P::Width],juce::String("WIDTH"),c::ink);
 body.addAndMakeVisible(*master);
 master->setAccessory(std::make_unique<soft::KnobLearn>(processor.apvts,specs()[P::PressureCC],processor.learnPressure,processor.learnedCC),48);

 auto& m=modules;
 // ---- screens
 m[0]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float shape=value(P::OscShape)*.01f,level=value(P::OscLevel)*.01f;
  const int unison=juce::roundToInt(value(P::Unison))+1;
  caption(g,area,juce::String("WAVE ")+juce::String(juce::roundToInt(shape*100.f))+"%",
          level>0.f?juce::String("SUB ")+juce::String(juce::roundToInt(value(P::SubLevel)))+"%":juce::String("OSC OFF"));
  auto wave=area.withTrimmedTop(14.f).reduced(0.f,4.f);
  const float det=value(P::Detune)*.01f;
  for(int u=unison-1;u>=1;--u){
   const float off=det*.08f*static_cast<float>(u);
   g.setColour(c::amber.withAlpha(.18f));
   g.strokePath(curve(wave,160,[&](float t){return .8f*std::max(.15f,level)*oscWave(t*2.f+off,shape);}),juce::PathStrokeType(1.2f));
  }
  soft::glowStroke(g,curve(wave,200,[&](float t){return .8f*std::max(.15f,level)*oscWave(t*2.f,shape);}),c::amber.withAlpha(level>0.f?1.f:.35f));
 };
 m[1]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float level=value(P::ResLevel);
  const float morph=value(P::ResMorph)*.01f,stretch=value(P::ResStretch)*.01f,bright=value(P::ResBright)*.01f;
  const auto& ra=materialRatios(juce::roundToInt(value(P::ResMaterial)));
  const auto& rb=materialRatios(juce::roundToInt(value(P::ResMaterialB)));
  caption(g,area,"PARTIALS",juce::String("DECAY ")+juce::String(value(P::ResDecay),1)+" s");
  auto plot=area.withTrimmedTop(16.f);
  g.setColour(c::screenMuted.withAlpha(.25f));
  g.drawHorizontalLine(juce::roundToInt(plot.getBottom()),plot.getX(),plot.getRight());
  auto xOf=[&](float ratio){return plot.getX()+plot.getWidth()*juce::jlimit(0.f,1.f,(std::log2(ratio)+2.2f)/9.f);};
  const float tilt=.1f+.8f*(1.f-bright);
  for(int i=0;i<numModes;++i){
   const auto k=static_cast<std::size_t>(i);
   float ratio=std::exp((1.f-morph)*std::log(ra[k])+morph*std::log(rb[k]));
   ratio=std::pow(ratio,1.f+.3f*stretch);
   const float h=plot.getHeight()*.95f/(1.f+tilt*static_cast<float>(i));
   const float x=xOf(ratio);
   if(morph>0.f && morph<1.f){
    g.setColour(c::screenMuted.withAlpha(.35f));
    g.drawVerticalLine(juce::roundToInt(xOf(rb[k])),plot.getBottom()-h*.6f,plot.getBottom());
   }
   juce::Path bar;
   bar.startNewSubPath(x,plot.getBottom());
   bar.lineTo(x,plot.getBottom()-h);
   soft::glowStroke(g,bar,c::cyan.withAlpha(level>0.f?1.f:.3f),2.f);
  }
  if(level<=0.f) notice(g,plot.withTrimmedBottom(plot.getHeight()*.4f),juce::String::fromUTF8("OFF  \xc2\xb7  TURN UP THE LEVEL"));
 };
 m[2]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   caption(g,area,"FILTER ENVELOPE",juce::String(juce::roundToInt(value(P::FilterEnv)))+"%");
   soft::glowStroke(g,adsr(area.withTrimmedTop(16.f).reduced(2.f,2.f),value(P::FAttack),value(P::FDecay),value(P::FSustain)*.01f,value(P::FRelease)),c::violet);
   return;
  }
  const int mode=juce::roundToInt(value(P::FilterMode));
  const float fc=value(P::Cutoff),k=2.f*(1.f-.97f*value(P::Resonance)*.01f),env=value(P::FilterEnv)*.01f;
  caption(g,area,abismo_ui::formatValue(specs()[P::Cutoff],fc),"ENV "+juce::String(juce::roundToInt(env*100.f))+"%");
  auto plot=area.withTrimmedTop(14.f);
  auto response=[&](float cutoff){
   return curve(plot,180,[&,cutoff](float t){
    const float f=20.f*std::pow(1000.f,t);
    const float w=f/cutoff;
    const float re=1.f-w*w,im=k*w;
    const float den=std::sqrt(re*re+im*im);
    const float mag=mode==0?1.f/den:mode==1?(k*w)/den:(w*w)/den;
    const float db=juce::jlimit(-42.f,24.f,20.f*std::log10(std::max(1e-6f,mag)));
    return (db+9.f)/33.f;
   });
  };
  if(std::abs(env)>.001f){
   g.setColour(c::violet.withAlpha(.35f));
   g.strokePath(response(juce::jlimit(20.f,20000.f,fc*std::exp2(env*5.f))),juce::PathStrokeType(1.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
  }
  soft::glowStroke(g,response(fc),c::violet);
 };
 m[3]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  soft::glowStroke(g,adsr(r.reduced(2.f,4.f),value(P::Attack),value(P::Decay),value(P::Sustain)*.01f,value(P::Release)),c::green);
 };
 m[4]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  if(page==1){
   const float rate=value(P::LfoRate);
   caption(g,r,abismo_ui::formatValue(specs()[P::LfoRate],rate));
   const float cycles=juce::jlimit(.5f,12.f,rate*2.f);
   const float phase=animation*rate;
   soft::glowStroke(g,curve(r.withTrimmedTop(12.f),160,[&](float t){return .8f*std::sin((t*cycles-phase)*6.2831853f);}),c::blue);
   return;
  }
  const float gl=value(P::PitchEnv),drift=value(P::Drift)*.01f;
  caption(g,r,juce::String::fromUTF8("GLISSANDO ")+(gl>0?"+":"")+juce::String(gl,1)+" st","DRIFT "+juce::String(juce::roundToInt(drift*100.f))+"%");
  const float timeShare=juce::jlimit(.1f,1.f,std::log10(1.f+value(P::PitchTime)/10.f)/3.4f);
  soft::glowStroke(g,curve(r.withTrimmedTop(12.f),160,[&](float t){
   const float g1=juce::jlimit(0.f,1.f,t/timeShare)*gl/24.f;
   return g1+drift*.12f*std::sin(t*23.f+std::sin(t*7.f)*2.f);
  }),c::blue);
 };
 m[5]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
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
  if(mix<=0.f) notice(g,row.withTrimmedTop(row.getHeight()*.62f),juce::String("VOICE AT 0%"));
 };
 m[6]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  r=r.withTrimmedBottom(34.f);
  auto area=r.withTrimmedRight(r.getWidth()*.42f);
  const float drive=value(P::Drive)*.01f;
  const int mode=juce::roundToInt(value(P::DriveMode));
  g.setColour(c::screenMuted.withAlpha(.25f));
  g.drawHorizontalLine(juce::roundToInt(area.getCentreY()),area.getX(),area.getRight());
  soft::glowStroke(g,curve(area,120,[&](float t){const float x=t*2.f-1.f;return drive>0.f?.9f*driveCurve(x,mode,drive):x*.9f;}),c::red.withAlpha(drive>0.f?1.f:.35f));
  auto info=r.withTrimmedLeft(r.getWidth()*.62f);
  g.setFont(soft::monoFont(10.f));
  g.setColour(value(P::RingMix)>0.f?c::screenInk:c::screenMuted);
  g.drawText("RING "+juce::String(juce::roundToInt(value(P::RingMix)))+"%",info.removeFromTop(16.f),juce::Justification::centredLeft);
  g.setColour(value(P::Warp)>0.f?c::screenInk:c::screenMuted);
  g.drawText("WARP "+juce::String(juce::roundToInt(value(P::Warp)))+"%",info.removeFromTop(16.f),juce::Justification::centredLeft);
 };
 m[7]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   const float crush=value(P::Crush)*.01f,ds=value(P::Downsample)*.01f;
   const float levels=std::exp2(9.f-crush*7.f),hold=1.f+ds*ds*30.f;
   soft::glowStroke(g,curve(area,240,[&](float t){
    const float tt=std::floor(t*240.f/hold)*hold/240.f;
    return .8f*std::round(std::sin(tt*6.2831853f*2.f)*levels)/levels;
   }),c::orange);
   return;
  }
  const int div=juce::roundToInt(value(P::StutterDiv));
  const bool free=juce::roundToInt(value(P::StutterSync))==1;
  const float depth=value(P::StutterDepth)*.01f,chaos=value(P::Chaos)*.01f,rev=value(P::Reverse)*.01f;
  if(div==0 && rev<=0.f){ notice(g,area,"STUTTER OFF"); return; }
  static const char* divs[7]={"","1/4","1/8","1/16","1/32","1/8T","1/16T"};
  caption(g,area,free?abismo_ui::formatValue(specs()[P::StutterMs],value(P::StutterMs)):juce::String(div>0?divs[div]:"1/8"),
          juce::String::fromUTF8(rev>0.f?"\xe2\x97\x80 REVERSE":""));
  auto grid=area.withTrimmedTop(16.f);
  const int steps=16;
  const float w=grid.getWidth()/static_cast<float>(steps);
  juce::Random rnd(1234);
  for(int i=0;i<steps;++i){
   const float a=rnd.nextFloat(),b=rnd.nextFloat();
   auto cell=juce::Rectangle<float>(grid.getX()+w*static_cast<float>(i),grid.getY(),w,grid.getHeight()).reduced(2.f,4.f);
   const bool reversed=b<rev, dropped=div>0 && a<chaos*.45f, repeated=div>0 && !dropped && a<chaos*.9f;
   const float open=div>0?1.f-.85f*depth:1.f;
   g.setColour(c::screenMuted.withAlpha(.18f));
   g.fillRoundedRectangle(cell,2.f);
   auto lit=cell.withWidth(cell.getWidth()*open);
   g.setColour((reversed?c::pink:c::orange).withAlpha(dropped?.25f:repeated?.55f:.9f));
   g.fillRoundedRectangle(lit,2.f);
   if(reversed){
    juce::Path tri;
    const auto ct=lit.getCentre();
    tri.addTriangle(ct.x+3.f,ct.y-4.f,ct.x+3.f,ct.y+4.f,ct.x-4.f,ct.y);
    g.setColour(c::screenTop);
    g.fillPath(tri);
   }
  }
 };
 m[8]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::RevMix)*.01f,decay=value(P::RevDecay),dark=value(P::RevDamp)*.01f,rev=value(P::RevReverse)*.01f;
  const bool frozen=value(P::Freeze)>=.5f;
  caption(g,r,frozen?juce::String("FROZEN"):abismo_ui::formatValue(specs()[P::RevDecay],decay),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
  auto plot=r.withTrimmedTop(14.f);
  const auto colour=c::blue.interpolatedWith(c::violet,.5f).interpolatedWith(c::screenMuted,dark*.5f);
  const float span=juce::jlimit(.05f,1.f,std::log10(1.f+decay)/1.5f);
  auto tail=[&](float t){
   if(frozen) return .7f;
   const float env=std::exp(-t/(span*.35f));
   const float swell=std::exp(-(1.f-std::min(t/span,1.f))*4.f)*(t<span?1.f:0.f);
   return .8f*((1.f-rev)*env+rev*swell);
  };
  juce::Path fill=curve(plot,160,[&](float t){return tail(t)*2.f-1.f;});
  soft::glowStroke(g,fill,colour.withAlpha(mix>0.f?1.f:.35f));
 };

 setResizable(true,true);
 setResizeLimits(designW*6/10,designH*6/10,designW*3/2,designH*3/2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW)/designH);
 setSize(designW*9/10,designH*9/10); // opens a little smaller; drag the corner to scale
 timerCallback();
 startTimerHz(30);
}

InversoAbismoEditor::~InversoAbismoEditor(){ setLookAndFeel(nullptr); }

// A control whose effect depends on another one is dimmed while it does nothing.
static bool synthControlActive(const juce::AudioProcessorValueTreeState& apvts,int p){
 auto v=[&](int i){return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load();};
 const bool res=v(P::ResLevel)>0.f, osc=v(P::OscLevel)>0.f;
 const bool stutter=juce::roundToInt(v(P::StutterDiv))>0, grid=stutter || v(P::Reverse)>0.f;
 const bool free=juce::roundToInt(v(P::StutterSync))==1;
 switch(p){
  case P::OscShape: case P::Unison: return osc;
  case P::Detune: case P::Spread: return osc && juce::roundToInt(v(P::Unison))>0;
  case P::ClusterLevel: return juce::roundToInt(v(P::ClusterMode))>0;
  case P::ResMaterial: case P::ResMorph: case P::ResDecay: case P::ResTune: case P::ResKeytrack: case P::Bow: case P::Scrape:
  case P::Strike: case P::NoiseTone: case P::ResStretch: case P::ResBright: case P::ResStereo: case P::ResRattle: return res;
  case P::ResMaterialB: return res && v(P::ResMorph)>0.f;
  case P::ResInput: return res && osc;
  case P::DriveMode: return v(P::Drive)>0.f;
  case P::RingPitch: return v(P::RingMix)>0.f;
  case P::FAttack: case P::FDecay: case P::FSustain: case P::FRelease: return std::abs(v(P::FilterEnv))>1e-4f;
  case P::PitchTime: return std::abs(v(P::PitchEnv))>1e-4f;
  case P::Decay: return v(P::Sustain)<100.f;
  case P::Vowel: case P::FormantShift: return v(P::VoiceMix)>0.f;
  case P::LfoRate: return v(P::LfoPitch)>0.f || v(P::LfoFilter)>0.f || v(P::LfoAmp)>0.f;
  case P::StutterSync: return grid;
  case P::StutterMs: return free && grid;
  case P::StutterDepth: case P::Chaos: return stutter;
  case P::RevSize: case P::RevDamp: case P::RevReverse: case P::Freeze: return v(P::RevMix)>0.f;
  case P::RevDecay: return v(P::RevMix)>0.f && v(P::Freeze)<.5f;
  default: return true;
 }
}

void InversoAbismoEditor::timerCallback(){
 animation+=1.f/30.f;
 presets.refresh();
 for(auto& m:modules){
  m->dimInactive([this](int p){return synthControlActive(processor.apvts,p);});
  m->screen.repaint();
 }
 master->setReadout(processor.outputPeak.load(),juce::String("VOICES ")+juce::String(processor.voicesSounding.load())+" / 16");
}

void InversoAbismoEditor::resized(){
 body.setBounds(0,0,designW,designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW));
 presets.setBounds(designW/2-210,14,420,72);
 // Cards carry a 10 px shadow margin, so neighbours overlap by it.
 const int x0=10,y0=88,mainW=1150;
 auto place=[](juce::Component& comp,int x,int y,int w,int h){comp.setBounds(x,y,w,h);};
 const int r1=318,r2=290,r3=290;
 place(*modules[0],x0,y0,350,r1);
 place(*modules[1],x0+350,y0,450,r1);
 place(*modules[2],x0+800,y0,mainW-800,r1);
 place(*modules[3],x0,y0+r1,320,r2);
 place(*modules[4],x0+320,y0+r1,330,r2);
 place(*modules[5],x0+650,y0+r1,200,r2);
 place(*modules[6],x0+850,y0+r1,mainW-850,r2);
 place(*modules[7],x0,y0+r1+r2,560,r3);
 place(*modules[8],x0+560,y0+r1+r2,mainW-560,r3);
 place(*master,x0+mainW,y0,designW-(x0+mainW)-10,r1+r2+r3);
}
