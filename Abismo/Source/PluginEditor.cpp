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
  "MATERIAL B","MORPH","TUNE","KEYTRACK","STRETCH","BRIGHT","STEREO","OSC INPUT","RATTLE",
  "DRIVE TYPE","RING","RING PITCH","REVERSE","REV REVERSE","WARP"};
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
  {"MUTILACAO",{P::Crush,P::Downsample,P::StutterDiv,P::StutterDepth,P::Chaos,P::Reverse}},
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
 startTimerHz(20);
}

InversoAbismoEditor::~InversoAbismoEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoEditor::refreshPresetBox(){
 presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
}

void InversoAbismoEditor::timerCallback(){
 refreshPresetBox();
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
 g.drawText("SINTETIZADOR DE TENSAO PARA TRILHA   /   MOD WHEEL = TENSION   /   AFTERTOUCH = BOW + FILTRO   /   STUTTER SEGUE O TEMPO DA DAW",24,48,760,18,juce::Justification::centredLeft);
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
