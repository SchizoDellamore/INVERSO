#include "PluginEditor.h"
#include <cmath>

using namespace inverso_berimbass;

namespace {
constexpr int headerHeight=72,viewHeight=150;
// Berimbass palette: verga wood, gourd ochre, steel wire. Same dark ground as the family.
const juce::Colour wood{150,98,52};
const juce::Colour gourdColour{196,140,62};
const juce::Colour wire{205,205,198};
}

void BerimbauView::paint(juce::Graphics& g){
 using namespace abismo_ui;
 const auto area=getLocalBounds().toFloat();
 g.setColour(panel);
 g.fillRoundedRectangle(area,7.f);
 auto param=[this](const char* id){return processor.apvts.getRawParameterValue(id)->load();};
 const float gourdOpen=juce::jlimit(0.f,1.f,param("GOURD")*.01f+processor.modWheel.load());
 const float stone=processor.pressure.load();
 const int body=juce::roundToInt(param("BODY"));
 const float vibrate=juce::jlimit(0.f,1.f,processor.outputPeak.load()*3.f);
 // The wire runs straight between the ends of the verga, which bows below it.
 const float x0=area.getX()+60.f,x1=area.getRight()-60.f,wireY=area.getY()+34.f;
 juce::Path verga;
 verga.startNewSubPath(x0,wireY);
 verga.quadraticTo((x0+x1)*.5f,wireY+95.f,x1,wireY);
 g.setColour(wood);
 g.strokePath(verga,juce::PathStrokeType(7.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 // The wire, trembling with the output level.
 juce::Path string;
 string.startNewSubPath(x0,wireY);
 string.quadraticTo((x0+x1)*.5f,wireY+vibrate*6.f,x1,wireY);
 g.setColour(wire);
 g.strokePath(string,juce::PathStrokeType(1.6f));
 // The resonator, tied to the verga near the right end and hanging below it.
 const float cx=x1-150.f;
 const float t=(cx-x0)/(x1-x0);
 const float vergaY=wireY+2.f*t*(1.f-t)*95.f;
 const float cy=vergaY+32.f;
 g.setColour(wire.withAlpha(.6f));
 if(body!=3) g.drawLine(cx,wireY,cx,vergaY,1.2f);
 if(body==0){
  g.setColour(gourdColour);
  g.fillEllipse(cx-30.f,cy-26.f,60.f,52.f);
  // the opening faces the player; its "mouth" widens as the gourd opens
  const float mouth=10.f+34.f*gourdOpen;
  g.setColour(bg.withAlpha(.85f));
  g.fillEllipse(cx-mouth*.5f,cy+12.f-mouth*.18f,mouth,mouth*.36f);
 } else if(body==1){
  g.setColour(text.withAlpha(.9f));
  g.fillEllipse(cx-32.f,cy-22.f,64.f,44.f);
  g.setColour(wood);
  g.drawEllipse(cx-32.f,cy-22.f,64.f,44.f,4.f);
 } else if(body==2){
  juce::Path tub;
  tub.addQuadrilateral(cx-36.f,cy-24.f,cx+36.f,cy-24.f,cx+27.f,cy+24.f,cx-27.f,cy+24.f);
  g.setColour(juce::Colour::fromRGB(120,126,132));
  g.fillPath(tub);
  g.setColour(juce::Colour::fromRGB(160,166,170));
  g.drawLine(cx-34.f,cy-12.f,cx+34.f,cy-12.f,2.f);
  g.drawLine(cx-30.f,cy+12.f,cx+30.f,cy+12.f,2.f);
 }
 // The stone (dobrao) close to the wire; pressure pushes it on.
 const float sx=x0+70.f,sy=wireY-12.f+10.f*stone;
 g.setColour(juce::Colour::fromRGB(110,110,104).interpolatedWith(wire,stone*.4f));
 g.fillEllipse(sx-11.f,sy-9.f,22.f,18.f);
 g.setColour(muted);g.setFont(juce::FontOptions(11.5f));
 g.drawText("CABACA "+juce::String(juce::roundToInt(gourdOpen*100.f))+"%  (mod wheel)",area.getX()+14.f,area.getBottom()-24.f,260.f,16.f,juce::Justification::left);
 g.drawText("DOBRAO "+juce::String(juce::roundToInt(stone*100.f))+"%  (aftertouch)",area.getRight()-274.f,area.getBottom()-24.f,260.f,16.f,juce::Justification::right);
}

InversoBerimbassEditor::InversoBerimbassEditor(InversoBerimbassProcessor& p):AudioProcessorEditor(&p),processor(p),view(p){
 setLookAndFeel(&look);
 static const char* shortNames[P::Count]={
  "TECHNIQUE","DECAY","BRIGHTNESS","POSITION","STIFFNESS","BUZZ","PRESSURE","BOW ATTACK","RELEASE",
  "BODY","BODY MIX","GOURD","GOURD RATE","GOURD DEPTH","BODY TUNE",
  "OCTAVE","FINE","MODE","GLIDE","BEND","STONE BEND","VIBRATO","VIB RATE",
  "SUB","DRIVE","TONE","VELOCITY","REVERB","SIZE","DECAY","OUTPUT",
  "CAXIXI"};
 panel=std::make_unique<abismo_ui::ParameterPanel>(p.apvts,specs().data(),std::vector<abismo_ui::ParameterPanel::Section>{
  {"CORDA",{P::Technique,P::Decay,P::Brightness,P::Position,P::Stiffness,P::Buzz,P::Pressure,P::Attack,P::Release}},
  {"CORPO",{P::Body,P::BodyMix,P::Gourd,P::GourdRate,P::GourdDepth,P::BodyTune,P::Caxixi}},
  {"AFINACAO",{P::Octave,P::Tune,P::VoiceMode,P::Glide,P::BendRange,P::StoneBend,P::Vibrato,P::VibRate}},
  {"SAIDA",{P::Sub,P::Drive,P::Tone,P::VelSens,P::RevMix,P::RevSize,P::RevDecay,P::Output}},
 },shortNames);
 addAndMakeVisible(*panel);
 addAndMakeVisible(view);
 for(int i=0;i<processor.getNumPrograms();++i) presetBox.addItem(processor.getProgramName(i),i+1);
 presetBox.onChange=[this]{
  const int index=presetBox.getSelectedItemIndex();
  if(index>=0 && index!=processor.getCurrentProgram()) processor.setCurrentProgram(index);
 };
 previous.onClick=[this]{ const int n=processor.getNumPrograms(); processor.setCurrentProgram((processor.getCurrentProgram()+n-1)%n); };
 next.onClick=[this]{ processor.setCurrentProgram((processor.getCurrentProgram()+1)%processor.getNumPrograms()); };
 addAndMakeVisible(presetBox);addAndMakeVisible(previous);addAndMakeVisible(next);
 setSize(1260,headerHeight+viewHeight+12+panel->heightForWidth(1228)+40);
 timerCallback();
 startTimerHz(30);
}

InversoBerimbassEditor::~InversoBerimbassEditor(){ setLookAndFeel(nullptr); }

void InversoBerimbassEditor::timerCallback(){
 presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
 view.repaint();
 const float peak=processor.outputPeak.load();
 const int voices=processor.voicesSounding.load();
 if(std::abs(peak-shownPeak)>.001f || voices!=shownVoices){
  shownPeak=peak;shownVoices=voices;
  repaint(0,0,getWidth(),headerHeight);
 }
}

void InversoBerimbassEditor::paint(juce::Graphics& g){
 using namespace abismo_ui;
 g.fillAll(bg);
 g.setColour(text);
 g.setFont(juce::FontOptions(28.f,juce::Font::bold));
 g.drawText("INVERSO   /   BERIMBASS",22,12,560,40,juce::Justification::centredLeft);
 g.setColour(gourdColour);g.setFont(12.f);
 g.drawText("BAIXO DE BERIMBAU, BANJO E BALDE   /   PIZZICATO, BAMBU, ARCO, PEDRA   /   MOD WHEEL = CABACA   /   AFTERTOUCH = DOBRAO",24,48,900,18,juce::Justification::centredLeft);
 const float db=juce::Decibels::gainToDecibels(shownPeak,-90.f);
 g.setColour(shownPeak>.89f?juce::Colours::salmon:bone);
 g.drawText("CORDAS "+juce::String(shownVoices)+"    PICO "+juce::String(db,1)+" dBFS",getWidth()-290,48,268,18,juce::Justification::centredRight);
 g.setColour(muted.withAlpha(.8f));g.setFont(11.f);
 g.drawText("MODE MONO: toque legato para deslizar   |   SHIFT + RODA: ajuste fino   |   presets: menu ou < >",22,getHeight()-26,900,18,juce::Justification::centredLeft);
}

void InversoBerimbassEditor::resized(){
 presetBox.setBounds(getWidth()-372,14,270,28);
 previous.setBounds(getWidth()-96,14,34,28);
 next.setBounds(getWidth()-56,14,34,28);
 view.setBounds(16,headerHeight,getWidth()-32,viewHeight);
 panel->setBounds(16,headerHeight+viewHeight+12,getWidth()-32,getHeight()-(headerHeight+viewHeight+12)-34);
}
