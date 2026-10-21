#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
namespace {
const auto bg=juce::Colour::fromRGB(16,19,23);
const auto panel=juce::Colour::fromRGB(22,27,31);
const auto ink=juce::Colour::fromRGB(228,220,207);
const auto muted=juce::Colour::fromRGB(151,164,161);
const auto mint=juce::Colour::fromRGB(114,173,165);
const auto orange=juce::Colour::fromRGB(216,154,103);
}
void InversoWheelSlider::mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel){
 if(!isEnabled() || wheel.deltaY==0)return;
 const double steps=juce::jlimit(-4.0,4.0,static_cast<double>(wheel.deltaY)*(wheel.isReversed?-5.0:5.0));
 const double amount=event.mods.isShiftDown()?0.1:0.5; // percent per step
 setValue(juce::jlimit(getMinimum(),getMaximum(),getValue()+steps*amount),juce::sendNotificationSync);
}
void InversoMeterPanel::paint(juce::Graphics& g){
 auto r=getLocalBounds().toFloat();g.setColour(panel);g.fillRoundedRectangle(r,9.f);
 g.setFont(12.f);
 const int count=processor.bandCount();
 const auto pad=22.f,spacing=10.f,usable=r.getWidth()-2*pad;
 const auto width=(usable-spacing*(count-1))/count;
 static const float mud[3]={160,275,440};
 static const float annoy[7]={1150,1600,2250,3150,4400,6100,8200};
 const float focus=processor.apvts.getRawParameterValue("FOCUS")->load()*0.01f;
 const float shift=std::pow(2.f,(focus-.5f)*.7f);
 for(int i=0;i<count;++i){
  auto reduction=juce::jlimit(0.f,12.f,processor.bandReductionDb[static_cast<std::size_t>(i)].load());
  float x=pad+i*(width+spacing);
  const auto meterY=52.f, meterHeight=75.f;
  g.setColour(juce::Colour::fromRGB(43,53,56));
  g.fillRoundedRectangle(x,meterY,width,meterHeight,3.f);
  if(reduction>0.03f){
   float h=(reduction/12.f)*meterHeight;
   g.setColour(processor.isMudder?mint:orange);
   g.fillRoundedRectangle(x,meterY+meterHeight-h,width,h,3.f);
  }
  const auto centre=(processor.isMudder?mud[i]:annoy[i])*shift;
  const auto text=centre>=1000.f?juce::String(centre/1000.f,1)+"k":juce::String(static_cast<int>(centre));
  g.setColour(muted);g.drawFittedText(text,juce::Rectangle<int>(static_cast<int>(x-8),29,static_cast<int>(width+16),18),juce::Justification::centred,1);
  g.setColour(ink);g.drawFittedText("-"+juce::String(reduction,1),juce::Rectangle<int>(static_cast<int>(x-8),138,static_cast<int>(width+16),20),juce::Justification::centred,1);
 }
 g.setColour(muted);g.drawText("DYNAMIC REDUCTION  ·  dB",juce::Rectangle<int>(18,4,340,20),juce::Justification::centredLeft);
 g.setColour(ink);
 g.drawText("MAX  -"+juce::String(processor.reductionDb.load(),1)+" dB",juce::Rectangle<int>(static_cast<int>(r.getWidth())-190,4,165,20),juce::Justification::centredRight);
}
InversoProcessorEditor::InversoProcessorEditor(InversoProcessor& p):AudioProcessorEditor(&p),processor(p),meter(p){
 setSize(700,370);
 const char* keys[]={"AMOUNT","SENSITIVITY","FOCUS"};
 const char* names[]={"AMOUNT","SENSITIVITY","FOCUS"};
 for(int i=0;i<3;++i){
  auto& s=sliders[static_cast<std::size_t>(i)];
  s.setSliderStyle(juce::Slider::LinearHorizontal);
  s.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,22);
  s.setColour(juce::Slider::trackColourId,mint);
  s.setColour(juce::Slider::thumbColourId,ink);
  s.setColour(juce::Slider::backgroundColourId,panel);
  s.textFromValueFunction=[](double v){return juce::String(v,1)+"%";};
  addAndMakeVisible(s);
  auto& l=labels[static_cast<std::size_t>(i)];l.setText(names[i],juce::dontSendNotification);l.setColour(juce::Label::textColourId,muted);l.setFont(12.f);addAndMakeVisible(l);
  attachments[static_cast<std::size_t>(i)]=std::make_unique<SA>(p.apvts,keys[i],s);
 }
 addAndMakeVisible(bypass);addAndMakeVisible(delta);addAndMakeVisible(meter);
 bypassAttach=std::make_unique<BA>(p.apvts,"BYPASS",bypass);
 deltaAttach=std::make_unique<BA>(p.apvts,"DELTA",delta);
 bypass.setColour(juce::ToggleButton::textColourId,muted);
 delta.setColour(juce::ToggleButton::textColourId,muted);
}
void InversoProcessorEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(ink);g.setFont(juce::Font(24.f,juce::Font::bold));
 g.drawText(processor.isMudder?"INVERSO  /  DE-MUDDER":"INVERSO  /  DE-ANNOYANCE",22,16,560,40,juce::Justification::centredLeft);
 g.setFont(12.f);g.setColour(muted);
 g.drawText(processor.isMudder?"Dynamic low-mid cleanup":"Dynamic presence and resonance control",24,52,560,25,juce::Justification::centredLeft);
 g.setColour(muted.withAlpha(.7f));g.drawText("SHIFT + WHEEL  ·  FINE ADJUSTMENT",22,340,350,18,juce::Justification::centredLeft);
}
void InversoProcessorEditor::resized(){
 meter.setBounds(22,86,getWidth()-44,170);
 for(int i=0;i<3;++i){
  const auto x=24+i*224;
  labels[static_cast<std::size_t>(i)].setBounds(x,264,210,16);
  sliders[static_cast<std::size_t>(i)].setBounds(x,280,205,36);
 }
 bypass.setBounds(465,332,105,22);delta.setBounds(565,332,140,22);
}
