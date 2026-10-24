#include "RelicarioEditor.h"
#include "DSP/RelicarioEngine.h"
#include <cmath>

using namespace inverso_abismo::relicario;
namespace c = soft::colour;

namespace {
// The window is designed at this size and scaled as a whole when resized.
constexpr int designW=1320, designH=1000;

const char* const* shortNames(){
 static const char* names[P::Count]={
  "LEVEL","LEVEL","LEVEL","LEVEL",
  "LOOP 1","LOOP 2","BALANCE","LOOP 2","DETUNE","LENGTH","WEAR","WOW","SATURATION","TONE",
  "ERA","BREATH","MOTOR","BELLOWS SAG","SPEAK",
  "SPARK","CLANG","DRIVE","BEAT",
  "16'","8'","4'","UPPER","INTERVAL","TONE","TREMOLO","RATE","SPEAKER","HUM",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY","OCTAVE","FINE","GLIDE","BEND",
  "CHORUS","MIX","TIME","REPEATS","DUST","MIX","SIZE","DECAY","WIDTH","OUTPUT",
  "TYPE","DRIVE","CRUSH","DOWNSAMPLE",
  "DIVISION","TIMING","TIME","GATE","CHAOS","REVERSE",
  "TYPE","CUTOFF","RESONANCE","ENVELOPE","ENV DECAY","LFO","LFO RATE",
  "RING","RING FREQ",
  "RATE","DEPTH",
  "MIX","RATE","DEPTH","FEEDBACK",
  "MIX","RATE","DEPTH","FEEDBACK",
  "DEPTH","RATE","SHAPE",
  "DEPTH","RATE",
  "SYNC","TONE","DARKNESS","AT KNOB","AGE",
  "LOOP 1","LOOP 2",
  "ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE",
  "ATTACK","DECAY","SUSTAIN","RELEASE","ATTACK","DECAY","SUSTAIN","RELEASE",
  "ON","ON","ON","ON","ON","ON","ON","ON","ON","ON","RESAMPLE","SATURATION"};
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
// A switched-off machine: its drawing stays faint behind a small label.
void notice(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& text){
 g.setFont(soft::uiFont(11.f,true));
 const float w=juce::GlyphArrangement::getStringWidth(g.getCurrentFont(),text)+24.f;
 g.setColour(c::screenBottom.withAlpha(.9f));
 g.fillRoundedRectangle(juce::Rectangle<float>(w,22.f).withCentre(r.getCentre()),11.f);
 g.setColour(c::screenMuted);
 g.drawText(text,r,juce::Justification::centred);
}
// The loop waveforms, as recorded on the tape (preview only).
float loopWave(int w,float t){
 t-=std::floor(t);
 switch(w){
  case 0: return 2.f*t-1.f;
  case 1: { const float u=t*2.f-std::floor(t*2.f); return .5f*(2.f*t-1.f)+.5f*(2.f*u-1.f); }
  case 2: return 1.f-4.f*std::abs(t-.5f);
  case 3: return t<.5f?1.f:-1.f;
  case 4: return std::sin(t*6.2831853f);
  default: return std::sin(t*91.f)*std::cos(t*217.f)*.9f;
 }
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
// The same flaw tables the engine lays on its two loops.
const TapeLoop& tape(int i){
 static const auto loops=[]{ std::array<TapeLoop,2> l; l[0].build(0x51A7u); l[1].build(0xE40u); return l; }();
 return loops[static_cast<std::size_t>(i)];
}
}

float InversoRelicarioEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoRelicarioEditor::InversoRelicarioEditor(InversoRelicarioProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 body.title="RELICARIO";
 body.subtitle=juce::String::fromUTF8("FOUR FORGOTTEN MACHINES   \xc2\xb7   MOD WHEEL: TAPE WEAR + ORGAN TREMOLO\nAFTERTOUCH: BELLOWS + SPARK   \xc2\xb7   RIGHT-CLICK ANY KNOB: MIDI LEARN   \xc2\xb7   KEYS > AGE: HOW WORN AND ALIVE THE MACHINES ARE");
 addAndMakeVisible(body);
 presets.getCount=[this]{return processor.getNumPrograms();};
 presets.getCurrent=[this]{return processor.getCurrentProgram();};
 presets.getName=[this](int i){return processor.getProgramName(i);};
 presets.select=[this](int i){processor.selectProgram(i);};
 presets.enableUserPresets(processor.apvts, "RELICARIO");
 body.addAndMakeVisible(presets);

 auto apvts=std::ref(processor.apvts);
 using Page=soft::Module::Page;
 auto add=[&](juce::String title,juce::Colour accent,std::vector<Page> pages,int screenH,std::vector<int> choices)->soft::Module&{
  modules.push_back(std::make_unique<soft::Module>(apvts.get(),specs().data(),shortNames(),title,accent,std::move(pages),screenH,std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 // ---- the four machines
 // Each machine's own envelope, as faders (the ENVELOPE module shapes the whole instrument).
 auto adsrPage=[](int m){ const int b=P::FitaAttack+4*m; return soft::Module::Page{"ADSR",{b,b+1,b+2,b+3},{},true}; };
 add("TAPE LOOPS",c::amber,{{"Loops",{P::FitaLevel,P::Loop1Level,P::Loop2Level,P::Loop2Detune},{P::FitaLevel}},
                            {"Tape",{P::Wear,P::Wow,P::TapeSat,P::FitaTone,P::LoopLength}},adsrPage(0)},112,{P::Loop1Wave,P::Loop2Wave,P::Loop2Octave});
 add("REED ORGAN",c::green,{{"Reeds",{P::HarmLevel,P::Era,P::Breath,P::Speak},{P::HarmLevel}},
                            {"Fan",{P::Motor,P::Sag}},adsrPage(1)},112,{});
 add("TUNING FORK",c::cyan,{{"Fork",{P::ForkLevel,P::Spark,P::Clang,P::Maintain,P::Beat},{P::ForkLevel}},adsrPage(2)},112,{});
 add("SOVIET ORGAN",c::red,{{"Stops",{P::OrganLevel,P::Reg16,P::Reg8,P::Reg4,P::Upper},{P::OrganLevel}},
                            {"Tone",{P::OrganTone,P::TremDepth,P::TremRate,P::SpeakerBreak,P::Hum}},adsrPage(3)},112,{P::UpperInterval});
 // ---- keys and the effects rack, in signal order
 add("KEYS",c::violet,{{"Play",{P::VelSens,P::Glide,P::BendRange,P::Fine}},
                       {"Age",{P::Age},{P::Age}}},96,{P::Octave});
 add("DESTROY",c::blood,{{"Drive",{P::Drive,P::Resample,P::ResampleSat},{P::Drive}},
                         {"Ring",{P::RingMix,P::RingFreq}}},96,{P::DriveType});
 add("FILTER",c::orange,{{"Filter",{P::Cutoff,P::Resonance,P::FilterEnv,P::FilterDecay},{P::Cutoff}},
                         {"LFO",{P::FilterLfo,P::FilterLfoRate}}},96,{P::FilterMode});
 add("CHOP",c::pink,{{"Chop",{P::ChopMs,P::ChopDepth,P::ChopChaos,P::ChopReverse}}},96,{P::ChopSync,P::ChopDiv});
 add("MODULATION",c::blue,{{"Chorus",{P::Ensemble,P::ChorusRate,P::ChorusDepth},{P::Ensemble}},
                           {"Flanger",{P::FlangerMix,P::FlangerRate,P::FlangerDepth,P::FlangerFeedback},{P::FlangerMix}},
                           {"Phaser",{P::PhaserMix,P::PhaserRate,P::PhaserDepth,P::PhaserFeedback},{P::PhaserMix}},
                           {"Tremolo",{P::TremoloDepth,P::TremoloRate,P::TremoloShape},{P::TremoloDepth}},
                           {"Vibrato",{P::VibratoDepth,P::VibratoRate},{P::VibratoDepth}}},96,{});
 add("ECHO",c::amber.interpolatedWith(c::pink,.5f),{{"Tape echo",{P::EchoMix,P::EchoTime,P::EchoFeedback,P::EchoTone},{P::EchoMix}}},96,{P::EchoSync});
 add("SPACE",c::blue.interpolatedWith(c::violet,.5f),{{"Reverb",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::Dust},{P::RevMix}}},96,{});
 // The envelope of the whole instrument, on faders.
 add("ENVELOPE",c::violet.interpolatedWith(c::pink,.4f),{{"ADSR",{P::Attack,P::Decay,P::Sustain,P::Release},{},true}},96,{});
 master=std::make_unique<soft::Master>(processor.apvts,specs()[P::Output],&specs()[P::Width],juce::String("WIDTH"),c::ink);
 body.addAndMakeVisible(*master);

 // ON/OFF in each module's header, separate from its level.
 {
  const int onParams[10]={P::FitaOn,P::HarmOn,P::ForkOn,P::OrganOn,P::DestroyOn,P::FilterOn,P::ChopOn,P::ModOn,P::EchoOn,P::SpaceOn};
  const int onModules[10]={0,1,2,3,5,6,7,8,9,10};
  for(int i=0;i<10;++i){
   auto& b=onButtons[static_cast<std::size_t>(i)];
   b.setClickingTogglesState(true);
   b.setTooltip("Switch this module on or off (its settings stay)");
   b.setColour(juce::TextButton::textColourOnId,c::amber.brighter(.3f));
   b.setColour(juce::TextButton::textColourOffId,c::muted);
   b.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff1c1712));
   b.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff3a332c));
   b.onStateChange=[&b]{ const auto text=b.getToggleState()?"ON":"OFF"; if(b.getButtonText()!=text) b.setButtonText(text); };
   onAttach[static_cast<std::size_t>(i)]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts,specs()[static_cast<std::size_t>(onParams[i])].id,b);
   b.onStateChange();
   modules[static_cast<std::size_t>(onModules[i])]->addHeaderComponent(b,36);
  }
 }
 auto& m=modules;
 const juce::String off=juce::String::fromUTF8("SILENT  \xc2\xb7  SWITCHED OFF OR LEVEL AT ZERO");
 // TAPE: both loops running past the head, their flaws drawn along them.
 m[0]->screen.draw=[this,off](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float level=value(P::FitaLevel)*value(P::FitaOn),wear=value(P::Wear)*.01f;
  caption(g,area,"LOOP "+abismo_ui::formatValue(specs()[P::LoopLength],value(P::LoopLength)),"WEAR "+juce::String(juce::roundToInt(wear*100.f))+"%");
  auto lanes=area.withTrimmedTop(16.f);
  const float laneH=lanes.getHeight()/2.f;
  for(int i=0;i<2;++i){
   auto lane=juce::Rectangle<float>(lanes.getX(),lanes.getY()+laneH*static_cast<float>(i),lanes.getWidth(),laneH).reduced(0.f,3.f);
   const float weight=.5f*value(i==0?P::Loop1Level:P::Loop2Level)*.01f;
   const int wave=juce::roundToInt(value(i==0?P::Loop1Wave:P::Loop2Wave));
   const auto& t=tape(i);
   const auto colour=c::amber.withAlpha(level>0.f?.35f+.65f*std::min(1.f,weight*2.f):.3f);
   g.setColour(c::screenMuted.withAlpha(.12f));
   g.fillRoundedRectangle(lane,3.f);
   soft::glowStroke(g,curve(lane,220,[&](float x){
    const auto k=static_cast<std::size_t>(x*(TapeLoop::points-1));
    const float flaw=std::max(0.f,1.f+.55f*wear*t.amp[k]);
    return .75f*flaw*loopWave(wave,x*9.f);
   }),colour,1.4f);
   const float head=i==0?processor.loop1.load():processor.loop2.load();
   g.setColour(c::screenInk.withAlpha(.8f));
   g.fillRect(lane.getX()+head*lane.getWidth()-1.f,lane.getY(),2.f,lane.getHeight());
  }
  if(level<=0.f) notice(g,lanes,off);
 };
 // REED ORGAN: the bellows gauge and the reed's pulse.
 m[1]->screen.draw=[this,off](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float level=value(P::HarmLevel)*value(P::HarmOn),era=value(P::Era)*.01f;
  const float bellows=juce::jlimit(0.f,1.f,processor.bellows.load());
  caption(g,r,era<.5f?juce::String("BAKELITE"):juce::String("PLASTIC"),"BELLOWS "+juce::String(juce::roundToInt(bellows*100.f))+"%");
  auto area=r.withTrimmedTop(16.f);
  auto gauge=area.removeFromRight(28.f).reduced(6.f,2.f);
  g.setColour(c::screenMuted.withAlpha(.18f));
  g.fillRoundedRectangle(gauge,4.f);
  g.setColour((bellows<.35f?c::red:c::green).withAlpha(level>0.f?.9f:.3f));
  g.fillRoundedRectangle(gauge.withTrimmedTop(gauge.getHeight()*(1.f-bellows)),4.f);
  const float duty=.12f+.2f*era,smoothness=1.f-.6f*bellows;
  soft::glowStroke(g,curve(area.reduced(2.f,8.f),220,[&](float t){
   const float ph=t*4.f-std::floor(t*4.f);
   const float pulse=ph<duty?1.f:-duty/(1.f-duty);
   return .8f*bellows*(smoothness*std::sin(ph*6.2831853f)+(1.f-smoothness)*pulse);
  }),c::green.withAlpha(level>0.f?1.f:.3f));
  if(level<=0.f) notice(g,area,off);
 };
 // TUNING FORK: two tines swinging, sparks at the contact.
 m[2]->screen.draw=[this,off](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float level=value(P::ForkLevel)*value(P::ForkOn),spark=value(P::Spark)*.01f,drive=value(P::Maintain)*.01f;
  caption(g,r,"BEAT "+abismo_ui::formatValue(specs()[P::Beat],value(P::Beat)),"DRIVE "+juce::String(juce::roundToInt(drive*100.f))+"%");
  auto area=r.withTrimmedTop(16.f).reduced(4.f);
  const auto colour=c::cyan.withAlpha(level>0.f?1.f:.3f);
  const float swing=level>0.f?std::sin(animation*31.f)*(2.f+6.f*drive):0.f;
  for(int f=0;f<2;++f){
   const float cx=area.getX()+area.getWidth()*(f==0?.33f:.67f);
   const float gap=12.f, top=area.getY()+2.f, base=area.getBottom()-24.f;
   const float s=f==0?swing:swing*std::cos(animation*value(P::Beat)*6.2831853f);
   juce::Path fork;
   fork.startNewSubPath(cx-gap-s,top);
   fork.quadraticTo(cx-gap,base,cx,base+6.f);
   fork.quadraticTo(cx+gap,base,cx+gap+s,top);
   fork.startNewSubPath(cx,base+6.f);
   fork.lineTo(cx,area.getBottom());
   soft::glowStroke(g,fork,colour,2.f);
   if(level>0.f && spark>0.f){
    juce::Random rnd(static_cast<juce::int64>(animation*30.f)+f*77);
    for(int k=0;k<juce::roundToInt(spark*8.f);++k){
     g.setColour(c::screenInk.withAlpha(.4f+.6f*rnd.nextFloat()));
     g.fillEllipse(cx+gap+s+rnd.nextFloat()*10.f-3.f,top+rnd.nextFloat()*18.f,2.f,2.f);
    }
   }
  }
  if(level<=0.f) notice(g,area,off);
 };
 // SOVIET ORGAN: the stops as bars.
 m[3]->screen.draw=[this,off](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float level=value(P::OrganLevel)*value(P::OrganOn);
  caption(g,area,value(P::SpeakerBreak)>0.f?juce::String("TORN SPEAKER"):juce::String("STOPS"),"TREMOLO "+abismo_ui::formatValue(specs()[P::TremRate],value(P::TremRate)));
  auto bars=area.withTrimmedTop(16.f);
  const int regs[4]={P::Reg16,P::Reg8,P::Reg4,P::Upper};
  const char* labels[4]={"16'","8'","4'","UP"};
  const float w=bars.getWidth()/4.f;
  const float trem=1.f-.25f*value(P::TremDepth)*.01f*(.5f+.5f*std::sin(animation*value(P::TremRate)*6.2831853f));
  for(int i=0;i<4;++i){
   auto col=juce::Rectangle<float>(bars.getX()+w*static_cast<float>(i),bars.getY(),w,bars.getHeight()).reduced(8.f,0.f);
   auto label=col.removeFromBottom(13.f);
   g.setFont(soft::monoFont(10.f));
   g.setColour(c::screenMuted);
   g.drawText(labels[i],label,juce::Justification::centred);
   g.setColour(c::screenMuted.withAlpha(.18f));
   g.fillRoundedRectangle(col,3.f);
   const float v=value(regs[i])*.01f*(level>0.f?trem:1.f);
   g.setColour(c::red.withAlpha(level>0.f?.9f:.3f));
   g.fillRoundedRectangle(col.withTrimmedTop(col.getHeight()*(1.f-v)),3.f);
  }
  if(level<=0.f) notice(g,bars,off);
 };
 // KEYS
 m[4]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   // AGE: twelve keys, each a little off and drifting, more so as the machines get older.
   const float a=value(P::Age)*.01f;
   caption(g,area,"AGE "+juce::String(juce::roundToInt(a*100.f))+"%",a<.05f?juce::String("FACTORY NEW"):a<.5f?juce::String("WELL KEPT"):a<.85f?juce::String("LIVED IN"):juce::String("WORN OUT"));
   auto plot=area.withTrimmedTop(16.f).reduced(2.f,4.f);
   g.setColour(c::screenMuted.withAlpha(.3f));
   g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
   juce::Random rnd(77);
   for(int k=0;k<12;++k){
    const float x=plot.getX()+plot.getWidth()*(static_cast<float>(k)+.5f)/12.f;
    const float off=rnd.nextFloat()*2.f-1.f,ph=rnd.nextFloat()*6.28f;
    const float y=plot.getCentreY()-a*plot.getHeight()*.42f*(.55f*off+.45f*std::sin(animation*(.6f+.5f*rnd.nextFloat())+ph));
    juce::Path tick; tick.startNewSubPath(x,plot.getCentreY()); tick.lineTo(x,y);
    soft::glowStroke(g,tick,c::violet,2.f);
   }
   return;
  }
  // how hard you play against how loud it sounds
  const float sens=value(P::VelSens)*.01f;
  caption(g,area,"VELOCITY "+juce::String(juce::roundToInt(sens*100.f))+"%");
  soft::glowStroke(g,curve(area.withTrimmedTop(14.f).reduced(2.f,4.f),80,[&](float t){ return 2.f*std::pow(1.f-sens+sens*t,1.3f)-1.f; }),c::violet);
 };
 modules[11]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  caption(g,r,juce::String::fromUTF8("MASTER OVER ALL MACHINES  \xc2\xb7  OPEN = EACH MACHINE'S OWN ADSR"));
  soft::glowStroke(g,adsr(r.withTrimmedTop(14.f).reduced(2.f,4.f),value(P::Attack),value(P::Decay),value(P::Sustain)*.01f,value(P::Release)),c::violet);
 };
 // DESTROY: the drive curve, or the ring carrier.
 m[5]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   const float mix=value(P::RingMix)*.01f,hz=value(P::RingFreq);
   caption(g,area,"RING "+abismo_ui::formatValue(specs()[P::RingFreq],hz),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
   const float cycles=juce::jlimit(1.f,24.f,hz/60.f);
   soft::glowStroke(g,curve(area.withTrimmedTop(14.f),220,[&](float t){
    const float x=std::sin(t*6.2831853f*2.f);
    return .8f*(x*(1.f-mix)+x*std::sin(t*6.2831853f*cycles)*mix);
   }),c::blood.brighter(.3f).withAlpha(mix>0.f?1.f:.35f));
   return;
  }
  const float drive=value(P::Drive)*.01f;
  const int type=juce::roundToInt(value(P::DriveType));
  caption(g,area,"DRIVE "+juce::String(juce::roundToInt(drive*100.f))+"%",
          "RESAMPLE "+(juce::roundToInt(value(P::Resample))==0?juce::String("OFF"):juce::String(inverso_abismo::Resampler::rateOf(juce::roundToInt(value(P::Resample))),0)+" Hz"));
  inverso_abismo::relicario::Drive preview;
  // RESAMPLE: a waveform that loses its edges as the rate drops
  const int rateChoice=juce::roundToInt(value(P::Resample));
  const float rate=inverso_abismo::Resampler::rateOf(rateChoice);
  const float keep=rateChoice==0?1.f:juce::jlimit(.05f,1.f,rate/20000.f);
  soft::glowStroke(g,curve(area.withTrimmedTop(14.f),160,[&](float t){
   // a square-ish wave: its corners round off as the top end goes
   float x=0.f;
   for(int h=1;h<=15;h+=2) if(h*1000.f<keep*20000.f*.45f+1000.f) x+=std::sin(t*6.2831853f*h)/static_cast<float>(h);
   x*=.6f;
   float y=drive>0.f?preview.process(x,type==4?0:type,drive):x;
   return .85f*juce::jlimit(-1.f,1.f,y);
  }),c::blood.brighter(.3f).withAlpha(drive>0.f || rateChoice>0?1.f:.35f));
 };
 // FILTER: the response, and where the envelope takes it.
 m[6]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  const int mode=juce::roundToInt(value(P::FilterMode));
  if(page==1){
   const float rate=value(P::FilterLfoRate),depth=value(P::FilterLfo)*.01f;
   caption(g,area,"LFO "+abismo_ui::formatValue(specs()[P::FilterLfoRate],rate),"DEPTH "+juce::String(juce::roundToInt(depth*100.f))+"%");
   const float cycles=juce::jlimit(.5f,12.f,rate*2.f);
   soft::glowStroke(g,curve(area.withTrimmedTop(14.f),160,[&](float t){return .8f*depth*std::sin((t*cycles-animation*rate)*6.2831853f);}),c::orange.withAlpha(mode>0?1.f:.35f));
   return;
  }
  const float fc=value(P::Cutoff),k=2.f-1.95f*value(P::Resonance)*.01f,env=value(P::FilterEnv)*.01f;
  caption(g,area,abismo_ui::formatValue(specs()[P::Cutoff],fc),mode==0?juce::String("FILTER OFF"):"ENV "+juce::String(juce::roundToInt(env*100.f))+"%");
  auto plot=area.withTrimmedTop(14.f);
  auto response=[&](float cutoff){
   return curve(plot,180,[&,cutoff](float t){
    if(mode==0) return .3f;
    const float f=20.f*std::pow(1000.f,t);
    const float w=f/cutoff;
    const float re=1.f-w*w,im=k*w;
    const float den=std::sqrt(re*re+im*im);
    const float mag=mode==1?1.f/den:mode==2?(k*w)/den:(w*w)/den;
    const float db=juce::jlimit(-42.f,24.f,20.f*std::log10(std::max(1e-6f,mag)));
    return (db+9.f)/33.f;
   });
  };
  if(mode>0 && std::abs(env)>.001f){
   g.setColour(c::orange.withAlpha(.35f));
   g.strokePath(response(juce::jlimit(20.f,20000.f,fc*std::exp2(env*6.f))),juce::PathStrokeType(1.f));
  }
  soft::glowStroke(g,response(fc),c::orange.withAlpha(mode>0?1.f:.35f));
 };
 // CHOP: the step grid, gated, dropped, repeated, reversed.
 m[7]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const int div=juce::roundToInt(value(P::ChopDiv));
  const bool free=juce::roundToInt(value(P::ChopSync))==1;
  const float depth=value(P::ChopDepth)*.01f,chaos=value(P::ChopChaos)*.01f,rev=value(P::ChopReverse)*.01f;
  if(div==0 && rev<=0.f){ notice(g,area,"CHOP OFF"); return; }
  static const char* divs[7]={"","1/4","1/8","1/16","1/32","1/8T","1/16T"};
  caption(g,area,free?abismo_ui::formatValue(specs()[P::ChopMs],value(P::ChopMs)):juce::String(div>0?divs[div]:"1/8"),rev>0.f?juce::String("REVERSE"):juce::String());
  auto grid=area.withTrimmedTop(16.f);
  const int steps=16;
  const float w=grid.getWidth()/static_cast<float>(steps);
  juce::Random rnd(1234);
  for(int i=0;i<steps;++i){
   const float a=rnd.nextFloat(),b=rnd.nextFloat();
   auto cell=juce::Rectangle<float>(grid.getX()+w*static_cast<float>(i),grid.getY(),w,grid.getHeight()).reduced(2.f,4.f);
   const bool reversed=b<rev, dropped=div>0 && a<chaos*.45f, repeated=div>0 && !dropped && a<chaos*.9f;
   g.setColour(c::screenMuted.withAlpha(.18f));
   g.fillRoundedRectangle(cell,2.f);
   auto lit=cell.withWidth(cell.getWidth()*(div>0?1.f-.85f*depth:1.f));
   g.setColour((reversed?c::amber:c::pink).withAlpha(dropped?.25f:repeated?.55f:.9f));
   g.fillRoundedRectangle(lit,2.f);
  }
 };
 // MODULATION: the LFO of the page shown, moving at its rate.
 m[8]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  struct Lfo { int mix, rate; const char* name; };
  static const Lfo lfos[5]={{P::Ensemble,P::ChorusRate,"CHORUS"},{P::FlangerMix,P::FlangerRate,"FLANGER"},{P::PhaserMix,P::PhaserRate,"PHASER"},
                            {P::TremoloDepth,P::TremoloRate,"TREMOLO"},{P::VibratoDepth,P::VibratoRate,"VIBRATO"}};
  const auto& l=lfos[juce::jlimit(0,4,page)];
  const float mix=value(l.mix),rate=value(l.rate);
  caption(g,r,juce::String(l.name)+" "+abismo_ui::formatValue(specs()[l.rate],rate),mix>0.f?juce::String("ON"):juce::String("OFF"));
  const bool square=page==3 && juce::roundToInt(value(P::TremoloShape))==1;
  const float cycles=juce::jlimit(.5f,12.f,rate*1.5f);
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),200,[&](float t){
   const float s=std::sin((t*cycles-animation*rate)*6.2831853f);
   return .75f*(square?std::tanh(s*6.f):s);
  }),c::blue.withAlpha(mix>0.f?1.f:.35f));
  if(page==3 && juce::roundToInt(value(P::TremoloShape))==2){
   soft::glowStroke(g,curve(r.withTrimmedTop(14.f),200,[&](float t){return -.75f*std::sin((t*cycles-animation*rate)*6.2831853f);}),c::pink.withAlpha(mix>0.f?.8f:.3f),1.2f);
  }
 };
 // ECHO: the repeats fading out.
 m[9]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float mix=value(P::EchoMix)*.01f,fb=value(P::EchoFeedback)*.01f;
  const int sync=juce::roundToInt(value(P::EchoSync));
  static const char* syncs[7]={"","1/4","1/8","1/8 DOTTED","1/16","1/4T","1/8T"};
  static const float beats[7]={0.f,1.f,.5f,.75f,.25f,2.f/3.f,1.f/3.f};
  const float time=sync>0?beats[sync]*500.f:value(P::EchoTime);
  caption(g,area,sync>0?juce::String(syncs[sync]):abismo_ui::formatValue(specs()[P::EchoTime],time),"REPEATS "+juce::String(juce::roundToInt(fb*100.f))+"%");
  auto plot=area.withTrimmedTop(16.f).reduced(2.f,2.f);
  float amp=1.f;
  const auto colour=c::amber.interpolatedWith(c::pink,.5f);
  for(int k=0;k<24;++k){
   const float x=plot.getX()+plot.getWidth()*juce::jmin(1.f,static_cast<float>(k)*time/2400.f);
   if(x>=plot.getRight()) break;
   const float h=plot.getHeight()*(k==0?1.f:amp*(.35f+.65f*mix));
   juce::Path tick; tick.startNewSubPath(x,plot.getBottom()); tick.lineTo(x,plot.getBottom()-h);
   soft::glowStroke(g,tick,colour.withAlpha(k==0?.5f:1.f),2.f);
   amp*=fb*.95f;
   if(k>0 && amp<.01f) break;
  }
  if(mix<=0.f) notice(g,plot,"ECHO OFF");
 };
 m[10]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::RevMix)*.01f,decay=value(P::RevDecay),dark=value(P::RevDamp)*.01f;
  caption(g,r,abismo_ui::formatValue(specs()[P::RevDecay],decay),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
  const float span=juce::jlimit(.05f,1.f,std::log10(1.f+decay)/1.4f);
  const auto colour=c::blue.interpolatedWith(c::violet,.5f).interpolatedWith(c::screenMuted,dark*.5f);
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),160,[&](float t){return 1.6f*std::exp(-t/(span*.35f))-1.f;}),colour.withAlpha(mix>0.f?1.f:.35f));
 };

 setResizable(true,true);
 setResizeLimits(designW*6/10,designH*6/10,designW*3/2,designH*3/2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW)/designH);
 setSize(designW*9/10,designH*9/10);
 timerCallback();
 startTimerHz(30);
}

InversoRelicarioEditor::~InversoRelicarioEditor(){ setLookAndFeel(nullptr); }

// A control whose machine or effect is switched off (or whose effect depends on another control) is dimmed.
static bool relicarioControlActive(const juce::AudioProcessorValueTreeState& apvts,int p){
 auto v=[&](int i){return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load();};
 const bool fita=v(P::FitaLevel)>0.f && v(P::FitaOn)>=.5f, harm=v(P::HarmLevel)>0.f && v(P::HarmOn)>=.5f;
 const bool fork=v(P::ForkLevel)>0.f && v(P::ForkOn)>=.5f, organ=v(P::OrganLevel)>0.f && v(P::OrganOn)>=.5f;
 if(p>=P::FitaOn) return true; // the switches themselves
 if(p>=P::FitaAttack && p<=P::OrganRelease){
  const int machine=(p-P::FitaAttack)/4;
  const bool on=machine==0?fita:machine==1?harm:machine==2?fork:organ;
  const int stage=(p-P::FitaAttack)%4;
  if(stage==1) return on && v(P::FitaSustain+4*machine)<100.f; // decay only matters below full sustain
  return on;
 }
 if(p==P::Loop1Wave) return fita && v(P::Loop1Level)>0.f;
 if(p==P::Loop2Wave || p==P::Loop2Octave || p==P::Loop2Detune) return fita && v(P::Loop2Level)>0.f;
 if(p==P::Loop1Level || p==P::Loop2Level) return fita;
 if(p>=P::Loop1Wave && p<=P::FitaTone) return fita;
 if(p>=P::Era && p<=P::Speak) return harm;
 if(p>=P::Spark && p<=P::Beat) return fork;
 if(p>=P::Reg16 && p<=P::Hum) {
  if(!organ) return false;
  if(p==P::UpperInterval) return v(P::Upper)>0.f;
  if(p==P::TremRate) return v(P::TremDepth)>0.f;
  return true;
 }
 const bool chop=juce::roundToInt(v(P::ChopDiv))>0, grid=chop || v(P::ChopReverse)>0.f;
 const bool filter=juce::roundToInt(v(P::FilterMode))>0;
 switch(p){
  case P::Decay: return v(P::Sustain)<100.f;
  case P::DriveType: return v(P::Drive)>0.f;
  case P::RingFreq: return v(P::RingMix)>0.f;
  case P::ChopSync: return grid;
  case P::ChopMs: return grid && juce::roundToInt(v(P::ChopSync))==1;
  case P::ChopDepth: case P::ChopChaos: return chop;
  case P::Cutoff: case P::Resonance: case P::FilterEnv: case P::FilterLfo: return filter;
  case P::FilterDecay: return filter && std::abs(v(P::FilterEnv))>1e-4f;
  case P::FilterLfoRate: return filter && v(P::FilterLfo)>0.f;
  case P::ChorusRate: case P::ChorusDepth: return v(P::Ensemble)>0.f;
  case P::FlangerRate: case P::FlangerDepth: case P::FlangerFeedback: return v(P::FlangerMix)>0.f;
  case P::PhaserRate: case P::PhaserDepth: case P::PhaserFeedback: return v(P::PhaserMix)>0.f;
  case P::TremoloRate: case P::TremoloShape: return v(P::TremoloDepth)>0.f;
  case P::VibratoRate: return v(P::VibratoDepth)>0.f;
  case P::EchoTime: return v(P::EchoMix)>0.f && juce::roundToInt(v(P::EchoSync))==0;
  case P::EchoFeedback: case P::EchoTone: case P::EchoSync: return v(P::EchoMix)>0.f;
  case P::RevSize: case P::RevDecay: case P::RevDamp: return v(P::RevMix)>0.f;
  default: return true;
 }
}

void InversoRelicarioEditor::timerCallback(){
 animation+=1.f/30.f;
 presets.refresh();
 for(auto& m:modules){
  m->dimInactive([this](int p){return relicarioControlActive(processor.apvts,p);});
  m->screen.repaint();
 }
 master->setReadout(processor.outputPeak.load(),juce::String("VOICES ")+juce::String(processor.voicesSounding.load())+" / 12");
}

void InversoRelicarioEditor::resized(){
 body.setBounds(0,0,designW,designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW));
 presets.setBounds(designW/2-210,14,420,72);
 // Plates carry a 10 px shadow margin, so neighbours overlap by it.
 const int x0=10,y0=88,mainW=1150;
 auto place=[](juce::Component& comp,int x,int y,int w,int h){comp.setBounds(x,y,w,h);};
 const int r1=360,r2=272,r3=designH-y0-r1-r2-12;
 const int w4=mainW/4;
 for(int i=0;i<4;++i) place(*modules[static_cast<std::size_t>(i)],x0+w4*i,y0,i==3?mainW-3*w4:w4,r1);
 place(*modules[4],x0,y0+r1,200,r2);
 place(*modules[11],x0+200,y0+r1,235,r2);
 place(*modules[5],x0+435,y0+r1,255,r2);
 place(*modules[6],x0+690,y0+r1,245,r2);
 place(*modules[7],x0+935,y0+r1,mainW-935,r2);
 place(*modules[8],x0,y0+r1+r2,520,r3);
 place(*modules[9],x0+520,y0+r1+r2,340,r3);
 place(*modules[10],x0+860,y0+r1+r2,mainW-860,r3);
 place(*master,x0+mainW,y0,designW-(x0+mainW)-10,r1+r2+r3);
}
