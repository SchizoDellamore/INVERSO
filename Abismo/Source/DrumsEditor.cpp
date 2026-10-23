#include "DrumsEditor.h"
#include <cmath>

using namespace inverso_abismo::drums;
using namespace abismo_ui;

namespace {
constexpr int headerHeight=72,padW=120,padH=76,padGap=8,gridTop=headerHeight+6;
juce::String noteName(int note){ return juce::MidiMessage::getMidiNoteName(note,true,true,3); }

const char* const* shortNames(){
 static std::array<const char*,P::Count> names=[]{
  std::array<const char*,P::Count> n{};
  static const char* padNames[P::PadCount]={"OBJECT","TUNE","DECAY","TONE","RATTLE","RUST","VARIATION","LEVEL","PAN"};
  for(int pad=0;pad<numPads;++pad) for(int k=0;k<P::PadCount;++k) n[static_cast<std::size_t>(P::padBase(pad)+k)]=padNames[k];
  const std::pair<int,const char*> rest[]={
   {P::Stumble,"STUMBLE"},{P::StutterDiv,"STUTTER"},{P::StutterDepth,"DEPTH"},{P::Chaos,"CHAOS"},{P::Crush,"CRUSH"},
   {P::Megaphone,"MEGAPHONE"},{P::Gramophone,"GRAMOPHONE"},{P::RoomMix,"MIX"},{P::RoomSize,"SIZE"},{P::RoomDecay,"DECAY"},
   {P::RoomDamp,"DARKNESS"},{P::SawLevel,"LEVEL"},{P::SawAttack,"BOW"},{P::SawRelease,"RELEASE"},{P::SawVibrato,"VIBRATO"},
   {P::SawVibRate,"VIB RATE"},{P::SawGlide,"GLIDE"},{P::SawTone,"BOW NOISE"},{P::SawWobble,"WOBBLE"},{P::VelSens,"VELOCITY"},
   {P::Output,"OUTPUT"}};
  for(const auto& [i,s]:rest) n[static_cast<std::size_t>(i)]=s;
  return n;
 }();
 return names.data();
}
}

PadButton::PadButton(int i,std::function<void(int)> press):index(i),onPress(std::move(press)){
 setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void PadButton::setObjectName(const juce::String& n){ if(n!=objectName){objectName=n;repaint();} }
void PadButton::setSelected(bool s){ if(s!=selected){selected=s;repaint();} }
void PadButton::flash(){ light=1.f; repaint(); }
bool PadButton::decay(){
 if(light<=0.f) return false;
 light=std::max(0.f,light-.12f);
 repaint();
 return light>0.f;
}
void PadButton::paint(juce::Graphics& g){
 const auto r=getLocalBounds().toFloat().reduced(1.f);
 g.setColour(panel.brighter(.06f).interpolatedWith(blood,light*.55f));
 g.fillRoundedRectangle(r,6.f);
 g.setColour(selected?blood.brighter(.25f):juce::Colour::fromRGB(48,46,50));
 g.drawRoundedRectangle(r,6.f,selected?2.f:1.f);
 g.setColour(muted);
 g.setFont(juce::FontOptions(11.f));
 g.drawText(noteName(firstPadNote+index)+"   PAD "+juce::String(index+1),r.reduced(9.f,6.f).toNearestInt(),juce::Justification::topLeft);
 g.setColour(text);
 g.setFont(juce::FontOptions(13.f,juce::Font::bold));
 g.drawFittedText(objectName.toUpperCase(),r.reduced(9.f,8.f).withTrimmedTop(18.f).toNearestInt(),juce::Justification::bottomLeft,2);
}
void PadButton::mouseDown(const juce::MouseEvent&){ if(onPress) onPress(index); }

InversoAbismoDrumsEditor::InversoAbismoDrumsEditor(InversoAbismoDrumsProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 for(int i=0;i<numPads;++i){
  pads.push_back(std::make_unique<PadButton>(i,[this](int pad){ selectPad(pad); processor.auditionPad(pad); }));
  addAndMakeVisible(*pads.back());
  const int b=P::padBase(i);
  std::vector<int> params;
  for(int k=0;k<P::PadCount;++k) params.push_back(b+k);
  padPanels.push_back(std::make_unique<ParameterPanel>(p.apvts,specs().data(),
   std::vector<ParameterPanel::Section>{{"PAD "+juce::String(i+1)+"   /   "+noteName(firstPadNote+i),params}},shortNames()));
  addChildComponent(*padPanels.back());
 }
 globals=std::make_unique<ParameterPanel>(p.apvts,specs().data(),std::vector<ParameterPanel::Section>{
  {"MAQUINA QUEBRADA",{P::Stumble,P::StutterDiv,P::StutterDepth,P::Chaos,P::Crush}},
  {"CARATER",{P::Megaphone,P::Gramophone}},
  {"SALA",{P::RoomMix,P::RoomSize,P::RoomDecay,P::RoomDamp}},
  {"SAIDA",{P::VelSens,P::Output}},
  {"SERROTE   /   C3 PARA CIMA",{P::SawLevel,P::SawAttack,P::SawRelease,P::SawVibrato,P::SawVibRate,P::SawGlide,P::SawTone,P::SawWobble}},
 },shortNames());
 addAndMakeVisible(*globals);
 for(int i=0;i<processor.getNumPrograms();++i) kitBox.addItem(processor.getProgramName(i),i+1);
 kitBox.onChange=[this]{
  const int index=kitBox.getSelectedItemIndex();
  if(index>=0 && index!=processor.getCurrentProgram()) processor.setCurrentProgram(index);
 };
 previous.onClick=[this]{ const int n=processor.getNumPrograms(); processor.setCurrentProgram((processor.getCurrentProgram()+n-1)%n); };
 next.onClick=[this]{ processor.setCurrentProgram((processor.getCurrentProgram()+1)%processor.getNumPrograms()); };
 addAndMakeVisible(kitBox);addAndMakeVisible(previous);addAndMakeVisible(next);
 selectPad(0);
 const int gridHeight=4*padH+3*padGap;
 setSize(1260,gridTop+gridHeight+14+globals->heightForWidth(1228)+40);
 timerCallback();
 startTimerHz(30);
}

InversoAbismoDrumsEditor::~InversoAbismoDrumsEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoDrumsEditor::selectPad(int pad){
 selected=juce::jlimit(0,numPads-1,pad);
 for(int i=0;i<numPads;++i){
  pads[static_cast<std::size_t>(i)]->setSelected(i==selected);
  padPanels[static_cast<std::size_t>(i)]->setVisible(i==selected);
 }
}

void InversoAbismoDrumsEditor::timerCallback(){
 kitBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
 const auto lights=processor.takePadLights();
 for(int i=0;i<numPads;++i){
  auto& pad=*pads[static_cast<std::size_t>(i)];
  const int object=juce::roundToInt(processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(P::padBase(i)+P::Object)].id)->load());
  pad.setObjectName(juce::String::fromUTF8(objects()[static_cast<std::size_t>(juce::jlimit(0,15,object))].name));
  if(lights & (1u<<i)) pad.flash(); else pad.decay();
 }
 const float peak=processor.outputPeak.load();
 const int hits=processor.hitsSounding.load();
 if(std::abs(peak-shownPeak)>.001f || hits!=shownHits){
  shownPeak=peak;shownHits=hits;
  repaint(0,0,getWidth(),headerHeight);
 }
}

void InversoAbismoDrumsEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);
 g.setFont(juce::FontOptions(28.f,juce::Font::bold));
 g.drawText("INVERSO   /   ABISMO DRUMS",22,12,560,40,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(12.f);
 g.drawText("PERCUSSAO DE FERRO-VELHO   /   PADS C1 A D#2   /   SERROTE DO C3 PARA CIMA   /   CLIQUE NUM PAD PARA OUVIR E EDITAR",24,48,900,18,juce::Justification::centredLeft);
 const float db=juce::Decibels::gainToDecibels(shownPeak,-90.f);
 g.setColour(shownPeak>.89f?juce::Colours::salmon:bone);
 g.drawText("SOANDO "+juce::String(shownHits)+"    PICO "+juce::String(db,1)+" dBFS",getWidth()-290,48,268,18,juce::Justification::centredRight);
 // Notes under the selected pad's controls.
 const int x=16+4*padW+3*padGap+16;
 const int y=gridTop+140+18;
 g.setColour(muted);g.setFont(12.5f);
 const juce::String help=
  "OBJECT troca o objeto do pad; qualquer pad pode ser qualquer objeto.\n"
  "RATTLE: pecas soltas vibrando junto.   RUST: o objeto torto, desafinado do jeito dele.\n"
  "VARIATION: cada batida diferente da anterior (afinacao, decay, volume).\n"
  "STUMBLE atrasa batidas ao acaso (ate 80 ms); STUTTER repete como disco arranhado.\n"
  "MEGAPHONE e GRAMOPHONE colocam o kit dentro de um alto-falante velho ou de um disco de 78 rpm.\n"
  "O serrote e monofonico: toque legato para ele deslizar entre as notas.";
 g.drawMultiLineText(help,x+4,y+14,getWidth()-x-24,juce::Justification::left,4.f);
 g.setColour(muted.withAlpha(.8f));g.setFont(11.f);
 g.drawText("SHIFT + RODA: ajuste fino   |   kits: menu ou < >",22,getHeight()-26,900,18,juce::Justification::centredLeft);
}

void InversoAbismoDrumsEditor::resized(){
 kitBox.setBounds(getWidth()-372,14,270,28);
 previous.setBounds(getWidth()-96,14,34,28);
 next.setBounds(getWidth()-56,14,34,28);
 // Pad 1 bottom-left, like a drum machine: rows run upward.
 for(int i=0;i<numPads;++i){
  const int col=i%4,row=3-i/4;
  pads[static_cast<std::size_t>(i)]->setBounds(16+col*(padW+padGap),gridTop+row*(padH+padGap),padW,padH);
 }
 const int x=16+4*padW+3*padGap+16;
 for(auto& panel:padPanels) panel->setBounds(x,gridTop,getWidth()-x-16,140);
 const int gridBottom=gridTop+4*padH+3*padGap;
 globals->setBounds(16,gridBottom+14,getWidth()-32,getHeight()-(gridBottom+14)-34);
}
