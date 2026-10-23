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
  "N\xc3\x8dVEL","N\xc3\x8dVEL","N\xc3\x8dVEL","N\xc3\x8dVEL",
  "LOOP 1","LOOP 2","BALAN\xc3\x87O","OITAVA 2","DESAFINA 2","COMPRIMENTO","DESGASTE","WOW","SATURA\xc3\x87\xc3\x83O","TOM",
  "BAQUELITE","SOPRO","MOTOR","FOLE CANSA","FALA",
  "FA\xc3\x8dSCA","GOLPE","ELETRICIDADE","BATIMENTO",
  "16'","8'","4'","SUPERIOR","INTERVALO","TOM","TREMOLO","VELOCIDADE","FALANTE","ZUMBIDO",
  "ATAQUE","QUEDA","SUSTENTA","SOLTURA","VELOCITY","OITAVA","AFINA\xc3\x87\xc3\x83O","GLIDE","BEND",
  "ENSEMBLE","ECO","TEMPO","REPETI\xc3\x87\xc3\x95" "ES","POEIRA","MIX","TAMANHO","DURA\xc3\x87\xc3\x83O","LARGURA","SA\xc3\x8d" "DA"};
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
 body.title=juce::String::fromUTF8("RELIC\xc3\x81RIO");
 body.subtitle=juce::String::fromUTF8("QUATRO M\xc3\x81QUINAS ESQUECIDAS   \xc2\xb7   MOD WHEEL = DESGASTE + TREMOLO   \xc2\xb7   AFTERTOUCH = FOLE + FA\xc3\x8dSCA");
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
 auto u8=[](const char* s){return juce::String::fromUTF8(s);};
 // 0 TAPE LOOPS
 add("FITA",c::amber,{{"Loops",{P::FitaLevel,P::LoopMix,P::Loop2Detune,P::LoopLength,P::Loop2Octave},{P::FitaLevel}},
                      {"Fita",{P::Wear,P::Wow,P::TapeSat,P::FitaTone}}},150,{P::Loop1Wave,P::Loop2Wave});
 // 1 REED ORGAN
 add(u8("HARM\xc3\x94NIO"),c::green,{{"Palheta",{P::HarmLevel,P::Era,P::Breath,P::Speak},{P::HarmLevel}},
                                    {"Fole",{P::Motor,P::Sag}}},150,{});
 // 2 TUNING FORK
 add(u8("DIAPAS\xc3\x83O"),c::cyan,{{"Garfo",{P::ForkLevel,P::Spark,P::Clang,P::Maintain,P::Beat},{P::ForkLevel}}},150,{});
 // 3 SOVIET ORGAN
 add(u8("\xc3\x93RG\xc3\x83O SOVI\xc3\x89TICO"),c::red,{{"Registros",{P::OrganLevel,P::Reg16,P::Reg8,P::Reg4,P::Upper},{P::OrganLevel}},
                                                  {"Som",{P::OrganTone,P::TremDepth,P::TremRate,P::SpeakerBreak,P::Hum}}},150,{P::UpperInterval});
 // 4 KEYBOARD
 add("TECLADO",c::violet,{{"Envelope",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens}},
                          {u8("Afina\xc3\xa7\xc3\xa3o"),{P::Fine,P::Glide,P::BendRange}}},120,{P::Octave});
 // 5 ECHOES
 add("ECOS",c::pink,{{"Eco",{P::EchoMix,P::EchoTime,P::EchoFeedback,P::Ensemble,P::Dust},{P::EchoMix}}},120,{});
 // 6 SPACE
 add(u8("ESPA\xc3\x87O"),c::blue,{{"Reverb",{P::RevMix,P::RevSize,P::RevDecay},{P::RevMix}}},120,{});
 master=std::make_unique<soft::Master>(processor.apvts,specs()[P::Output],&specs()[P::Width],juce::String("LARGURA"),c::ink);
 body.addAndMakeVisible(*master);

 auto& m=modules;
 // ---- screens
 // FITA: both loops of tape running past the head, their flaws drawn along them.
 m[0]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float level=value(P::FitaLevel),wear=value(P::Wear)*.01f,mix=value(P::LoopMix)*.01f;
  caption(g,area,"LOOP "+abismo_ui::formatValue(specs()[P::LoopLength],value(P::LoopLength)),"DESGASTE "+juce::String(juce::roundToInt(wear*100.f))+"%");
  auto lanes=area.withTrimmedTop(16.f);
  const float laneH=lanes.getHeight()/2.f;
  for(int i=0;i<2;++i){
   auto lane=juce::Rectangle<float>(lanes.getX(),lanes.getY()+laneH*static_cast<float>(i),lanes.getWidth(),laneH).reduced(0.f,3.f);
   const float weight=i==0?1.f-mix:mix;
   const int wave=juce::roundToInt(value(i==0?P::Loop1Wave:P::Loop2Wave));
   const auto& t=tape(i);
   const auto colour=c::amber.withAlpha(level>0.f?.35f+.65f*std::min(1.f,weight*2.f):.3f);
   g.setColour(c::screenMuted.withAlpha(.12f));
   g.fillRoundedRectangle(lane,3.f);
   // The waveform, its loudness eaten where the oxide is worn.
   soft::glowStroke(g,curve(lane,220,[&](float x){
    const auto k=static_cast<std::size_t>(x*(TapeLoop::points-1));
    const float flaw=std::max(0.f,1.f+.55f*wear*t.amp[k]);
    return .75f*flaw*loopWave(wave,x*9.f);
   }),colour,1.4f);
   const float head=processor.loop1.load()*(i==0?1.f:0.f)+processor.loop2.load()*(i==1?1.f:0.f);
   const float hx=lane.getX()+head*lane.getWidth();
   g.setColour(c::screenInk.withAlpha(.8f));
   g.fillRect(hx-1.f,lane.getY(),2.f,lane.getHeight());
  }
  if(level<=0.f) notice(g,lanes,juce::String::fromUTF8("DESLIGADA  \xc2\xb7  SUBA O N\xc3\x8dVEL"));
 };
 // HARMONIO: the bellows gauge and the reed's pulse.
 m[1]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float level=value(P::HarmLevel),era=value(P::Era)*.01f;
  const float bellows=juce::jlimit(0.f,1.f,processor.bellows.load());
  caption(g,r,era<.5f?juce::String("BAQUELITE"):juce::String::fromUTF8("PL\xc3\x81STICO"),"FOLE "+juce::String(juce::roundToInt(bellows*100.f))+"%");
  auto area=r.withTrimmedTop(16.f);
  auto gauge=area.removeFromRight(28.f).reduced(6.f,2.f);
  g.setColour(c::screenMuted.withAlpha(.18f));
  g.fillRoundedRectangle(gauge,4.f);
  auto fill=gauge.withTrimmedTop(gauge.getHeight()*(1.f-bellows));
  g.setColour((bellows<.35f?c::red:c::green).withAlpha(level>0.f?.9f:.3f));
  g.fillRoundedRectangle(fill,4.f);
  const float duty=.12f+.2f*era,smoothness=1.f-.6f*bellows;
  soft::glowStroke(g,curve(area.reduced(2.f,8.f),220,[&](float t){
   const float ph=t*4.f-std::floor(t*4.f);
   const float pulse=ph<duty?1.f:-duty/(1.f-duty);
   const float smooth=std::sin(ph*6.2831853f);
   return .8f*bellows*(smoothness*smooth+(1.f-smoothness)*pulse);
  }),c::green.withAlpha(level>0.f?1.f:.3f));
  if(level<=0.f) notice(g,area,juce::String::fromUTF8("DESLIGADO  \xc2\xb7  SUBA O N\xc3\x8dVEL"));
 };
 // DIAPASAO: the fork's two tines swinging; sparks at the contact.
 m[2]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float level=value(P::ForkLevel),spark=value(P::Spark)*.01f,drive=value(P::Maintain)*.01f;
  caption(g,r,"BATIMENTO "+abismo_ui::formatValue(specs()[P::Beat],value(P::Beat)),"ELETRICIDADE "+juce::String(juce::roundToInt(drive*100.f))+"%");
  auto area=r.withTrimmedTop(16.f).reduced(4.f);
  const auto colour=c::cyan.withAlpha(level>0.f?1.f:.3f);
  const float swing=level>0.f?std::sin(animation*31.f)*(2.f+6.f*drive):0.f;
  for(int f=0;f<2;++f){
   const float cx=area.getX()+area.getWidth()*(f==0?.33f:.67f);
   const float gap=14.f, top=area.getY()+4.f, base=area.getBottom()-30.f;
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
     const float x=cx+gap+s+rnd.nextFloat()*10.f-3.f,y=top+rnd.nextFloat()*18.f;
     g.setColour(c::screenInk.withAlpha(.4f+.6f*rnd.nextFloat()));
     g.fillEllipse(x,y,2.f,2.f);
    }
   }
  }
  if(level<=0.f) notice(g,area,juce::String::fromUTF8("DESLIGADO  \xc2\xb7  SUBA O N\xc3\x8dVEL"));
 };
 // ORGAO: the registers as bars, like the stops on the panel.
 m[3]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const float level=value(P::OrganLevel);
  caption(g,area,"REGISTROS","TREMOLO "+abismo_ui::formatValue(specs()[P::TremRate],value(P::TremRate)));
  auto bars=area.withTrimmedTop(16.f);
  const int regs[4]={P::Reg16,P::Reg8,P::Reg4,P::Upper};
  const char* labels[4]={"16'","8'","4'","SUP"};
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
  if(value(P::SpeakerBreak)>0.f){
   g.setFont(soft::monoFont(10.f));
   g.setColour(c::screenInk);
   g.drawText("FALANTE RASGADO",area.withTrimmedTop(14.f).withHeight(12.f),juce::Justification::centredTop);
  }
  if(level<=0.f) notice(g,bars,juce::String::fromUTF8("DESLIGADO  \xc2\xb7  SUBA O N\xc3\x8dVEL"));
 };
 m[4]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int page){
  auto area=r.withTrimmedBottom(34.f);
  if(page==1){
   caption(g,area,"GLIDE "+abismo_ui::formatValue(specs()[P::Glide],value(P::Glide)),"BEND "+juce::String(juce::roundToInt(value(P::BendRange)))+" st");
   const float glide=juce::jlimit(0.f,1.f,std::log10(1.f+value(P::Glide)/10.f)/2.3f);
   soft::glowStroke(g,curve(area.withTrimmedTop(16.f).reduced(2.f,4.f),160,[&](float t){
    const float k=glide>0.f?juce::jlimit(0.f,1.f,(t-.3f)/(glide*.7f)):(t<.3f?0.f:1.f);
    return t<.3f?-.6f:-.6f+1.2f*(1.f-std::exp(-4.f*k))/(1.f-std::exp(-4.f));
   }),c::violet);
   return;
  }
  soft::glowStroke(g,adsr(area.reduced(2.f,4.f),value(P::Attack),value(P::Decay),value(P::Sustain)*.01f,value(P::Release)),c::violet);
 };
 // ECOS: the tape echo's repeats fading out.
 m[5]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::EchoMix)*.01f,fb=value(P::EchoFeedback)*.01f,time=value(P::EchoTime);
  caption(g,r,"ECO "+abismo_ui::formatValue(specs()[P::EchoTime],time),"POEIRA "+juce::String(juce::roundToInt(value(P::Dust)))+"%");
  auto area=r.withTrimmedTop(16.f).reduced(2.f,2.f);
  const float span=2400.f, base=area.getBottom();
  float amp=1.f;
  for(int k=0;k<24;++k){
   const float x=area.getX()+area.getWidth()*juce::jmin(1.f,static_cast<float>(k)*time/span);
   if(x>=area.getRight()) break;
   const float h=area.getHeight()*(k==0?1.f:amp*(.35f+.65f*mix));
   juce::Path tick; tick.startNewSubPath(x,base); tick.lineTo(x,base-h);
   soft::glowStroke(g,tick,c::pink.withAlpha(k==0?.5f:1.f),2.f);
   amp*=fb*.95f;
   if(k>0 && amp<.01f) break;
  }
  if(mix<=0.f) notice(g,area,"ECO DESLIGADO");
 };
 m[6]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::RevMix)*.01f,decay=value(P::RevDecay);
  caption(g,r,abismo_ui::formatValue(specs()[P::RevDecay],decay),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
  const float span=juce::jlimit(.05f,1.f,std::log10(1.f+decay)/1.4f);
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),160,[&](float t){return 1.6f*std::exp(-t/(span*.35f))-1.f;}),c::blue.withAlpha(mix>0.f?1.f:.35f));
 };

 setResizable(true,true);
 setResizeLimits(designW*6/10,designH*6/10,designW*3/2,designH*3/2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW)/designH);
 setSize(designW*9/10,designH*9/10);
 timerCallback();
 startTimerHz(30);
}

InversoRelicarioEditor::~InversoRelicarioEditor(){ setLookAndFeel(nullptr); }

// A control whose machine is switched off (or whose effect depends on another control) is dimmed.
static bool relicarioControlActive(const juce::AudioProcessorValueTreeState& apvts,int p){
 auto v=[&](int i){return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load();};
 if(p>=P::Loop1Wave && p<=P::FitaTone) return v(P::FitaLevel)>0.f;
 if(p>=P::Era && p<=P::Speak) return v(P::HarmLevel)>0.f;
 if(p>=P::Spark && p<=P::Beat) return v(P::ForkLevel)>0.f;
 if(p>=P::Reg16 && p<=P::Hum) {
  if(v(P::OrganLevel)<=0.f) return false;
  if(p==P::UpperInterval) return v(P::Upper)>0.f;
  if(p==P::TremRate) return v(P::TremDepth)>0.f;
  return true;
 }
 switch(p){
  case P::Decay: return v(P::Sustain)<100.f;
  case P::EchoTime: case P::EchoFeedback: return v(P::EchoMix)>0.f;
  case P::RevSize: case P::RevDecay: return v(P::RevMix)>0.f;
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
 master->setReadout(processor.outputPeak.load(),juce::String("VOZES ")+juce::String(processor.voicesSounding.load())+" / 12");
}

void InversoRelicarioEditor::resized(){
 body.setBounds(0,0,designW,designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW));
 presets.setBounds(designW/2-210,14,420,72);
 const int x0=10,y0=88,mainW=1150;
 auto place=[](juce::Component& comp,int x,int y,int w,int h){comp.setBounds(x,y,w,h);};
 const int r1=470,r2=designH-y0-r1-12;
 const int w4=mainW/4;
 for(int i=0;i<4;++i) place(*modules[static_cast<std::size_t>(i)],x0+w4*i,y0,i==3?mainW-3*w4:w4,r1);
 place(*modules[4],x0,y0+r1,440,r2);
 place(*modules[5],x0+440,y0+r1,380,r2);
 place(*modules[6],x0+820,y0+r1,mainW-820,r2);
 place(*master,x0+mainW,y0,designW-(x0+mainW)-10,r1+r2);
}
