#include "DrumsEditor.h"
#include <cmath>

using namespace inverso_abismo::drums;
namespace c = soft::colour;

namespace {
constexpr int designW=1320, designH=1000;
juce::String noteName(int note){ return juce::MidiMessage::getMidiNoteName(note,true,true,3); }

const char* const* shortNames(){
 static std::array<const char*,P::Count> names=[]{
  std::array<const char*,P::Count> n{};
  static const char* padNames[P::PadCount]={"OBJECT","TUNE","DECAY","TONE","RATTLE","RUST",
                                            "VARIATION","LEVEL","PAN"};
  for(int pad=0;pad<numPads;++pad) for(int k=0;k<P::PadCount;++k) n[static_cast<std::size_t>(P::padBase(pad)+k)]=padNames[k];
  const std::pair<int,const char*> rest[]={
   {P::Stumble,"STUMBLE"},{P::StutterDiv,"DIVISION"},{P::StutterDepth,"DEPTH"},{P::Chaos,"CHAOS"},{P::Crush,"BITS"},
   {P::Megaphone,"MEGAPHONE"},{P::Gramophone,"GRAMOPHONE"},{P::RoomMix,"MIX"},{P::RoomSize,"SIZE"},{P::RoomDecay,"DECAY"},
   {P::RoomDamp,"DARKNESS"},{P::SawLevel,"LEVEL"},{P::SawAttack,"BOW"},{P::SawRelease,"RELEASE"},{P::SawVibrato,"VIBRATO"},
   {P::SawVibRate,"RATE"},{P::SawGlide,"GLIDE"},{P::SawTone,"NOISE"},{P::SawWobble,"WOBBLE"},{P::VelSens,"VELOCITY"},
   {P::Output,"OUTPUT"},{P::StutterSync,"TIMING"},{P::StutterMs,"TIME"}};
  for(const auto& [i,s]:rest) n[static_cast<std::size_t>(i)]=s;
  return n;
 }();
 return names.data();
}

juce::Path curve(juce::Rectangle<float> r,int points,const std::function<float(float)>& f){
 juce::Path p;
 for(int i=0;i<=points;++i){
  const float t=static_cast<float>(i)/static_cast<float>(points);
  const float y=juce::jlimit(-1.2f,1.2f,f(t));
  const juce::Point<float> pt(r.getX()+t*r.getWidth(),r.getCentreY()-y*r.getHeight()*.5f);
  if(i==0) p.startNewSubPath(pt); else p.lineTo(pt);
 }
 return p;
}
void caption(juce::Graphics& g,juce::Rectangle<float> r,const juce::String& left,const juce::String& right={}){
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left,r.withHeight(12.f),juce::Justification::topLeft);
 if(right.isNotEmpty()) g.drawText(right,r.withHeight(12.f),juce::Justification::topRight);
}
}

// ---------------------------------------------------------------- pads

void PadGrid::setPad(int pad,const juce::String& name,bool user,bool lost){
 auto& k=keys[static_cast<std::size_t>(pad)];
 if(k.name!=name || k.user!=user || k.missing!=lost){ k.name=name; k.user=user; k.missing=lost; repaint(); }
}
void PadGrid::setSelected(int pad){ if(pad!=selected){ selected=pad; repaint(); } }
void PadGrid::flash(int pad){ keys[static_cast<std::size_t>(pad)].light=1.f; repaint(); }
void PadGrid::decay(){
 bool any=false;
 for(auto& k:keys) if(k.light>0.f){ k.light=std::max(0.f,k.light-.09f); any=true; }
 if(any) repaint();
}
void PadGrid::setDropping(int pad){ if(pad!=dropping){ dropping=pad; repaint(); } }

juce::Rectangle<float> PadGrid::keyBounds(int pad) const {
 // Pad 1 bottom-left, like a drum machine: rows run upward.
 const auto area=getLocalBounds().toFloat().reduced(10.f).reduced(18.f,14.f).withTrimmedTop(40.f);
 const float gap=14.f,w=(area.getWidth()-3.f*gap)/4.f,h=(area.getHeight()-3.f*gap)/4.f;
 const int col=pad%4,row=3-pad/4;
 return {area.getX()+static_cast<float>(col)*(w+gap),area.getY()+static_cast<float>(row)*(h+gap),w,h};
}
int PadGrid::padAt(juce::Point<int> p) const {
 for(int i=0;i<numPads;++i) if(keyBounds(i).contains(p.toFloat())) return i;
 return -1;
}
void PadGrid::mouseDown(const juce::MouseEvent& e){
 const int pad=padAt(e.getPosition());
 if(pad>=0 && onPress) onPress(pad);
}
void PadGrid::paint(juce::Graphics& g){
 const float scale=g.getInternalContext().getPhysicalPixelScaleFactor();
 const int w=juce::roundToInt(static_cast<float>(getWidth())*scale),h=juce::roundToInt(static_cast<float>(getHeight())*scale);
 if(cardCache.getWidth()!=w || cardCache.getHeight()!=h){
  // Card and the raised keys' shadows rarely change: draw them once.
  cardCache=juce::Image(juce::Image::ARGB,std::max(1,w),std::max(1,h),true);
  juce::Graphics cg(cardCache);
  cg.addTransform(juce::AffineTransform::scale(scale));
  soft::paintCard(cg,getLocalBounds().toFloat().reduced(10.f));
  for(int i=0;i<numPads;++i){
   juce::Path p;
   p.addRoundedRectangle(keyBounds(i),12.f);
   juce::DropShadow(c::shadow.withAlpha(.5f),14,{4,6}).drawForPath(cg,p);
   juce::DropShadow(juce::Colours::white,10,{-4,-4}).drawForPath(cg,p);
  }
 }
 g.drawImageTransformed(cardCache,juce::AffineTransform::scale(1.f/scale));
 const auto inner=getLocalBounds().reduced(10).reduced(18,14);
 g.setColour(c::blood);
 g.fillEllipse(static_cast<float>(inner.getX()),static_cast<float>(inner.getY())+6.f,8.f,8.f);
 g.setColour(c::ink);
 g.setFont(soft::uiFont(12.5f,true).withExtraKerningFactor(.05f));
 g.drawText("PADS",inner.getX()+16,inner.getY(),200,20,juce::Justification::centredLeft);
 g.setColour(c::muted);
 g.setFont(soft::uiFont(10.f));
 g.drawText(juce::String::fromUTF8("CLICK TO HEAR  \xc2\xb7  DROP FILES ON A PAD"),inner.withHeight(20),juce::Justification::centredRight);
 for(int i=0;i<numPads;++i){
  const auto r=keyBounds(i);
  const auto& k=keys[static_cast<std::size_t>(i)];
  const bool sel=i==selected;
  g.setGradientFill(juce::ColourGradient(sel?c::card:juce::Colours::white,r.getX(),r.getY(),c::card.darker(.03f),r.getX(),r.getBottom(),false));
  g.fillRoundedRectangle(r,12.f);
  if(k.light>0.f){
   g.setColour(c::blood.withAlpha(.22f*k.light));
   g.fillRoundedRectangle(r,12.f);
   g.setColour(c::blood.withAlpha(.6f*k.light));
   g.drawRoundedRectangle(r.reduced(1.f),12.f,2.f);
  }
  if(sel || i==dropping){
   g.setColour(i==dropping?c::amber:c::blood);
   g.drawRoundedRectangle(r.reduced(1.5f),12.f,2.5f);
  }
  auto text=r.reduced(12.f,10.f);
  g.setFont(soft::monoFont(10.f));
  g.setColour(c::muted);
  g.drawText(noteName(firstPadNote+i)+"  "+juce::String(i+1),text.withHeight(12.f),juce::Justification::topLeft);
  if(k.user || k.missing){
   g.setColour(k.missing?c::red:c::amber);
   g.fillEllipse(text.getRight()-7.f,text.getY()+2.f,7.f,7.f);
  }
  g.setColour(c::ink);
  g.setFont(soft::uiFont(12.5f,true));
  g.drawFittedText(k.name.toUpperCase(),text.withTrimmedTop(16.f).toNearestInt(),juce::Justification::bottomLeft,2,.85f);
 }
}

// ---------------------------------------------------------------- editor

float InversoAbismoDrumsEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

InversoAbismoDrumsEditor::InversoAbismoDrumsEditor(InversoAbismoDrumsProcessor& p):AudioProcessorEditor(&p),processor(p){
 setLookAndFeel(&look);
 body.title="ABISMO DRUMS";
 body.subtitle=juce::String::fromUTF8("RECORDED SCRAPYARD   \xc2\xb7   PADS C1 TO D#2   \xc2\xb7   SAW FROM C3 UP (LEGATO SLIDES)");
 addAndMakeVisible(body);
 kits.getCount=[this]{return processor.getNumPrograms();};
 kits.getCurrent=[this]{return processor.getCurrentProgram();};
 kits.getName=[this](int i){return processor.getProgramName(i);};
 kits.select=[this](int i){processor.setCurrentProgram(i);};
 body.addAndMakeVisible(kits);
 grid.onPress=[this](int pad){ selectPad(pad); processor.auditionPad(pad); };
 body.addAndMakeVisible(grid);

 using Page=soft::Module::Page;
 for(int i=0;i<numPads;++i){
  const int b=P::padBase(i);
  auto m=std::make_unique<soft::Module>(processor.apvts,specs().data(),shortNames(),
   "PAD "+juce::String(i+1)+juce::String::fromUTF8("  \xc2\xb7  ")+noteName(firstPadNote+i),c::blood,
   std::vector<Page>{{"Sound",{b+P::Tune,b+P::Decay,b+P::Tone,b+P::Level,b+P::Pan},{b+P::Tune}},
                     {"Dirt",{b+P::Rattle,b+P::Rust,b+P::Variation}}},270,std::vector<int>{b+P::Object});
  m->screen.draw=[this,i](juce::Graphics& g,juce::Rectangle<float> r,int){ drawPadScreen(g,r,i); };
  body.addChildComponent(*m);
  padModules.push_back(std::move(m));
 }
 auto add=[&](juce::String title,juce::Colour accent,std::vector<Page> pages,int screenH,std::vector<int> choices)->soft::Module&{
  modules.push_back(std::make_unique<soft::Module>(processor.apvts,specs().data(),shortNames(),title,accent,std::move(pages),screenH,std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 add("BROKEN MACHINE",c::orange,{{"Chop",{P::Stumble,P::StutterMs,P::StutterDepth,P::Chaos,P::Crush}}},166,{P::StutterSync,P::StutterDiv});
 add("CHARACTER",c::pink,{{"Speaker",{P::Megaphone,P::Gramophone}}},166,{});
 add("ROOM",c::blue.interpolatedWith(c::violet,.5f),{{"Room",{P::RoomMix,P::RoomSize,P::RoomDecay,P::RoomDamp},{P::RoomMix}}},166,{});
 add("MUSICAL SAW",c::amber,{{"Bow",{P::SawLevel,P::SawAttack,P::SawRelease,P::SawTone}},{"Vibrato",{P::SawVibrato,P::SawVibRate,P::SawGlide,P::SawWobble}}},166,{});
 master=std::make_unique<soft::Master>(processor.apvts,specs()[P::Output],&specs()[P::VelSens],juce::String("VELOCITY"),c::ink);
 body.addAndMakeVisible(*master);

 loadButton.setTooltip("One file, or several (they become dynamic layers)");
 clearButton.setTooltip("Remove your sample and go back to the pad's OBJECT");
 loadButton.onClick=[this]{ chooseFiles(); };
 clearButton.onClick=[this]{ loadError.clear(); processor.clearPadSample(selected); };
 body.addAndMakeVisible(loadButton);
 body.addAndMakeVisible(clearButton);

 auto& m=modules;
 m[0]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  auto area=r.withTrimmedBottom(34.f);
  const int div=juce::roundToInt(value(P::StutterDiv));
  const bool free=juce::roundToInt(value(P::StutterSync))==1;
  const float depth=value(P::StutterDepth)*.01f,chaos=value(P::Chaos)*.01f,stumble=value(P::Stumble)*.01f;
  caption(g,area,juce::String("STUMBLE ")+juce::String(juce::roundToInt(stumble*80.f))+" ms",
          div==0?juce::String("STUTTER OFF"):free?abismo_ui::formatValue(specs()[P::StutterMs],value(P::StutterMs))
          :juce::String(juce::StringArray{"","1/4","1/8","1/16","1/32","1/8T","1/16T"}[div]));
  auto steps=area.withTrimmedTop(18.f);
  const float w=steps.getWidth()/16.f;
  juce::Random rnd(99);
  for(int i=0;i<16;++i){
   const float a=rnd.nextFloat(),late=rnd.nextFloat()*stumble;
   auto cell=juce::Rectangle<float>(steps.getX()+w*static_cast<float>(i),steps.getY(),w,steps.getHeight()).reduced(2.f,4.f);
   g.setColour(c::screenMuted.withAlpha(.18f));
   g.fillRoundedRectangle(cell,2.f);
   const bool dropped=div>0 && a<chaos*.45f,repeated=div>0 && !dropped && a<chaos*.9f;
   const float open=div>0?1.f-.85f*depth:1.f;
   auto lit=cell.withTrimmedLeft(cell.getWidth()*late*.5f);
   lit=lit.withWidth(lit.getWidth()*open);
   g.setColour(c::orange.withAlpha(dropped?.25f:repeated?.55f:.9f));
   g.fillRoundedRectangle(lit,2.f);
  }
 };
 m[1]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mega=value(P::Megaphone)*.01f,gram=value(P::Gramophone)*.01f;
  caption(g,r,"MEGAPHONE "+juce::String(juce::roundToInt(mega*100.f))+"%","78 RPM "+juce::String(juce::roundToInt(gram*100.f))+"%");
  auto plot=r.withTrimmedTop(16.f);
  // The speaker's band: narrower and more nasal with the megaphone, duller with the record.
  soft::glowStroke(g,curve(plot,160,[&](float t){
   const float f=std::pow(1000.f,t)*20.f;
   const float band=std::exp(-std::pow(std::log2(f/1500.f),2.f)*(.15f+mega*.9f));
   const float flat=1.f-mega;
   const float roll=1.f/(1.f+std::pow(f/(12000.f-gram*8500.f),4.f));
   return (flat+mega*band*1.4f)*roll*1.2f-.6f;
  }),c::pink);
 };
 m[2]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float mix=value(P::RoomMix)*.01f,decay=value(P::RoomDecay),dark=value(P::RoomDamp)*.01f;
  caption(g,r,abismo_ui::formatValue(specs()[P::RoomDecay],decay),"MIX "+juce::String(juce::roundToInt(mix*100.f))+"%");
  const auto colour=c::blue.interpolatedWith(c::violet,.5f).interpolatedWith(c::screenMuted,dark*.5f);
  const float span=juce::jlimit(.05f,1.f,std::log10(1.f+decay)/1.2f);
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),160,[&](float t){return .8f*std::exp(-t/(span*.35f))*2.f-1.f;}),colour.withAlpha(mix>0.f?1.f:.35f));
 };
 m[3]->screen.draw=[this](juce::Graphics& g,juce::Rectangle<float> r,int){
  const float vib=value(P::SawVibrato)*.01f,rate=value(P::SawVibRate),wob=value(P::SawWobble)*.01f;
  caption(g,r,"VIBRATO "+juce::String(juce::roundToInt(vib*100.f))+"%",abismo_ui::formatValue(specs()[P::SawVibRate],rate));
  const float level=value(P::SawLevel)*.01f;
  soft::glowStroke(g,curve(r.withTrimmedTop(14.f),200,[&](float t){
   const float env=std::min(1.f,t*4.f)*level;
   return env*.25f*std::sin((t*rate*1.2f-animation*rate*.2f)*6.2831853f)*vib*3.f+wob*.15f*std::sin(t*5.f+animation)+.0f;
  }),c::amber.withAlpha(level>0.f?1.f:.35f));
 };

 setResizable(true,true);
 setResizeLimits(designW*6/10,designH*6/10,designW*3/2,designH*3/2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW)/designH);
 selectPad(0);
 setSize(designW*9/10,designH*9/10);
 timerCallback();
 startTimerHz(30);
}

InversoAbismoDrumsEditor::~InversoAbismoDrumsEditor(){ setLookAndFeel(nullptr); }

void InversoAbismoDrumsEditor::drawPadScreen(juce::Graphics& g,juce::Rectangle<float> r,int pad){
 auto area=r.withTrimmedBottom(34.f);
 const int object=juce::roundToInt(value(P::padBase(pad)+P::Object));
 const auto own=processor.getPadDisplay(pad);
 const auto& lib=InversoAbismoDrumsProcessor::library();
 const SampleSet* set=own.get();
 if(!set && isSampleObject(object) && static_cast<std::size_t>(object)<lib.objects.size()) set=&lib.objects[static_cast<std::size_t>(object)];
 const auto lost=processor.getPadMissing(pad);
 juce::String left,right;
 if(loadError.isNotEmpty()) left=loadError;
 else if(lost.isNotEmpty()) left=juce::String("FILE NOT FOUND: ")+lost.upToFirstOccurrenceOf("\n",false,false);
 else if(own) left=juce::String::fromUTF8("YOUR SAMPLE  \xc2\xb7  ")+processor.getPadSampleName(pad);
 else left=isSampleObject(object)?juce::String("RECORDING"):juce::String("SYNTHESISED OBJECT");
 if(set && !set->layers.empty())
  right=juce::String(static_cast<int>(set->layers.size()))+(set->layers.size()==1?" TAKE":" TAKES")+"   "+
        juce::String(static_cast<double>(set->layers.back().size())/set->sampleRate,2)+" s";
 g.setFont(soft::monoFont(10.f));
 g.setColour(loadError.isNotEmpty()||lost.isNotEmpty()?c::red:c::screenMuted);
 g.drawText(left,area.withHeight(12.f),juce::Justification::topLeft);
 g.setColour(c::screenMuted);
 g.drawText(right,area.withHeight(12.f),juce::Justification::topRight);
 auto wave=area.withTrimmedTop(18.f);
 const float light=padLight[static_cast<std::size_t>(pad)];
 const auto colour=c::blood.brighter(.35f).interpolatedWith(juce::Colours::white,light*.5f);
 if(set && !set->layers.empty()){
  const auto& data=set->layers.back();
  const int columns=juce::roundToInt(wave.getWidth()/2.f);
  const double per=static_cast<double>(data.size())/columns;
  float peak=1e-4f;
  for(float x:data) peak=std::max(peak,std::abs(x));
  juce::Path shape;
  for(int col=0;col<columns;++col){
   const auto a=static_cast<std::size_t>(col*per),b=std::min(data.size(),std::max(a+1,static_cast<std::size_t>((col+1)*per)));
   float lo=0.f,hi=0.f;
   for(std::size_t i=a;i<b;++i){ lo=std::min(lo,data[i]); hi=std::max(hi,data[i]); }
   const float half=wave.getHeight()*.48f/peak;
   shape.addRectangle(wave.getX()+static_cast<float>(col)*2.f,wave.getCentreY()-hi*half,2.f,std::max(1.f,(hi-lo)*half));
  }
  g.setColour(colour.withAlpha(.18f+.3f*light));
  g.fillRect(wave.withWidth(wave.getWidth()*light*.08f+.1f));
  g.setColour(colour);
  g.fillPath(shape);
 } else {
  // Synthesised object: a struck, decaying body.
  soft::glowStroke(g,curve(wave,240,[&](float t){return .85f*std::exp(-t*5.f)*std::sin(t*80.f+std::sin(t*31.f));}),colour);
 }
}

void InversoAbismoDrumsEditor::selectPad(int pad){
 selected=juce::jlimit(0,numPads-1,pad);
 grid.setSelected(selected);
 for(int i=0;i<numPads;++i) padModules[static_cast<std::size_t>(i)]->setVisible(i==selected);
 loadError.clear();
}

int InversoAbismoDrumsEditor::padAt(int x,int y) const {
 return grid.padAt(grid.getLocalPoint(this,juce::Point<int>(x,y)));
}
bool InversoAbismoDrumsEditor::isInterestedInFileDrag(const juce::StringArray& files){
 for(const auto& f:files) if(processor.isAudioFile(f) || juce::File(f).isDirectory()) return true;
 return false;
}
void InversoAbismoDrumsEditor::fileDragMove(const juce::StringArray&,int x,int y){ grid.setDropping(padAt(x,y)); }
void InversoAbismoDrumsEditor::fileDragExit(const juce::StringArray&){ grid.setDropping(-1); }
void InversoAbismoDrumsEditor::filesDropped(const juce::StringArray& files,int x,int y){
 grid.setDropping(-1);
 const int hit=padAt(x,y);
 loadFiles(hit>=0?hit:selected,files);
}
// Files, or a folder of takes, become the pad's recordings.
void InversoAbismoDrumsEditor::loadFiles(int pad,const juce::StringArray& paths){
 juce::Array<juce::File> files;
 for(const auto& path:paths){
  const juce::File f(path);
  if(f.isDirectory()){
   auto inside=f.findChildFiles(juce::File::findFiles,false);
   inside.sort();
   for(const auto& ch:inside) if(processor.isAudioFile(ch.getFullPathName())) files.add(ch);
  } else if(processor.isAudioFile(path)) files.add(f);
 }
 selectPad(pad);
 if(files.isEmpty() || !processor.loadPadFiles(pad,files))
  loadError=juce::String("COULD NOT READ THAT AUDIO FILE");
}
void InversoAbismoDrumsEditor::chooseFiles(){
 chooser=std::make_unique<juce::FileChooser>("Sample(s) for pad "+juce::String(selected+1),juce::File(),"*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
 const int pad=selected;
 chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectMultipleItems,
  [this,pad](const juce::FileChooser& fc){
   juce::StringArray paths;
   for(const auto& f:fc.getResults()) paths.add(f.getFullPathName());
   if(!paths.isEmpty()) loadFiles(pad,paths);
  });
}

// A control whose effect depends on another one is dimmed while it does nothing.
static bool drumsControlActive(const juce::AudioProcessorValueTreeState& apvts,int p){
 auto v=[&](int i){return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load();};
 const bool stutter=juce::roundToInt(v(P::StutterDiv))>0, free=juce::roundToInt(v(P::StutterSync))==1;
 const bool saw=v(P::SawLevel)>0.f, room=v(P::RoomMix)>0.f;
 switch(p){
  case P::StutterSync: return stutter;
  case P::StutterMs: return free && stutter;
  case P::StutterDepth: case P::Chaos: return stutter;
  case P::RoomSize: case P::RoomDecay: case P::RoomDamp: return room;
  case P::SawAttack: case P::SawRelease: case P::SawVibrato: case P::SawGlide: case P::SawTone: case P::SawWobble: return saw;
  case P::SawVibRate: return saw && v(P::SawVibrato)>0.f;
  default: return true;
 }
}

void InversoAbismoDrumsEditor::timerCallback(){
 animation+=1.f/30.f;
 kits.refresh();
 for(auto& m:modules){
  m->dimInactive([this](int p){return drumsControlActive(processor.apvts,p);});
  m->screen.repaint();
 }
 const auto lights=processor.takePadLights();
 for(int i=0;i<numPads;++i){
  auto& l=padLight[static_cast<std::size_t>(i)];
  if(lights & (1u<<i)){ grid.flash(i); l=1.f; } else l=std::max(0.f,l-.08f);
  const int object=juce::roundToInt(value(P::padBase(i)+P::Object));
  const auto own=processor.getPadSampleName(i);
  grid.setPad(i,own.isNotEmpty()?own:juce::String::fromUTF8(objectName(object).c_str()),own.isNotEmpty(),processor.getPadMissing(i).isNotEmpty());
 }
 grid.decay();
 padModules[static_cast<std::size_t>(selected)]->screen.repaint();
 clearButton.setEnabled(processor.getPadSampleName(selected).isNotEmpty() || processor.getPadMissing(selected).isNotEmpty());
 if(InversoAbismoDrumsProcessor::library().objects.empty())
  body.subtitle=juce::String::fromUTF8("RECORDING LIBRARY NOT FOUND: REINSTALL  \xc2\xb7  PLAYING THE SYNTHESISED OBJECTS");
 master->setReadout(processor.outputPeak.load(),"SOUNDING "+juce::String(processor.hitsSounding.load()));
}

void InversoAbismoDrumsEditor::resized(){
 body.setBounds(0,0,designW,designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW));
 kits.setBounds(designW/2-210,14,420,72);
 const int x0=10,y0=88,mainW=1150,r1=520,r2=380;
 grid.setBounds(x0,y0,540,r1);
 for(auto& m:padModules) m->setBounds(x0+540,y0,mainW-540,r1);
 // The sample buttons sit in the pad module's header row.
 const auto header=juce::Rectangle<int>(x0+540,y0,mainW-540,r1).reduced(10).reduced(16,12).withHeight(22);
 clearButton.setBounds(header.getRight()-170,header.getY()-3,170,28);
 loadButton.setBounds(header.getRight()-170-164,header.getY()-3,160,28);
 modules[0]->setBounds(x0,y0+r1,400,r2);
 modules[1]->setBounds(x0+400,y0+r1,220,r2);
 modules[2]->setBounds(x0+620,y0+r1,270,r2);
 modules[3]->setBounds(x0+890,y0+r1,mainW-890,r2);
 master->setBounds(x0+mainW,y0,designW-(x0+mainW)-10,r1+r2);
}
