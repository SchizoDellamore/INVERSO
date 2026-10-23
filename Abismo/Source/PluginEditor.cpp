#include "PluginEditor.h"
#include <cmath>

using namespace inverso_abismo;
using namespace abismo_ui;

namespace { constexpr int headerHeight=72; }

InversoAbismoEditor::InversoAbismoEditor(InversoAbismoProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 static const char* shortNames[P::Count]={
  "SHAPE","LEVEL","OCTAVE","UNISON","DETUNE","SPREAD","SUB",
  "CLUSTER","CLUSTER LVL","BREATH","TENSION",
  "MATERIAL","LEVEL","DECAY","BOW","SCRAPE","STRIKE","EXC TONE",
  "TYPE","CUTOFF","RESO","ENV AMT","KEYTRACK","DRIVE",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY",
  "ATTACK","DECAY","SUSTAIN","RELEASE",
  "GLISSANDO","GLISS TIME","DRIFT","GLIDE","BEND",
  "RATE","PITCH","FILTER","AMP",
  "MIX","SIZE","DECAY","DARKNESS","FREEZE","WIDTH","OUTPUT",
  "VOICE","VOWEL","FORMANT","CRUSH","DOWNSAMPLE","STUTTER","DEPTH","CHAOS",
  "MATERIAL B","MORPH","TUNE","SEGUE NOTA","STRETCH","BRIGHT","STEREO","OSC INPUT","RATTLE",
  "DRIVE TYPE","RING","RING PITCH","REVERSE","REV REVERSE","WARP","TEMPO","TIME"};
 panel=std::make_unique<ParameterPanel>(p.apvts,specs().data(),std::vector<ParameterPanel::Section>{
  {"OSCILADOR",{P::OscShape,P::OscLevel,P::Octave,P::Unison,P::Detune,P::Spread,P::SubLevel}},
  {"DISSONANCIA",{P::ClusterMode,P::ClusterLevel,P::NoiseLevel,P::Tension}},
  {"RESSONADOR",{P::ResMaterial,P::ResMaterialB,P::ResMorph,P::ResLevel,P::ResDecay,P::ResTune,P::ResKeytrack}},
  {"CORPO DO RESSONADOR",{P::Bow,P::Scrape,P::Strike,P::NoiseTone,P::ResInput,P::ResStretch,P::ResBright,P::ResStereo,P::ResRattle}},
  {"FILTRO",{P::FilterMode,P::Cutoff,P::Resonance,P::FilterEnv,P::Keytrack}},
  {"DESTRUICAO",{P::DriveMode,P::Drive,P::RingMix,P::RingPitch,P::Warp}},
  {"AMPLITUDE",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens}},
  {"ENVELOPE DO FILTRO",{P::FAttack,P::FDecay,P::FSustain,P::FRelease}},
  {"AFINACAO / MOVIMENTO",{P::PitchEnv,P::PitchTime,P::Drift,P::Glide,P::BendRange}},
  {"VOZ",{P::VoiceMix,P::Vowel,P::FormantShift}},
  {"MUTILACAO",{P::Crush,P::Downsample,P::StutterSync,P::StutterDiv,P::StutterMs,P::StutterDepth,P::Chaos,P::Reverse}},
  {"LFO",{P::LfoRate,P::LfoPitch,P::LfoFilter,P::LfoAmp}},
  {"ESPACO",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::RevReverse,P::Freeze,P::Width,P::Output}},
 },shortNames);
 addAndMakeVisible(*panel);
 for(int i=0;i<processor.getNumPrograms();++i) presetBox.addItem(processor.getProgramName(i),i+1);
 presetBox.onChange=[this]{
  const int index=presetBox.getSelectedItemIndex();
  if(index>=0 && index!=processor.getCurrentProgram()) processor.setCurrentProgram(index);
 };
 previous.onClick=[this]{
  const int n=processor.getNumPrograms();
  processor.setCurrentProgram((processor.getCurrentProgram()+n-1)%n);
  refreshPresetBox();
 };
 next.onClick=[this]{
  processor.setCurrentProgram((processor.getCurrentProgram()+1)%processor.getNumPrograms());
  refreshPresetBox();
 };
 addAndMakeVisible(presetBox);addAndMakeVisible(previous);addAndMakeVisible(next);
 refreshPresetBox();
 setSize(1180,headerHeight+panel->heightForWidth(1148)+40);
 timerCallback();
 startTimerHz(20);
}

InversoAbismoEditor::~InversoAbismoEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoEditor::refreshPresetBox(){
 presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
}

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
 refreshPresetBox();
 panel->dimInactive([this](int p){return synthControlActive(processor.apvts,p);});
 const float peak=processor.outputPeak.load();
 const int voices=processor.voicesSounding.load();
 if(std::abs(peak-shownPeak)>.001f || voices!=shownVoices){
  shownPeak=peak;shownVoices=voices;
  repaint(0,0,getWidth(),headerHeight);
 }
}

void InversoAbismoEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);
 g.setFont(juce::FontOptions(28.f,juce::Font::bold));
 g.drawText("INVERSO   /   ABISMO",22,12,420,40,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(12.f);
 g.drawText("SINTETIZADOR DE TENSAO PARA TRILHA   /   MOD WHEEL = TENSION   /   AFTERTOUCH = BOW + FILTRO   /   STUTTER NO TEMPO DA DAW OU LIVRE",24,48,760,18,juce::Justification::centredLeft);
 const float db=juce::Decibels::gainToDecibels(shownPeak,-90.f);
 g.setColour(shownPeak>.89f?juce::Colours::salmon:bone);
 g.setFont(12.f);
 g.drawText("VOZES "+juce::String(shownVoices)+"/16    PICO "+juce::String(db,1)+" dBFS",getWidth()-290,48,268,18,juce::Justification::centredRight);
 g.setColour(muted.withAlpha(.8f));g.setFont(11.f);
 g.drawText("SHIFT + RODA: ajuste fino   |   arraste vertical ou horizontal nos knobs   |   presets: menu ou < >",
  22,getHeight()-26,900,18,juce::Justification::centredLeft);
}

void InversoAbismoEditor::resized(){
 presetBox.setBounds(getWidth()-372,14,270,28);
 previous.setBounds(getWidth()-96,14,34,28);
 next.setBounds(getWidth()-56,14,34,28);
 panel->setBounds(16,headerHeight,getWidth()-32,getHeight()-headerHeight-34);
}
