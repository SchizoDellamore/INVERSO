#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
namespace {
const auto bg=juce::Colour::fromRGB(14,17,20);
const auto panel=juce::Colour::fromRGB(24,29,33);
const auto text=juce::Colour::fromRGB(235,228,215);
const auto muted=juce::Colour::fromRGB(154,160,161);
const auto orange=juce::Colour::fromRGB(245,132,42);
const char* padNames[]={"KICK","SNARE","CLOSED HH","OPEN HH","TOM 1","TOM 2","CLAP","PERC 1",
 "KICK 2","SNARE 2","HAT 2","HAT 3","TOM 3","PERC 2","PERC 3","PERC 4"};
const char* sampleControls[]={"START %","END %","PITCH ST","ATTACK %","DECAY %"};
const char* fxControls[]={"PAD DRIVE","REVERB SEND","DELAY SEND","REVERB MIX","REVERB TAIL","DELAY MIX","FEEDBACK"};
}
void InversoDrumEditor::createSlider(juce::Slider& slider,juce::Label& label,const char* title,double min,double max,double initial){
 slider.setRange(min,max,(max-min)>20?1:.01);
 slider.setSliderStyle(juce::Slider::LinearHorizontal);
 slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,22);
 slider.setValue(initial,juce::dontSendNotification);
 slider.setColour(juce::Slider::trackColourId,orange);
 slider.setColour(juce::Slider::thumbColourId,text);
 slider.setColour(juce::Slider::textBoxTextColourId,text);
 addAndMakeVisible(slider);
 label.setText(title,juce::dontSendNotification);
 label.setFont(juce::Font(juce::FontOptions(12.f,juce::Font::bold)));
 label.setColour(juce::Label::textColourId,muted);
 addAndMakeVisible(label);
}
InversoDrumEditor::InversoDrumEditor(InversoDrumProcessor& p)
:AudioProcessorEditor(&p),processor(p),grid(*this),waveform(*this){
 setSize(1250,865);
 addAndMakeVisible(grid);
 addAndMakeVisible(waveform);
 for(int i=0;i<8;++i){
  auto& b=patternButtons[i];
  b.setButtonText(juce::String::charToString('A'+i));
  b.onClick=[this,i]{processor.engine.selectedPattern.store(i);updateUi();};
  addAndMakeVisible(b);
  auto& page=pageButtons[i];
  page.setButtonText(juce::String(i*16+1)+"-"+juce::String((i+1)*16));
  page.onClick=[this,i]{currentPage=i;updateUi();};
  addAndMakeVisible(page);
 }
 for(int i=0;i<16;++i){
  auto& b=padButtons[i];b.setButtonText(padNames[i]);
  b.onClick=[this,i]{selectPad(i);processor.auditionPad(i);};
  addAndMakeVisible(b);
 }
 chain.setToggleState(p.engine.chain.load(),juce::dontSendNotification);
 chain.onClick=[this]{processor.engine.chain.store(chain.getToggleState());updateUi();};
 addAndMakeVisible(chain);
 createSlider(swing,swingLabel,"SWING",0,100,0);
 swing.onValueChange=[this]{if(!refreshing)processor.engine.swing.store(static_cast<float>(swing.getValue()*.01));};
 createSlider(laneLength,lengthLabel,"STEPS",1,128,16);
 laneLength.onValueChange=[this]{if(!refreshing){processor.engine.setLaneLength(selectedPad,static_cast<int>(laneLength.getValue()));updateUi();}};
 createSlider(level,levelLabel,"LEVEL",0,200,80);
 level.onValueChange=[this]{if(!refreshing)processor.engine.gain[selectedPad].store(static_cast<float>(level.getValue()*.01));};
 createSlider(pan,panLabel,"PAN",-100,100,0);
 pan.onValueChange=[this]{if(!refreshing)processor.engine.pan[selectedPad].store(static_cast<float>(pan.getValue()*.01));};
 auto setupCombo=[this](juce::ComboBox& b,const char* entries[],int n){
  for(int i=0;i<n;++i)b.addItem(entries[i],i+1);
  b.setColour(juce::ComboBox::backgroundColourId,panel);
  b.setColour(juce::ComboBox::textColourId,text);
  addAndMakeVisible(b);
 };
 const char* notes[]={"1/4","1/8","1/16","1/32"};
 const char* timeStyles[]={"Straight","Dotted","Triplet"};
 setupCombo(laneRate,notes,4);setupCombo(laneFeel,timeStyles,3);
 setupCombo(delayRate,notes,4);setupCombo(delayFeel,timeStyles,3);
 rateLabel.setText("DIVISION",juce::dontSendNotification);addAndMakeVisible(rateLabel);
 feelLabel.setText("FEEL",juce::dontSendNotification);addAndMakeVisible(feelLabel);
 delayRateLabel.setText("DELAY NOTE",juce::dontSendNotification);addAndMakeVisible(delayRateLabel);
 delayFeelLabel.setText("DELAY FEEL",juce::dontSendNotification);addAndMakeVisible(delayFeelLabel);
 laneRate.onChange=[this]{if(!refreshing)processor.engine.setDivision(selectedPad,laneRate.getSelectedId()-1,laneFeel.getSelectedId()-1);};
 laneFeel.onChange=[this]{if(!refreshing)processor.engine.setDivision(selectedPad,laneRate.getSelectedId()-1,laneFeel.getSelectedId()-1);};
 delayRate.onChange=[this]{processor.effects.delayDivision.store(std::max(0,delayRate.getSelectedId()-1));};
 delayFeel.onChange=[this]{processor.effects.delayFeel.store(std::max(0,delayFeel.getSelectedId()-1));};
 for(int i=0;i<5;++i){
  const auto init=i==1||i==4?100.:0.;
  createSlider(sampleSliders[i],sampleLabels[i],sampleControls[i],i==2?-24.:0.,i==2?24.:100.,init);
  sampleSliders[i].onValueChange=[this,i]{
   if(refreshing)return;
   const float value=static_cast<float>(sampleSliders[i].getValue());
   auto& e=processor.engine;
   switch(i){case 0:e.sampleStart[selectedPad].store(value*.01f);break;
             case 1:e.sampleEnd[selectedPad].store(value*.01f);break;
             case 2:e.samplePitch[selectedPad].store(value);break;
             case 3:e.sampleAttack[selectedPad].store(value*.01f);break;
             case 4:e.sampleDecay[selectedPad].store(value*.01f);break;}
   waveform.repaint();
  };
 }
 const float defaults[]={0.f,15.f,0.f,30.f,45.f,25.f,30.f};
 for(int i=0;i<7;++i){
  createSlider(fxSliders[i],fxLabels[i],fxControls[i],0,100,defaults[i]);
  fxSliders[i].onValueChange=[this,i]{
   if(refreshing)return;
   const float value=static_cast<float>(fxSliders[i].getValue()*.01);
   auto& engine=processor.engine;
   auto& effects=processor.effects;
   switch(i){case 0:engine.drive[selectedPad].store(value);break;
             case 1:engine.sendVerb[selectedPad].store(value);break;
             case 2:engine.sendDelay[selectedPad].store(value);break;
             case 3:effects.reverbWet.store(value);break;
             case 4:effects.reverbDecay.store(value);break;
             case 5:effects.delayWet.store(value);break;
             case 6:effects.delayFeedback.store(value);break;}
  };
 }
 reverse.onClick=[this]{processor.engine.sampleReverse[selectedPad].store(reverse.getToggleState());waveform.repaint();};
 addAndMakeVisible(reverse);
 sampleTab.onClick=[this]{showFx=false;updateTabs();};addAndMakeVisible(sampleTab);
 fxTab.onClick=[this]{showFx=true;updateTabs();};addAndMakeVisible(fxTab);
 loadButton.onClick=[this]{loadSelected();};addAndMakeVisible(loadButton);
 exportButton.onClick=[this]{exportMidi();};addAndMakeVisible(exportButton);
 selectedSample.setColour(juce::Label::textColourId,text);addAndMakeVisible(selectedSample);
 tempoLabel.setColour(juce::Label::textColourId,muted);addAndMakeVisible(tempoLabel);
 selectPad(0);
 swing.setValue(p.engine.swing.load()*100,juce::dontSendNotification);
 delayRate.setSelectedId(p.effects.delayDivision.load()+1,juce::dontSendNotification);
 delayFeel.setSelectedId(p.effects.delayFeel.load()+1,juce::dontSendNotification);
 updateTabs();updateUi();startTimerHz(15);
}
void InversoDrumEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);g.setFont(juce::Font(juce::FontOptions(25.f,juce::Font::bold)));
 g.drawText("INVERSO / DRUM",22,12,520,39,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12.f)));
 g.drawText("16 LANES  /  UP TO 128 STEPS EACH  /  HOST-SYNCED SEQUENCER",25,53,700,19,juce::Justification::centredLeft);
 g.setColour(orange);g.setFont(juce::Font(juce::FontOptions(13.f,juce::Font::bold)));
 g.drawText("PATTERNS",25,84,160,21,juce::Justification::centredLeft);
 g.setColour(muted);g.drawText("STEP PAGE",188,132,174,21,juce::Justification::centredLeft);
 g.drawText("LANES",24,166,150,19,juce::Justification::centredLeft);
 g.setColour(panel);g.fillRoundedRectangle(18.f,548.f,1214.f,68.f,7.f);
 g.fillRoundedRectangle(18.f,658.f,1214.f,178.f,7.f);
 g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(11.f)));
 g.drawText("LEFT CLICK: TOGGLE STEP  |  RIGHT CLICK: VELOCITY  |  MIDI NOTES 36-51  |  SAMPLES REFERENCE SOURCE PATHS",20,838,1170,17,juce::Justification::centredLeft);
}
void InversoDrumEditor::resized(){
 for(int i=0;i<8;++i){
  patternButtons[i].setBounds(154+i*58,82,51,30);
  pageButtons[i].setBounds(189+i*129,128,120,31);
 }
 chain.setBounds(683,84,138,28);
 swingLabel.setBounds(847,79,85,18);swing.setBounds(840,99,190,28);
 tempoLabel.setBounds(1030,89,193,25);
 const float row=22.f;
 for(int i=0;i<16;++i)padButtons[i].setBounds(20,187+static_cast<int>(i*row),156,21);
 grid.setBounds(188,186,1034,352);
 lengthLabel.setBounds(30,551,85,18);laneLength.setBounds(25,575,193,30);
 rateLabel.setBounds(255,551,125,18);laneRate.setBounds(250,577,136,29);
 feelLabel.setBounds(405,551,110,18);laneFeel.setBounds(400,577,150,29);
 levelLabel.setBounds(588,551,100,18);level.setBounds(583,575,202,30);
 panLabel.setBounds(834,551,105,18);pan.setBounds(828,575,194,30);
 exportButton.setBounds(1060,577,157,29);
 sampleTab.setBounds(19,625,164,30);fxTab.setBounds(190,625,155,30);
 selectedSample.setBounds(365,625,840,27);
 waveform.setBounds(24,668,450,76);
 reverse.setBounds(498,671,130,27);
 loadButton.setBounds(1063,702,149,30);
 for(int i=0;i<5;++i){
  const int x=26+i*239;
  sampleLabels[i].setBounds(x,759,223,18);
  sampleSliders[i].setBounds(x,783,214,28);
 }
 for(int i=0;i<7;++i){
  const int x=25+(i%4)*299;
  const int y=665+(i/4)*72;
  fxLabels[i].setBounds(x,y,275,18);
  fxSliders[i].setBounds(x,y+22,268,30);
 }
 delayRateLabel.setBounds(925,745,136,18);delayRate.setBounds(924,767,134,28);
 delayFeelLabel.setBounds(1068,745,145,18);delayFeel.setBounds(1068,767,140,28);
}
void InversoDrumEditor::selectPad(int pad){
 selectedPad=juce::jlimit(0,15,pad);
 const auto& e=processor.engine;
 refreshing=true;
 laneLength.setValue(e.laneLength[selectedPad].load(),juce::dontSendNotification);
 laneRate.setSelectedId(e.division[selectedPad].load()+1,juce::dontSendNotification);
 laneFeel.setSelectedId(e.feel[selectedPad].load()+1,juce::dontSendNotification);
 level.setValue(e.gain[selectedPad].load()*100.,juce::dontSendNotification);
 pan.setValue(e.pan[selectedPad].load()*100.,juce::dontSendNotification);
 const double vals[]={e.sampleStart[selectedPad].load()*100.,e.sampleEnd[selectedPad].load()*100.,
  e.samplePitch[selectedPad].load(),e.sampleAttack[selectedPad].load()*100.,e.sampleDecay[selectedPad].load()*100.};
 for(int i=0;i<5;++i)sampleSliders[i].setValue(vals[i],juce::dontSendNotification);
 const double fx[]={e.drive[selectedPad].load()*100.,e.sendVerb[selectedPad].load()*100.,
  e.sendDelay[selectedPad].load()*100.,processor.effects.reverbWet.load()*100.,
  processor.effects.reverbDecay.load()*100.,processor.effects.delayWet.load()*100.,
  processor.effects.delayFeedback.load()*100.};
 for(int i=0;i<7;++i)fxSliders[i].setValue(fx[i],juce::dontSendNotification);
 reverse.setToggleState(e.sampleReverse[selectedPad].load(),juce::dontSendNotification);
 refreshing=false;
 selectedSample.setText("PAD "+juce::String(selectedPad+1)+"  /  "+processor.sampleName(selectedPad),juce::dontSendNotification);
 waveform.repaint();updateUi();
}
void InversoDrumEditor::updateUi(){
 const int active=processor.engine.selectedPattern.load();
 for(int i=0;i<8;++i){
  patternButtons[i].setColour(juce::TextButton::buttonColourId,i==active?orange:panel);
  patternButtons[i].setColour(juce::TextButton::textColourOffId,i==active?juce::Colours::black:text);
  pageButtons[i].setColour(juce::TextButton::buttonColourId,i==currentPage?orange:panel);
  pageButtons[i].setColour(juce::TextButton::textColourOffId,i==currentPage?juce::Colours::black:text);
 }
 for(int i=0;i<16;++i){
  padButtons[i].setColour(juce::TextButton::buttonColourId,i==selectedPad?orange:panel);
  padButtons[i].setColour(juce::TextButton::textColourOffId,i==selectedPad?juce::Colours::black:text);
 }
 grid.repaint();
}
void InversoDrumEditor::updateTabs(){
 waveform.setVisible(!showFx);reverse.setVisible(!showFx);loadButton.setVisible(!showFx);
 for(auto& s:sampleSliders)s.setVisible(!showFx);
 for(auto& l:sampleLabels)l.setVisible(!showFx);
 for(auto& s:fxSliders)s.setVisible(showFx);
 for(auto& l:fxLabels)l.setVisible(showFx);
 delayRate.setVisible(showFx);delayFeel.setVisible(showFx);
 delayRateLabel.setVisible(showFx);delayFeelLabel.setVisible(showFx);
 sampleTab.setColour(juce::TextButton::buttonColourId,!showFx?orange:panel);
 fxTab.setColour(juce::TextButton::buttonColourId,showFx?orange:panel);
 repaint();
}
void InversoDrumEditor::timerCallback(){
 grid.repaint();
 tempoLabel.setText("HOST "+juce::String(processor.hostBpm.load(),1)+" BPM  /  OUT "+
  juce::String(juce::Decibels::gainToDecibels(processor.outputPeak.load(),-100.f),1)+" dBFS",juce::dontSendNotification);
}
void InversoDrumEditor::StepGrid::paint(juce::Graphics& g){
 const int pat=editor.processor.engine.selectedPattern.load();
 const bool playingPat=editor.processor.engine.currentPattern.load()==pat;
 const float w=getWidth()/16.f,h=getHeight()/16.f;
 for(int lane=0;lane<16;++lane){
  const int len=editor.processor.engine.laneLength[lane].load();
  const int play=editor.processor.engine.currentLaneStep[lane].load();
  for(int col=0;col<16;++col){
   const int step=editor.currentPage*16+col;
   const float x=col*w+2.f,y=lane*h+1.f;
   const bool enabled=step<len;
   const int vel=editor.processor.engine.get(pat,lane,step);
   const bool highlight=play==step&&playingPat;
   juce::Colour color=enabled?(vel>0?orange.withAlpha(.30f+.70f*vel/127.f):juce::Colour::fromRGB(col%4==0?49:34,40,42)):
    juce::Colour::fromRGB(23,26,28);
   g.setColour(color);g.fillRoundedRectangle(x,y,w-4.f,h-3.f,3.f);
   if(highlight){g.setColour(juce::Colours::white.withAlpha(.88f));g.drawRoundedRectangle(x,y,w-4.f,h-3.f,3.f,1.3f);}
   if(lane==editor.selectedPad && !highlight){g.setColour(orange.withAlpha(.60f));g.drawRoundedRectangle(x,y,w-4.f,h-3.f,3.f,1.f);}
  }
 }
}
void InversoDrumEditor::StepGrid::mouseDown(const juce::MouseEvent& ev){previousCell=-1;mouseDrag(ev);}
void InversoDrumEditor::StepGrid::mouseDrag(const juce::MouseEvent& ev){
 const int col=static_cast<int>(ev.position.x/(getWidth()/16.f));
 const int lane=static_cast<int>(ev.position.y/(getHeight()/16.f));
 if(col<0||col>=16||lane<0||lane>=16)return;
 const int step=editor.currentPage*16+col;
 if(step>=editor.processor.engine.laneLength[lane].load())return;
 const int cell=lane*128+step;if(cell==previousCell)return;previousCell=cell;
 const int pat=editor.processor.engine.selectedPattern.load();
 const int previous=editor.processor.engine.get(pat,lane,step);
 const int next=ev.mods.isRightButtonDown()?(previous==0?55:previous<80?90:previous<115?127:0):(previous?0:105);
 editor.processor.engine.set(pat,lane,step,next);
 editor.selectPad(lane);repaint();
}
void InversoDrumEditor::Waveform::paint(juce::Graphics& g){
 const auto bounds=getLocalBounds().toFloat();
 g.setColour(panel);g.fillRoundedRectangle(bounds,5.f);
 const auto sample=editor.processor.engine.getSample(editor.selectedPad);
 if(!sample||sample->left.empty())return;
 g.setColour(orange.withAlpha(.85f));
 const int width=getWidth(),height=getHeight();
 for(int x=0;x<width;++x){
  const size_t index=std::min(sample->left.size()-1,static_cast<size_t>(static_cast<double>(x)/width*sample->left.size()));
  const float amplitude=std::min(1.f,std::abs(sample->left[index]));
  const float size=std::max(1.f,amplitude*(height-10)*.5f);
  g.drawVerticalLine(x,height*.5f-size,height*.5f+size);
 }
 g.setColour(juce::Colours::white.withAlpha(.8f));
 const int a=static_cast<int>(editor.processor.engine.sampleStart[editor.selectedPad].load()*width);
 const int b=static_cast<int>(editor.processor.engine.sampleEnd[editor.selectedPad].load()*width);
 g.drawVerticalLine(a,2.f,static_cast<float>(height-2));
 g.drawVerticalLine(std::min(width-1,b),2.f,static_cast<float>(height-2));
}
bool InversoDrumEditor::isInterestedInFileDrag(const juce::StringArray& files){
 for(const auto& file:files){const auto ext=juce::File(file).getFileExtension().toLowerCase();if(ext==".wav"||ext==".aif"||ext==".aiff")return true;}
 return false;
}
void InversoDrumEditor::filesDropped(const juce::StringArray& files,int,int){
 if(files.size()>0&&processor.loadPadFile(selectedPad,juce::File(files[0])))selectPad(selectedPad);
}
void InversoDrumEditor::loadSelected(){
 chooser=std::make_unique<juce::FileChooser>("Load drum sample",juce::File{},"*.wav;*.aif;*.aiff");
 const int pad=selectedPad;
 juce::Component::SafePointer<InversoDrumEditor> safe(this);
 chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
 [safe,pad](const juce::FileChooser& fc){
  if(safe==nullptr)return;
  const auto file=fc.getResult();
  if(file.existsAsFile()&&safe->processor.loadPadFile(pad,file))safe->selectPad(pad);
 });
}
void InversoDrumEditor::exportMidi(){
 chooser=std::make_unique<juce::FileChooser>("Export drum pattern to MIDI",juce::File{},"*.mid");
 juce::Component::SafePointer<InversoDrumEditor> safe(this);
 chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
 [safe](const juce::FileChooser& fc){
  if(safe==nullptr)return;
  const auto file=fc.getResult();if(file==juce::File{})return;
  const auto target=file.hasFileExtension(".mid")?file:file.withFileExtension(".mid");
  safe->processor.exportMidi(target);
 });
}
