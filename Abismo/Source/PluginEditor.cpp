#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo;

namespace {
const auto bg=juce::Colour::fromRGB(10,10,13);
const auto panel=juce::Colour::fromRGB(21,21,26);
const auto text=juce::Colour::fromRGB(229,221,206);
const auto muted=juce::Colour::fromRGB(140,136,130);
const auto blood=juce::Colour::fromRGB(176,38,42);
const auto bone=juce::Colour::fromRGB(155,147,131);
constexpr int knobWidth=70, choiceWidth=124, knobHeight=96, sectionPad=10, headerHeight=72;
int naturalWidth(int param){return specs()[static_cast<std::size_t>(param)].kind==Kind::Choice?choiceWidth:knobWidth;}

juce::String formatValue(const ParamSpec& s,double v){
 const juce::String unit(s.unit);
 if(unit=="Hz") return v>=1000.?juce::String(v/1000.,2)+" kHz":juce::String(v,v<10.?2:0)+" Hz";
 if(unit=="ms") return v>=1000.?juce::String(v/1000.,2)+" s":juce::String(v,0)+" ms";
 if(unit=="s") return juce::String(v,v<10.?2:1)+" s";
 if(unit=="dB") return juce::String(v,1)+" dB";
 if(unit=="st") return (v>0?"+":"")+juce::String(v,s.kind==Kind::Int?0:1)+" st";
 return juce::String(v,0)+"%";
}
}

AbismoLook::AbismoLook(){
 setColour(juce::ComboBox::backgroundColourId,panel.brighter(.08f));
 setColour(juce::ComboBox::textColourId,text);
 setColour(juce::ComboBox::outlineColourId,muted.withAlpha(.35f));
 setColour(juce::ComboBox::arrowColourId,bone);
 setColour(juce::PopupMenu::backgroundColourId,panel);
 setColour(juce::PopupMenu::textColourId,text);
 setColour(juce::PopupMenu::highlightedBackgroundColourId,blood.withAlpha(.6f));
 setColour(juce::TextButton::buttonColourId,panel.brighter(.08f));
 setColour(juce::TextButton::textColourOffId,text);
 setColour(juce::ToggleButton::textColourId,text);
 setColour(juce::ToggleButton::tickColourId,blood.brighter(.3f));
 setColour(juce::Slider::textBoxTextColourId,text);
 setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxHighlightColourId,blood.withAlpha(.5f));
 setColour(juce::Label::textColourId,muted);
}

void AbismoLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&){
 const auto area=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),static_cast<float>(h)).reduced(4.f);
 const float radius=std::min(area.getWidth(),area.getHeight())*.5f;
 const auto centre=area.getCentre();
 const float angle=start+pos*(end-start);
 juce::Path track;
 track.addCentredArc(centre.x,centre.y,radius,radius,0.f,start,end,true);
 g.setColour(juce::Colour::fromRGB(48,46,50));
 g.strokePath(track,juce::PathStrokeType(3.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 juce::Path value;
 value.addCentredArc(centre.x,centre.y,radius,radius,0.f,start,angle,true);
 g.setColour(blood);
 g.strokePath(value,juce::PathStrokeType(3.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 g.setColour(panel.brighter(.12f));
 g.fillEllipse(juce::Rectangle<float>(radius*1.4f,radius*1.4f).withCentre(centre));
 g.setColour(text);
 const juce::Point<float> tip(centre.x+std::sin(angle)*radius*.62f,centre.y-std::cos(angle)*radius*.62f);
 g.drawLine({centre,tip},2.f);
}

AbismoKnob::AbismoKnob(){
 setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
 setTextBoxStyle(juce::Slider::TextBoxBelow,false,knobWidth,18);
 setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
 setDoubleClickReturnValue(false,0.0);
 setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxTextColourId,text);
}
void AbismoKnob::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
 if(!isEnabled() || std::abs(wheel.deltaY)<1e-6f) return;
 // Step in knob travel, not raw units, so Hz and ms ranges feel the same as %.
 const double step=static_cast<double>(wheel.deltaY)*(wheel.isReversed?-1.:1.)*(e.mods.isShiftDown()?.04:.25);
 const double proportion=juce::jlimit(0.,1.,valueToProportionOfLength(getValue())+step);
 setValue(proportionOfLengthToValue(proportion),juce::sendNotificationSync);
}

InversoAbismoEditor::InversoAbismoEditor(InversoAbismoProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 sections={
  {"OSCILADOR",{P::OscShape,P::OscLevel,P::Octave,P::Unison,P::Detune,P::Spread,P::SubLevel},{}},
  {"DISSONANCIA",{P::ClusterMode,P::ClusterLevel,P::NoiseLevel,P::Tension},{}},
  {"RESSONADOR",{P::ResMaterial,P::ResLevel,P::ResDecay,P::Bow,P::Scrape,P::Strike,P::NoiseTone},{}},
  {"FILTRO",{P::FilterMode,P::Cutoff,P::Resonance,P::FilterEnv,P::Keytrack,P::Drive},{}},
  {"AMPLITUDE",{P::Attack,P::Decay,P::Sustain,P::Release,P::VelSens},{}},
  {"ENVELOPE DO FILTRO",{P::FAttack,P::FDecay,P::FSustain,P::FRelease},{}},
  {"AFINACAO / MOVIMENTO",{P::PitchEnv,P::PitchTime,P::Drift,P::Glide,P::BendRange},{}},
  {"LFO",{P::LfoRate,P::LfoPitch,P::LfoFilter,P::LfoAmp},{}},
  {"ESPACO",{P::RevMix,P::RevSize,P::RevDecay,P::RevDamp,P::Freeze,P::Width,P::Output},{}},
 };
 static const char* shortNames[P::Count]={
  "SHAPE","LEVEL","OCTAVE","UNISON","DETUNE","SPREAD","SUB",
  "CLUSTER","CLUSTER LVL","BREATH","TENSION",
  "MATERIAL","LEVEL","DECAY","BOW","SCRAPE","STRIKE","EXC TONE",
  "TYPE","CUTOFF","RESO","ENV AMT","KEYTRACK","DRIVE",
  "ATTACK","DECAY","SUSTAIN","RELEASE","VELOCITY",
  "ATTACK","DECAY","SUSTAIN","RELEASE",
  "GLISSANDO","GLISS TIME","DRIFT","GLIDE","BEND",
  "RATE","PITCH","FILTER","AMP",
  "MIX","SIZE","DECAY","DARKNESS","FREEZE","WIDTH","OUTPUT"};
 for(const auto& section:sections){
  for(int param:section.params){
   const auto& spec=specs()[static_cast<std::size_t>(param)];
   Control c;
   c.param=param;
   c.label=std::make_unique<juce::Label>();
   c.label->setText(shortNames[param],juce::dontSendNotification);
   c.label->setJustificationType(juce::Justification::centred);
   c.label->setFont(juce::FontOptions(11.5f,juce::Font::bold));
   c.label->setTooltip(spec.name);
   addAndMakeVisible(*c.label);
   if(spec.kind==Kind::Choice){
    c.combo=std::make_unique<juce::ComboBox>();
    c.combo->addItemList(juce::StringArray::fromTokens(spec.choices,"|",""),1);
    addAndMakeVisible(*c.combo);
    c.comboAttachment=std::make_unique<CA>(p.apvts,spec.id,*c.combo);
   } else if(spec.kind==Kind::Bool){
    c.toggle=std::make_unique<juce::ToggleButton>("ON");
    addAndMakeVisible(*c.toggle);
    c.buttonAttachment=std::make_unique<BA>(p.apvts,spec.id,*c.toggle);
   } else {
    c.knob=std::make_unique<AbismoKnob>();
    c.knob->setTooltip(spec.name);
    addAndMakeVisible(*c.knob);
    c.sliderAttachment=std::make_unique<SA>(p.apvts,spec.id,*c.knob);
    // After the attachment, which installs the parameter's own text function.
    c.knob->textFromValueFunction=[&spec](double v){return formatValue(spec,v);};
    c.knob->updateText();
   }
   controls.push_back(std::move(c));
  }
 }
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
 setSize(1180,headerHeight+4*(knobHeight+34+sectionPad)+40);
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
 g.drawText("SINTETIZADOR DE TENSAO PARA TRILHA   /   MOD WHEEL = TENSION   /   AFTERTOUCH = BOW + FILTRO",24,48,760,18,juce::Justification::centredLeft);
 const float db=juce::Decibels::gainToDecibels(shownPeak,-90.f);
 g.setColour(shownPeak>.89f?juce::Colours::salmon:bone);
 g.setFont(12.f);
 g.drawText("VOZES "+juce::String(shownVoices)+"/16    PICO "+juce::String(db,1)+" dBFS",getWidth()-290,48,268,18,juce::Justification::centredRight);
 for(const auto& s:sections){
  g.setColour(panel);
  g.fillRoundedRectangle(s.bounds.toFloat(),7.f);
  g.setColour(blood.withAlpha(.85f));
  g.fillRect(s.bounds.getX()+10,s.bounds.getY()+9,3,13);
  g.setColour(text);g.setFont(juce::FontOptions(12.5f,juce::Font::bold));
  g.drawText(s.title,s.bounds.getX()+19,s.bounds.getY()+5,s.bounds.getWidth()-24,20,juce::Justification::centredLeft);
 }
 g.setColour(muted.withAlpha(.8f));g.setFont(11.f);
 g.drawText("SHIFT + RODA: ajuste fino   |   arraste vertical ou horizontal nos knobs   |   presets: menu ou < >",
  22,getHeight()-26,900,18,juce::Justification::centredLeft);
}

void InversoAbismoEditor::resized(){
 presetBox.setBounds(getWidth()-372,14,270,28);
 previous.setBounds(getWidth()-96,14,34,28);
 next.setBounds(getWidth()-56,14,34,28);
 // Flow layout: sections fill rows in declaration order, then each row is
 // stretched to the full width so the right edge lines up.
 const int left=16,maxRight=getWidth()-16,rowHeight=knobHeight+34;
 auto natural=[](const Section& s){int w=16;for(int p:s.params)w+=naturalWidth(p);return w;};
 std::vector<std::vector<Section*>> rows(1);
 int x=left;
 for(auto& s:sections){
  const int width=natural(s);
  if(x+width>maxRight && !rows.back().empty()){rows.emplace_back();x=left;}
  rows.back().push_back(&s);
  x+=width+sectionPad;
 }
 int y=headerHeight;
 for(auto& row:rows){
  int used=0;
  for(auto* s:row) used+=natural(*s);
  const int available=maxRight-left-sectionPad*static_cast<int>(row.size()-1);
  const float stretch=static_cast<float>(available)/static_cast<float>(used);
  x=left;
  for(std::size_t i=0;i<row.size();++i){
   const int width=i+1==row.size()?maxRight-x:juce::roundToInt(static_cast<float>(natural(*row[i]))*stretch);
   row[i]->bounds={x,y,width,rowHeight};
   x+=width+sectionPad;
  }
  y+=rowHeight+sectionPad;
 }
 std::size_t index=0;
 for(const auto& s:sections){
  const float scale=static_cast<float>(s.bounds.getWidth()-16)/static_cast<float>(natural(s)-16);
  float cx=static_cast<float>(s.bounds.getX()+8);
  for(std::size_t i=0;i<s.params.size();++i,++index){
   auto& c=controls[index];
   const int w=naturalWidth(c.param);
   const int slot=juce::roundToInt(static_cast<float>(w)*scale);
   const int px=juce::roundToInt(cx)+(slot-w)/2,cy=s.bounds.getY()+28;
   c.label->setBounds(px,cy,w,16);
   if(c.knob) c.knob->setBounds(px+2,cy+16,w-4,knobHeight-18);
   if(c.combo) c.combo->setBounds(px+3,cy+40,w-6,26);
   if(c.toggle) c.toggle->setBounds(px+12,cy+40,w-16,26);
   cx+=static_cast<float>(w)*scale;
  }
 }
}
