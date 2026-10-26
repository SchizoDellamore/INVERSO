#include "AbismoUI.h"
#include <algorithm>
#include <cmath>

using namespace inverso_abismo;

namespace abismo_ui {

namespace {
constexpr int knobWidth=70, choiceWidth=124, knobHeight=96, sectionPad=10, rowHeight=knobHeight+34;
}

namespace {
juce::CriticalSection& registryLock(){ static juce::CriticalSection l; return l; }
std::vector<MidiLearn*>& registry(){ static std::vector<MidiLearn*> r; return r; }
std::vector<const juce::AudioProcessor*>& owners(){ static std::vector<const juce::AudioProcessor*> o; return o; }
}

MidiLearn::MidiLearn(juce::AudioProcessor& p):processor(p){
 for(auto& m:map) m.store(-1);
 for(auto& v:pending) v.store(-1);
 { const juce::ScopedLock l(registryLock()); registry().push_back(this); owners().push_back(&p); }
 startTimerHz(60);
}
MidiLearn::~MidiLearn(){
 stopTimer();
 const juce::ScopedLock l(registryLock());
 for(std::size_t i=0;i<registry().size();++i) if(registry()[i]==this){ registry().erase(registry().begin()+static_cast<long>(i)); owners().erase(owners().begin()+static_cast<long>(i)); break; }
}
MidiLearn* MidiLearn::find(const juce::AudioProcessor* p){
 const juce::ScopedLock l(registryLock());
 for(std::size_t i=0;i<owners().size();++i) if(owners()[i]==p) return registry()[i];
 return nullptr;
}
int MidiLearn::learn(const juce::MidiMessage& m){
 if(!m.isController()) return -1;
 const int cc=m.getControllerNumber();
 if(cc<0 || cc>=120) return -1; // 120+ are channel mode messages
 int want=armed.load(std::memory_order_relaxed);
 if(want>=0 && armed.compare_exchange_strong(want,-1)){
  for(auto& slot:map){ int same=want; slot.compare_exchange_strong(same,-1); } // one CC per parameter
  map[static_cast<std::size_t>(cc)].store(want);
 }
 return cc;
}
void MidiLearn::handle(const juce::MidiMessage& m){
 const int cc=learn(m);
 if(cc>=0 && map[static_cast<std::size_t>(cc)].load(std::memory_order_relaxed)>=0) pending[static_cast<std::size_t>(cc)].store(m.getControllerValue(),std::memory_order_relaxed);
}
void MidiLearn::flush(){
 const auto& params=processor.getParameters();
 for(std::size_t cc=0;cc<map.size();++cc){
  const int v=pending[cc].exchange(-1,std::memory_order_relaxed);
  const int p=map[cc].load(std::memory_order_relaxed);
  if(v<0 || p<0 || p>=params.size()) continue;
  auto* param=params[p];
  param->beginChangeGesture();
  param->setValueNotifyingHost(static_cast<float>(v)/127.f);
  param->endChangeGesture();
 }
}
void MidiLearn::arm(int index){ armed.store(index); }
int MidiLearn::ccFor(int index) const{
 for(std::size_t cc=0;cc<map.size();++cc) if(map[cc].load()==index) return static_cast<int>(cc);
 return -1;
}
void MidiLearn::forget(int index){ for(auto& slot:map){ int same=index; slot.compare_exchange_strong(same,-1); } }
int MidiLearn::indexOf(const juce::String& id) const{
 const auto& params=processor.getParameters();
 for(int i=0;i<params.size();++i) if(auto* r=dynamic_cast<juce::AudioProcessorParameterWithID*>(params[i])) if(r->paramID==id) return i;
 return -1;
}
void MidiLearn::save(juce::ValueTree& state) const{
 juce::ValueTree learn("MIDI_LEARN");
 const auto& params=processor.getParameters();
 for(std::size_t cc=0;cc<map.size();++cc){
  const int p=map[cc].load();
  if(p<0 || p>=params.size()) continue;
  if(auto* r=dynamic_cast<juce::AudioProcessorParameterWithID*>(params[p])){
   juce::ValueTree item("CC"); item.setProperty("number",static_cast<int>(cc),nullptr); item.setProperty("param",r->paramID,nullptr);
   learn.appendChild(item,nullptr);
  }
 }
 auto old=state.getChildWithName("MIDI_LEARN");
 if(old.isValid()) state.removeChild(old,nullptr);
 state.appendChild(learn,nullptr);
}
void MidiLearn::load(const juce::ValueTree& state){
 for(auto& m:map) m.store(-1);
 const auto learn=state.getChildWithName("MIDI_LEARN");
 for(const auto& item:learn){
  const int cc=item.getProperty("number",-1);
  const int p=indexOf(item.getProperty("param").toString());
  if(cc>=0 && cc<120 && p>=0) map[static_cast<std::size_t>(cc)].store(p);
 }
}

juce::File userPresetFolder(const juce::String& plugin){
 return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("INVERSO").getChildFile(plugin).getChildFile("Presets");
}
juce::Array<juce::File> userPresets(const juce::String& plugin){
 auto files=userPresetFolder(plugin).findChildFiles(juce::File::findFiles,false,"*.xml");
 files.sort();
 return files;
}
bool saveUserPreset(juce::AudioProcessorValueTreeState& apvts,const juce::String& plugin,const juce::String& name){
 const auto clean=juce::File::createLegalFileName(name.trim());
 if(clean.isEmpty()) return false;
 const auto folder=userPresetFolder(plugin);
 folder.createDirectory();
 auto state=apvts.copyState();
 auto learn=state.getChildWithName("MIDI_LEARN");
 if(learn.isValid()) state.removeChild(learn,nullptr); // the knobs belong to the keyboard, not the sound
 auto xml=state.createXml();
 return xml!=nullptr && xml->writeTo(folder.getChildFile(clean+".xml"));
}
bool loadUserPreset(juce::AudioProcessorValueTreeState& apvts,const juce::File& file){
 const auto xml=juce::parseXML(file);
 if(xml==nullptr) return false;
 const auto state=juce::ValueTree::fromXml(*xml);
 bool any=false;
 for(const auto& child:state){
  if(!child.hasType("PARAM")) continue;
  if(auto* p=apvts.getParameter(child.getProperty("id").toString())){
   p->beginChangeGesture();
   p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(child.getProperty("value"))));
   p->endChangeGesture();
   any=true;
  }
 }
 return any;
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
 if(unit=="knob") return juce::String(v,1);
 if(unit=="q") return "Q "+juce::String(v,v<10.?2:1);
 if(unit=="oct") return (v>0?"+":"")+juce::String(juce::roundToInt(v))+" oct";
 if(unit=="n") return juce::String(juce::roundToInt(v));
 if(unit==":1") return juce::String(v,v<10.?1:0)+":1";
 if(unit=="hpf") return v<=10.5?juce::String("OFF"):formatValue(ParamSpec{"","",0,0,0,0,0,Kind::Float,nullptr,"Hz"},v);
 if(unit=="lpf") return v>=21900.?juce::String("OFF"):formatValue(ParamSpec{"","",0,0,0,0,0,Kind::Float,nullptr,"Hz"},v);
 if(unit=="off20") return v<=20.5?juce::String("OFF"):formatValue(ParamSpec{"","",0,0,0,0,0,Kind::Float,nullptr,"Hz"},v);
 if(unit=="thr") return juce::String(v,0)+" dB";
 if(unit=="gate") return v<=-89.9?juce::String("OFF"):juce::String(v,0)+" dB";
 if(unit=="ct") return (v>0?"+":"")+juce::String(v,0)+" ct";
 if(unit=="/s") return juce::String(v,v<10.?1:0)+" /s";
 if(*s.unit==0 && s.kind==Kind::Int) return juce::MidiMessage::getMidiNoteName(juce::roundToInt(v),true,true,3);
 return juce::String(v,0)+"%";
}

// What a DAW shows next to a value: the display-formatting keys of this file are not for it.
static juce::String hostLabel(const juce::String& unit){
 if(unit=="hpf"||unit=="lpf"||unit=="off20") return "Hz";
 if(unit=="q"||unit=="n") return {};
 return unit;
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
     AudioParameterFloatAttributes().withLabel(hostLabel(s.unit))));
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
