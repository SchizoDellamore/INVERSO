#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
namespace {
const auto bg=juce::Colour::fromRGB(14,17,21);
const auto panel=juce::Colour::fromRGB(24,28,33);
const auto text=juce::Colour::fromRGB(233,224,206);
const auto muted=juce::Colour::fromRGB(154,166,164);
const auto teal=juce::Colour::fromRGB(155,147,131);
const auto orange=juce::Colour::fromRGB(240,133,42);
}
void WheelSlider::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
 if(!isEnabled() || wheel.deltaY==0)return;
 const double change=static_cast<double>(wheel.deltaY)*(wheel.isReversed?-1.:1.)*(e.mods.isShiftDown()?.15:1.5);
 setValue(juce::jlimit(getMinimum(),getMaximum(),getValue()+change),juce::sendNotificationSync);
}
InversoBassDriveEditor::InversoBassDriveEditor(InversoBassDriveProcessor& p)
 :AudioProcessorEditor(&p),processor(p){
 setSize(780,570);
 const char* ids[]={"DRIVE","BODY","CROSSOVER","MIX","OUTPUT","FUZZ","TONE"};
 const char* names[]={"DRIVE","BODY","CROSSOVER","MIX","OUTPUT","FUZZ AMOUNT","FUZZ TONE"};
 for(int i=0;i<7;++i){
  auto& s=sliders[static_cast<size_t>(i)];
  s.setSliderStyle(juce::Slider::LinearHorizontal);
  s.setTextBoxStyle(juce::Slider::TextBoxRight,false,76,24);
  s.setColour(juce::Slider::trackColourId,(i==0||i==1||i==5)?orange:teal);
  s.setColour(juce::Slider::thumbColourId,text);
  s.setColour(juce::Slider::backgroundColourId,juce::Colour::fromRGB(51,58,62));
  s.setColour(juce::Slider::textBoxTextColourId,text);
  s.setColour(juce::Slider::textBoxOutlineColourId,panel);
  if(i==2)s.textFromValueFunction=[](double v){return juce::String(v,0)+" Hz";};
  else if(i==4)s.textFromValueFunction=[](double v){return juce::String(v,1)+" dB";};
  else s.textFromValueFunction=[](double v){return juce::String(v,1)+"%";};
  addAndMakeVisible(s);
  auto& label=labels[static_cast<size_t>(i)];
  label.setText(names[i],juce::dontSendNotification);
  label.setFont(juce::Font(13.f,juce::Font::bold));
  label.setColour(juce::Label::textColourId,muted);
  addAndMakeVisible(label);
  attachments[static_cast<size_t>(i)]=std::make_unique<SA>(p.apvts,ids[i],s);
 }
 modeLabel.setText("FUZZ CHARACTER",juce::dontSendNotification);
 modeLabel.setColour(juce::Label::textColourId,muted);
 modeLabel.setFont(juce::Font(13.f,juce::Font::bold));
 addAndMakeVisible(modeLabel);
 modes.addItem("Off - Warm drive",1);
 modes.addItem("Vintage - Asymmetric",2);
 modes.addItem("Silicon - Aggressive",3);
 modes.addItem("Gated - Broken",4);
 modes.setColour(juce::ComboBox::backgroundColourId,panel);
 modes.setColour(juce::ComboBox::textColourId,text);
 modes.setColour(juce::ComboBox::outlineColourId,muted.withAlpha(.3f));
 addAndMakeVisible(modes);
 modeAttachment=std::make_unique<CA>(p.apvts,"FUZZ_MODE",modes);
 lowProtect.setColour(juce::ToggleButton::textColourId,text);
 addAndMakeVisible(lowProtect);
 bypass.setColour(juce::ToggleButton::textColourId,text);
 addAndMakeVisible(bypass);
 bypassAttachment=std::make_unique<BA>(p.apvts,"BYPASS",bypass);
 lowProtectAttachment=std::make_unique<BA>(p.apvts,"LOW_SAFE",lowProtect);
 startTimerHz(18);
}
void InversoBassDriveEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);
 g.setFont(juce::Font(27.f,juce::Font::bold));
 g.drawText("INVERSO   /   BASS DRIVE",22,13,620,44,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(12.5f);
 g.drawText("HARMONIC DRIVE   /   BODY   /   OPTIONAL FUZZ   /   ZERO LATENCY",24,55,700,23,juce::Justification::centredLeft);
 const auto visual=juce::Rectangle<float>(20.f,86.f,740.f,96.f);
 g.setColour(panel);g.fillRoundedRectangle(visual,8.f);
 const auto crossover=processor.apvts.getRawParameterValue("CROSSOVER")->load();
 const float boundary=juce::jmap(juce::jlimit(60.f,500.f,crossover),60.f,500.f,190.f,354.f);
 g.setColour(teal.withAlpha(.12f));g.fillRoundedRectangle(30.f,97.f,boundary-30.f,58.f,4.f);
 g.setColour(orange.withAlpha(.12f));g.fillRoundedRectangle(boundary,97.f,750.f-boundary,58.f,4.f);
 g.setColour(teal);g.fillRoundedRectangle(39.f,133.f,boundary-48.f,4.f,2.f);
 g.setColour(orange);g.fillRoundedRectangle(boundary+8.f,133.f,739.f-boundary-17.f,4.f,2.f);
 g.setColour(text);g.setFont(13.f);
 g.drawText("CLEAN LOW",42,103,150,22,juce::Justification::centredLeft);
 g.drawText("DRIVE   /   OPTIONAL FUZZ",static_cast<int>(boundary+19),103,200,22,juce::Justification::centredLeft);
 const auto out=processor.outputPeak.load();
 const auto in=processor.inputPeak.load();
 const auto inter=processor.oversampledPeak.load();
 g.setColour(muted);g.setFont(12.f);
 g.drawText("IN  "+juce::String(juce::Decibels::gainToDecibels(in,-100.f),1)+" dBFS",33,155,160,24,juce::Justification::centredLeft);
 const auto nearClip=inter>=1.f || out>=1.f;
 g.setColour(nearClip?juce::Colours::salmon:teal);
 g.drawText("OUT  "+juce::String(juce::Decibels::gainToDecibels(out,-100.f),1)+" dBFS"+(nearClip?"  /  PEAK ALERT":""),540,155,205,24,juce::Justification::centredRight);
 g.setColour(muted);g.setFont(11.5f);
 g.setColour(orange);
 g.setFont(13.f);
 g.drawText("DRIVE ENGINE",24,191,260,22,juce::Justification::centredLeft);
 g.setColour(muted);
 g.drawText("FUZZ  /  OPTIONAL",24,378,260,22,juce::Justification::centredLeft);
 g.setFont(11.5f);
 g.drawText("SHIFT + WHEEL: finer adjustment   |   Reduce OUTPUT if peak warning appears",23,545,690,17,juce::Justification::centredLeft);
}
void InversoBassDriveEditor::resized(){
 const int xx[]={24,272,520,24,272,24,272};
 const int yy[]={218,218,218,300,300,406,406};
 for(int i=0;i<7;++i){
  labels[static_cast<size_t>(i)].setBounds(xx[i],yy[i],228,20);
  sliders[static_cast<size_t>(i)].setBounds(xx[i],yy[i]+22,225,32);
 }
 modeLabel.setBounds(521,401,235,20);
 modes.setBounds(521,425,235,31);
 lowProtect.setBounds(25,503,190,34);
 bypass.setBounds(649,503,110,34);
}
