#include "AbismoUI.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo;

namespace abismo_ui {

namespace {
constexpr int knobWidth=70, choiceWidth=124, knobHeight=96, sectionPad=10, rowHeight=knobHeight+34;
}

juce::String formatValue(const ParamSpec& s,double v){
 const juce::String unit(s.unit);
 if(unit=="Hz") return v>=1000.?juce::String(v/1000.,2)+" kHz":juce::String(v,v<10.?2:0)+" Hz";
 if(unit=="ms") return v>=1000.?juce::String(v/1000.,2)+" s":juce::String(v,0)+" ms";
 if(unit=="s") return juce::String(v,v<10.?2:1)+" s";
 if(unit=="dB") return juce::String(v,1)+" dB";
 if(unit=="st") return (v>0?"+":"")+juce::String(v,s.kind==Kind::Int?0:1)+" st";
 if(unit=="pan") return std::abs(v)<.5?juce::String("C"):(v<0?"L ":"R ")+juce::String(std::abs(v),0);
 if(unit=="cc") return v<.5?juce::String("OFF"):"CC "+juce::String(juce::roundToInt(v));
 if(unit=="ct") return (v>0?"+":"")+juce::String(v,0)+" ct";
 if(unit=="/s") return juce::String(v,v<10.?1:0)+" /s";
 if(*s.unit==0 && s.kind==Kind::Int) return juce::MidiMessage::getMidiNoteName(juce::roundToInt(v),true,true,3);
 return juce::String(v,0)+"%";
}

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(const ParamSpec* specs,int count){
 using namespace juce;
 std::vector<std::unique_ptr<RangedAudioParameter>> p;
 for(int i=0;i<count;++i){
  const auto& s=specs[i];
  switch(s.kind){
   case Kind::Choice:
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{s.id,1},s.name,
     StringArray::fromTokens(s.choices,"|",""),static_cast<int>(s.def)));
    break;
   case Kind::Bool:
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{s.id,1},s.name,s.def>=.5f));
    break;
   case Kind::Int:
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{s.id,1},s.name,
     static_cast<int>(s.min),static_cast<int>(s.max),static_cast<int>(s.def)));
    break;
   case Kind::Float: {
    NormalisableRange<float> range(s.min,s.max,s.interval);
    if(s.centre>0.f) range.setSkewForCentre(s.centre);
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{s.id,1},s.name,range,s.def,
     AudioParameterFloatAttributes().withLabel(s.unit)));
    break;
   }
  }
 }
 return {p.begin(),p.end()};
}

Look::Look(){
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

void Look::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&){
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

Knob::Knob(){
 setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
 setTextBoxStyle(juce::Slider::TextBoxBelow,false,knobWidth,18);
 setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
 setDoubleClickReturnValue(false,0.0);
 setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
 setColour(juce::Slider::textBoxTextColourId,text);
}
void Knob::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
 if(!isEnabled() || std::abs(wheel.deltaY)<1e-6f) return;
 // Step in knob travel, not raw units, so Hz and ms ranges feel the same as %.
 const double step=static_cast<double>(wheel.deltaY)*(wheel.isReversed?-1.:1.)*(e.mods.isShiftDown()?.04:.25);
 const double proportion=juce::jlimit(0.,1.,valueToProportionOfLength(getValue())+step);
 setValue(proportionOfLengthToValue(proportion),juce::sendNotificationSync);
}

ParameterPanel::ParameterPanel(juce::AudioProcessorValueTreeState& apvts,const ParamSpec* table,
                               std::vector<Section> sectionList,const char* const* shortNames)
 :specs(table),sections(std::move(sectionList)){
 for(const auto& section:sections){
  for(int param:section.params){
   const auto& spec=specs[param];
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
    c.comboAttachment=std::make_unique<CA>(apvts,spec.id,*c.combo);
   } else if(spec.kind==Kind::Bool){
    c.toggle=std::make_unique<juce::ToggleButton>("ON");
    addAndMakeVisible(*c.toggle);
    c.buttonAttachment=std::make_unique<BA>(apvts,spec.id,*c.toggle);
   } else {
    c.knob=std::make_unique<Knob>();
    c.knob->setTooltip(spec.name);
    addAndMakeVisible(*c.knob);
    c.sliderAttachment=std::make_unique<SA>(apvts,spec.id,*c.knob);
    // After the attachment, which installs the parameter's own text function.
    c.knob->textFromValueFunction=[&spec](double v){return formatValue(spec,v);};
    c.knob->updateText();
   }
   controls.push_back(std::move(c));
  }
 }
}

void ParameterPanel::dimInactive(const std::function<bool(int)>& isActive){
 for(auto& c:controls){
  const float alpha=isActive(c.param)?1.f:.32f;
  for(juce::Component* comp:{static_cast<juce::Component*>(c.label.get()),static_cast<juce::Component*>(c.knob.get()),
                             static_cast<juce::Component*>(c.combo.get()),static_cast<juce::Component*>(c.toggle.get())})
   if(comp!=nullptr && std::abs(comp->getAlpha()-alpha)>.01f) comp->setAlpha(alpha);
 }
}

int ParameterPanel::naturalWidth(int param) const {return specs[param].kind==Kind::Choice?choiceWidth:knobWidth;}
int ParameterPanel::naturalWidth(const Section& s) const {int w=16;for(int p:s.params)w+=naturalWidth(p);return w;}

// Flow layout: sections fill rows in declaration order, then each row is
// stretched to the full width so the right edge lines up.
std::vector<juce::Rectangle<int>> ParameterPanel::layoutSections(int width) const {
 std::vector<juce::Rectangle<int>> result(sections.size());
 std::vector<std::vector<std::size_t>> rows(1);
 int x=0;
 for(std::size_t i=0;i<sections.size();++i){
  const int w=naturalWidth(sections[i]);
  if(x+w>width && !rows.back().empty()){rows.emplace_back();x=0;}
  rows.back().push_back(i);
  x+=w+sectionPad;
 }
 int y=0;
 for(auto& row:rows){
  int used=0;
  for(auto i:row) used+=naturalWidth(sections[i]);
  const int available=width-sectionPad*static_cast<int>(row.size()-1);
  const float stretch=static_cast<float>(available)/static_cast<float>(used);
  x=0;
  for(std::size_t j=0;j<row.size();++j){
   const int w=j+1==row.size()?width-x:juce::roundToInt(static_cast<float>(naturalWidth(sections[row[j]]))*stretch);
   result[row[j]]={x,y,w,rowHeight};
   x+=w+sectionPad;
  }
  y+=rowHeight+sectionPad;
 }
 return result;
}

int ParameterPanel::heightForWidth(int width) const {
 int bottom=0;
 for(const auto& r:layoutSections(width)) bottom=std::max(bottom,r.getBottom());
 return bottom;
}

void ParameterPanel::paint(juce::Graphics& g){
 for(std::size_t i=0;i<sections.size() && i<bounds.size();++i){
  const auto& b=bounds[i];
  g.setColour(panel);
  g.fillRoundedRectangle(b.toFloat(),7.f);
  g.setColour(blood.withAlpha(.85f));
  g.fillRect(b.getX()+10,b.getY()+9,3,13);
  g.setColour(text);g.setFont(juce::FontOptions(12.5f,juce::Font::bold));
  g.drawText(sections[i].title,b.getX()+19,b.getY()+5,b.getWidth()-24,20,juce::Justification::centredLeft);
 }
}

void ParameterPanel::resized(){
 bounds=layoutSections(getWidth());
 std::size_t index=0;
 for(std::size_t si=0;si<sections.size();++si){
  const auto& s=sections[si];
  const auto& b=bounds[si];
  const float scale=static_cast<float>(b.getWidth()-16)/static_cast<float>(naturalWidth(s)-16);
  float cx=static_cast<float>(b.getX()+8);
  for(std::size_t i=0;i<s.params.size();++i,++index){
   auto& c=controls[index];
   const int w=naturalWidth(c.param);
   const int slot=juce::roundToInt(static_cast<float>(w)*scale);
   const int px=juce::roundToInt(cx)+(slot-w)/2,cy=b.getY()+28;
   c.label->setBounds(px,cy,w,16);
   if(c.knob) c.knob->setBounds(px+2,cy+16,w-4,knobHeight-18);
   if(c.combo) c.combo->setBounds(px+3,cy+40,w-6,26);
   if(c.toggle) c.toggle->setBounds(px+12,cy+40,w-16,26);
   cx+=static_cast<float>(w)*scale;
  }
 }
}

} // namespace abismo_ui
