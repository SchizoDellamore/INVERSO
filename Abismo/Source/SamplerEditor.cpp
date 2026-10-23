#include "SamplerEditor.h"
#include <cmath>

using namespace inverso_abismo::sampler;
using namespace abismo_ui;

namespace { constexpr int headerHeight=72, viewTop=112, viewHeight=150; }
static bool samplerControlActive(const juce::AudioProcessorValueTreeState&,int);

SampleView::SampleView(InversoAbismoSamplerProcessor& p):processor(p){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }

float SampleView::param(const char* id) const { return processor.apvts.getRawParameterValue(id)->load(); }

void SampleView::refresh(){
 peaks.clear();
 const auto sample=processor.getDisplaySample();
 const int columns=std::max(1,getWidth());
 if(!sample || sample->length()==0){repaint();return;}
 const auto& l=sample->left;
 const auto& r=sample->right.empty()?sample->left:sample->right;
 const double per=static_cast<double>(sample->length())/columns;
 peaks.resize(static_cast<std::size_t>(columns));
 for(int c=0;c<columns;++c){
  const auto a=static_cast<std::size_t>(c*per),b=std::max(a+1,static_cast<std::size_t>((c+1)*per));
  float lo=0.f,hi=0.f;
  for(std::size_t i=a;i<b && i<l.size();++i){lo=std::min({lo,l[i],r[i]});hi=std::max({hi,l[i],r[i]});}
  peaks[static_cast<std::size_t>(c)]={lo,hi};
 }
 repaint();
}

void SampleView::paint(juce::Graphics& g){
 const auto area=getLocalBounds().toFloat();
 g.setColour(panel);
 g.fillRoundedRectangle(area,7.f);
 const float w=area.getWidth(),h=area.getHeight(),mid=h*.5f;
 const bool granular=param("MODE")>=.5f;
 const int loop=juce::roundToInt(param("LOOP_MODE"));
 if(!granular && loop>0){
  const float a=std::min(param("LOOP_START"),param("LOOP_END"))*.01f,b=std::max(param("LOOP_START"),param("LOOP_END"))*.01f;
  g.setColour(blood.withAlpha(.13f));
  g.fillRect(a*w,0.f,(b-a)*w,h);
 }
 g.setColour(bone.withAlpha(.85f));
 for(std::size_t x=0;x<peaks.size();++x){
  const auto [lo,hi]=peaks[x];
  g.drawVerticalLine(static_cast<int>(x),mid-hi*mid*.92f,mid-lo*mid*.92f+1.f);
 }
 if(granular){
  const float pos=param("POSITION")*.01f*w,spray=param("SPRAY")*.01f;
  const auto sample=processor.getDisplaySample();
  if(sample && sample->length()>0){
   const float sprayPx=static_cast<float>(spray*.5*sample->sampleRate/sample->length())*w;
   g.setColour(blood.withAlpha(.18f));
   g.fillRect(pos-sprayPx,0.f,sprayPx*2.f,h);
  }
  g.setColour(blood);
  g.fillRect(pos-1.f,0.f,2.f,h);
 } else {
  g.setColour(blood);
  g.fillRect(param("START")*.01f*w-1.f,0.f,2.f,h);
 }
 const float head=processor.playhead.load();
 if(head>=0.f){g.setColour(text);g.fillRect(head*w,0.f,1.5f,h);}
 if(peaks.empty()){
  g.setColour(muted);g.setFont(13.f);
  g.drawText("sem sample",getLocalBounds(),juce::Justification::centred);
 }
}

void SampleView::setFromX(float x){
 const bool granular=param("MODE")>=.5f;
 auto* p=processor.apvts.getParameter(granular?"POSITION":"START");
 const float value=juce::jlimit(0.f,100.f,x/static_cast<float>(std::max(1,getWidth()))*100.f);
 p->setValueNotifyingHost(p->convertTo0to1(value));
 repaint();
}
void SampleView::mouseDown(const juce::MouseEvent& e){
 const bool granular=param("MODE")>=.5f;
 processor.apvts.getParameter(granular?"POSITION":"START")->beginChangeGesture();
 setFromX(e.position.x);
 processor.apvts.getParameter(granular?"POSITION":"START")->endChangeGesture();
}
void SampleView::mouseDrag(const juce::MouseEvent& e){ setFromX(e.position.x); }

InversoAbismoSamplerEditor::InversoAbismoSamplerEditor(InversoAbismoSamplerProcessor& p)
 :AudioProcessorEditor(&p),processor(p),view(p){
 setLookAndFeel(&look);
 static const char* shortNames[P::Count]={
  "MODE","ROOT","TUNE","START","REVERSE","LOOP","LOOP START","LOOP END","XFADE",
  "POSITION","SCAN","SIZE","DENSITY","SPRAY","PITCH RND","REVERSE %","SPREAD",
  "TYPE","CUTOFF","RESO","ENV AMT","KEYTRACK","DRIVE",
  "VOICE","VOWEL","FORMANT",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY",
  "ATTACK","DECAY","SUSTAIN","RELEASE",
  "GLISSANDO","GLISS TIME","DRIFT","BEND",
  "RATE","PITCH","FILTER","AMP","POSITION",
  "TENSION","CRUSH","DOWNSAMPLE","STUTTER","DEPTH","CHAOS",
  "MIX","SIZE","DECAY","DARKNESS","FREEZE","WIDTH","OUTPUT","TEMPO","TIME"};
 panel=std::make_unique<ParameterPanel>(p.apvts,specs().data(),std::vector<ParameterPanel::Section>{
  {"SAMPLE",{P::RootNote,P::Tune,P::Start,P::Reverse,P::LoopMode,P::LoopStart,P::LoopEnd,P::Xfade}},
  {"GRANULAR",{P::Mode,P::Position,P::Scan,P::GrainSize,P::Density,P::Spray,P::GrainPitch,P::GrainReverse,P::GrainSpread}},
  {"FILTRO",{P::FilterMode,P::Cutoff,P::Resonance,P::FilterEnv,P::Keytrack,P::Drive}},
  {"VOZ",{P::VoiceMix,P::Vowel,P::FormantShift}},
  {"AMPLITUDE",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens}},
  {"ENVELOPE DO FILTRO",{P::FAttack,P::FDecay,P::FSustain,P::FRelease}},
  {"AFINACAO / MOVIMENTO",{P::PitchEnv,P::PitchTime,P::Drift,P::BendRange,P::Tension}},
  {"LFO",{P::LfoRate,P::LfoPitch,P::LfoFilter,P::LfoAmp,P::LfoPosition}},
  {"MUTILACAO",{P::Crush,P::Downsample,P::StutterSync,P::StutterDiv,P::StutterMs,P::StutterDepth,P::Chaos}},
  {"ESPACO",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::Freeze,P::Width,P::Output}},
 },shortNames);
 addAndMakeVisible(*panel);
 addAndMakeVisible(view);
 loadButton.onClick=[this]{
  chooser=std::make_unique<juce::FileChooser>("Escolha um sample",juce::File(),"*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
  chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
   [this](const juce::FileChooser& fc){ if(fc.getResult()!=juce::File()) load(fc.getResult()); });
 };
 builtInButton.onClick=[this]{ processor.useBuiltInSample(); };
 addAndMakeVisible(loadButton);addAndMakeVisible(builtInButton);
 for(int i=0;i<processor.getNumPrograms();++i) presetBox.addItem(processor.getProgramName(i),i+1);
 presetBox.onChange=[this]{
  const int index=presetBox.getSelectedItemIndex();
  if(index>=0 && index!=processor.getCurrentProgram()) processor.setCurrentProgram(index);
 };
 previous.onClick=[this]{
  const int n=processor.getNumPrograms();
  processor.setCurrentProgram((processor.getCurrentProgram()+n-1)%n);
 };
 next.onClick=[this]{ processor.setCurrentProgram((processor.getCurrentProgram()+1)%processor.getNumPrograms()); };
 addAndMakeVisible(presetBox);addAndMakeVisible(previous);addAndMakeVisible(next);
 setSize(1380,viewTop+viewHeight+12+panel->heightForWidth(1348)+40);
 panel->dimInactive([this](int param){return samplerControlActive(processor.apvts,param);});
 startTimerHz(25);
}

InversoAbismoSamplerEditor::~InversoAbismoSamplerEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoSamplerEditor::load(const juce::File& file){
 if(!processor.loadSampleFile(file)) status="Nao foi possivel ler: "+file.getFileName();
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
  case P::Start: case P::LoopMode: return !granular;
  case P::LoopStart: case P::LoopEnd: case P::Xfade: return !granular && loop;
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
 panel->dimInactive([this](int p){return samplerControlActive(processor.apvts,p);});
 presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
 if(shownVersion!=processor.sampleVersion.load()){
  shownVersion=processor.sampleVersion.load();
  const auto missing=processor.getMissingPath();
  if(missing.isNotEmpty()) status="ARQUIVO NAO ENCONTRADO: "+missing+"  (tocando a voz interna)";
  else if(const auto s=processor.getDisplaySample())
   status=juce::String::fromUTF8(s->name.c_str())+"   "+juce::String(s->length()/s->sampleRate,2)+" s   "+
          juce::String(s->sampleRate/1000.,1)+" kHz   "+(s->right.empty()?"mono":"estereo");
  view.refresh();
  repaint(0,headerHeight,getWidth(),viewTop-headerHeight);
 }
 view.repaint();
 const float peak=processor.outputPeak.load();
 const int voices=processor.voicesSounding.load();
 if(std::abs(peak-shownPeak)>.001f || voices!=shownVoices){
  shownPeak=peak;shownVoices=voices;
  repaint(0,0,getWidth(),headerHeight);
 }
}

void InversoAbismoSamplerEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);
 g.setFont(juce::FontOptions(28.f,juce::Font::bold));
 g.drawText("INVERSO   /   ABISMO SAMPLER",22,12,560,40,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(12.f);
 g.drawText("MANIPULACAO DE SAMPLES PARA TRILHA   /   ARRASTE UM ARQUIVO DE AUDIO PARA CA   /   MOD WHEEL = TENSION",24,48,820,18,juce::Justification::centredLeft);
 const float db=juce::Decibels::gainToDecibels(shownPeak,-90.f);
 g.setColour(shownPeak>.89f?juce::Colours::salmon:bone);
 g.drawText("VOZES "+juce::String(shownVoices)+"/16    PICO "+juce::String(db,1)+" dBFS",getWidth()-290,48,268,18,juce::Justification::centredRight);
 g.setColour(status.startsWith("ARQUIVO")||status.startsWith("Nao")?juce::Colours::salmon:text);
 g.setFont(juce::FontOptions(13.f,juce::Font::bold));
 g.drawText(status,330,headerHeight+6,getWidth()-350,26,juce::Justification::centredLeft);
 g.setColour(muted.withAlpha(.8f));g.setFont(11.f);
 g.drawText("CLIQUE NA ONDA: inicio (classico) ou posicao dos graos (granular)   |   SHIFT + RODA: ajuste fino",
  22,getHeight()-26,900,18,juce::Justification::centredLeft);
 if(dragging){g.setColour(blood);g.drawRect(getLocalBounds(),3);}
}

void InversoAbismoSamplerEditor::resized(){
 presetBox.setBounds(getWidth()-372,14,270,28);
 previous.setBounds(getWidth()-96,14,34,28);
 next.setBounds(getWidth()-56,14,34,28);
 loadButton.setBounds(16,headerHeight+4,170,30);
 builtInButton.setBounds(194,headerHeight+4,124,30);
 view.setBounds(16,viewTop,getWidth()-32,viewHeight);
 panel->setBounds(16,viewTop+viewHeight+12,getWidth()-32,getHeight()-(viewTop+viewHeight+12)-34);
}
