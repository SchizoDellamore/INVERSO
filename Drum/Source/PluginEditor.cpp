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
 setSize(1250,1005);
 addAndMakeVisible(grid);
 libraryButton.setColour(juce::TextButton::buttonColourId,orange);
 libraryButton.setColour(juce::TextButton::textColourOffId,juce::Colours::black);
 libraryButton.onClick=[this]{
  sampleLibrary.setPad(selectedPad);
  sampleLibrary.open();
 };
 addAndMakeVisible(libraryButton);
 sampleLibrary.setLoadHandler([this](int pad,const juce::File& file){
  if(!processor.loadPadFile(pad,file))return false;
  selectPad(pad);
  processor.auditionPad(pad);
  return true;
 });
 addAndMakeVisible(sampleLibrary);
 sampleLibrary.setVisible(false);
 viewBars.addItem("1 bar",1);viewBars.addItem("2 bars",2);
 viewBars.addItem("4 bars",3);viewBars.addItem("8 bars",4);
 viewBars.addItem("16 bars",5);
 viewBars.setSelectedId(3,juce::dontSendNotification);
 viewBars.onChange=[this]{
  const int spans[]={1,2,4,8,16};
  displayedBars=spans[juce::jlimit(0,4,viewBars.getSelectedId()-1)];
  updateUi();
 };
 viewBars.setColour(juce::ComboBox::backgroundColourId,panel);
 viewBars.setColour(juce::ComboBox::textColourId,text);
 addAndMakeVisible(viewBars);
 viewLabel.setText("VIEW",juce::dontSendNotification);addAndMakeVisible(viewLabel);
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
 createSlider(stepVelocity,stepVelocityLabel,"STEP VELOCITY (0=OFF)",0,127,105);
 stepVelocity.setNumDecimalPlacesToDisplay(0);
 stepVelocity.onValueChange=[this]{
  if(refreshing)return;
  processor.engine.set(processor.engine.selectedPattern.load(),selectedPad,selectedStep,
                       static_cast<int>(stepVelocity.getValue()));
  refreshStepVelocity();
  grid.repaint();
 };
 const char* stepNames[]={"STEP DRIVE","STEP REVERB SEND","STEP DELAY SEND"};
 for(int i=0;i<3;++i){
  createSlider(stepFxSliders[i],stepFxLabels[i],stepNames[i],0,100,0);
  stepFxSliders[i].setNumDecimalPlacesToDisplay(0);
  stepFxSliders[i].onValueChange=[this,i]{
   if(refreshing)return;
   processor.engine.setEventFx(processor.engine.selectedPattern.load(),selectedPad,selectedStep,
       i,static_cast<int>(stepFxSliders[i].getValue()));
   refreshStepEffects();grid.repaint();
  };
 }
 resetStepFx.setColour(juce::TextButton::buttonColourId,panel);
 resetStepFx.onClick=[this]{
  for(int i=0;i<3;++i)processor.engine.setEventFx(
    processor.engine.selectedPattern.load(),selectedPad,selectedStep,i,inverso_drum::Engine::inheritFx);
  refreshStepEffects();grid.repaint();
 };
 addAndMakeVisible(resetStepFx);
 stepInfo.setColour(juce::Label::textColourId,text);
 addAndMakeVisible(stepInfo);
 clearStep.onClick=[this]{stepVelocity.setValue(0,juce::sendNotificationSync);};
 addAndMakeVisible(clearStep);
 swing.onValueChange=[this]{if(!refreshing)processor.engine.swing.store(static_cast<float>(swing.getValue()*.01));};
 createSlider(laneLength,lengthLabel,"STEPS (x4)",4,128,16);
 laneLength.setRange(4,128,4);
 laneLength.setNumDecimalPlacesToDisplay(0);
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
 const float defaults[]={0.f,36.f,24.f,55.f,55.f,45.f,34.f};
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
 exportButton.onClick=[this]{exportMidi();};
 exportButton.onStartExternalDrag=[this](juce::Component* source){
  const auto destination=juce::File::getSpecialLocation(juce::File::tempDirectory)
     .getNonexistentChildFile("INVERSO_DRUM_pattern", ".mid",false);
  if(processor.exportMidi(destination)){
   juce::DragAndDropContainer::performExternalDragDropOfFiles(
     juce::StringArray{destination.getFullPathName()},false,source);
  }
 };
 addAndMakeVisible(exportButton);
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
 g.drawText("INVERSO / DRUM  v0.6",22,12,620,39,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(12.f)));
 g.drawText("16 LANES  /  UP TO 128 STEPS EACH  /  HOST-SYNCED SEQUENCER",25,53,700,19,juce::Justification::centredLeft);
 g.setColour(orange);g.setFont(juce::Font(juce::FontOptions(13.f,juce::Font::bold)));
 g.drawText("PATTERNS",25,84,160,21,juce::Justification::centredLeft);
 g.setColour(muted);g.drawText("JUMP TO STEP",188,132,174,21,juce::Justification::centredLeft);
 g.drawText("LANES",24,166,150,19,juce::Justification::centredLeft);
 g.setColour(panel);g.fillRoundedRectangle(18.f,548.f,1214.f,68.f,7.f);
 g.fillRoundedRectangle(18.f,798.f,1214.f,178.f,7.f);
 g.setColour(muted);g.setFont(juce::Font(juce::FontOptions(11.f)));
 g.drawText("EXTERNAL WAV/AIFF  |  STEP VELOCITY AND FX ARE EVENT-SPECIFIC  |  EFFECT TAILS CONTINUE NATURALLY",20,978,1195,17,juce::Justification::centredLeft);
 if(dragOver){
  g.setColour(orange.withAlpha(.85f));
  g.drawRoundedRectangle(8.f,178.f,1228.f,368.f,6.f,2.f);
  g.setFont(juce::Font(juce::FontOptions(15.f,juce::Font::bold)));
  g.drawText("DROP SAMPLE ON THE DESIRED TRACK",600,160,500,20,juce::Justification::centredRight);
 }
}
void InversoDrumEditor::resized(){
 for(int i=0;i<8;++i){
  patternButtons[i].setBounds(154+i*58,82,51,30);
  pageButtons[i].setBounds(189+i*129,128,120,31);
 }
 chain.setBounds(683,84,138,28);
 swingLabel.setBounds(847,79,85,18);swing.setBounds(840,99,190,28);
 tempoLabel.setBounds(1030,89,193,25);
 viewLabel.setBounds(1080,51,55,20);viewBars.setBounds(1138,47,94,30);
 libraryButton.setBounds(925,14,188,29);
 sampleLibrary.setBounds(155,115,940,770);
 const float row=22.f;
 for(int i=0;i<16;++i)padButtons[i].setBounds(20,187+static_cast<int>(i*row),156,21);
 grid.setBounds(188,186,1034,352);
 lengthLabel.setBounds(30,551,85,18);laneLength.setBounds(25,575,193,30);
 rateLabel.setBounds(255,551,125,18);laneRate.setBounds(250,577,136,29);
 feelLabel.setBounds(405,551,110,18);laneFeel.setBounds(400,577,150,29);
 levelLabel.setBounds(588,551,100,18);level.setBounds(583,575,202,30);
 panLabel.setBounds(834,551,105,18);pan.setBounds(828,575,194,30);
 exportButton.setBounds(1046,577,175,29);
 stepVelocityLabel.setBounds(26,622,205,20);
 stepVelocity.setBounds(225,626,490,30);
 stepInfo.setBounds(740,624,333,29);
 clearStep.setBounds(1091,623,130,31);
 const int stepXs[]={25,355,685};
 for(int i=0;i<3;++i){
  stepFxLabels[i].setBounds(stepXs[i],676,320,20);
  stepFxSliders[i].setBounds(stepXs[i],698,315,30);
 }
 resetStepFx.setBounds(1048,697,175,31);
 sampleTab.setBounds(19,765,164,30);fxTab.setBounds(190,765,155,30);
 selectedSample.setBounds(365,765,840,27);
 waveform.setBounds(24,808,450,76);
 reverse.setBounds(498,811,130,27);
 loadButton.setBounds(1063,842,149,30);
 for(int i=0;i<5;++i){
  const int x=26+i*239;
  sampleLabels[i].setBounds(x,899,223,18);
  sampleSliders[i].setBounds(x,923,214,28);
 }
 for(int i=0;i<7;++i){
  const int x=25+(i%4)*299;
  const int y=805+(i/4)*72;
  fxLabels[i].setBounds(x,y,275,18);
  fxSliders[i].setBounds(x,y+22,268,30);
 }
 delayRateLabel.setBounds(925,885,136,18);delayRate.setBounds(924,907,134,28);
 delayFeelLabel.setBounds(1068,885,145,18);delayFeel.setBounds(1068,907,140,28);
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
 sampleLibrary.setPad(selectedPad);
 waveform.repaint();updateUi();
}
void InversoDrumEditor::selectStep(int pad,int step){
 selectedStep=juce::jlimit(0,inverso_drum::steps-1,step);
 selectPad(pad);
}
void InversoDrumEditor::refreshStepVelocity(){
 const int pat=juce::jlimit(0,7,processor.engine.selectedPattern.load());
 const int velocity=processor.engine.get(pat,selectedPad,selectedStep);
 refreshing=true;
 stepVelocity.setValue(velocity,juce::dontSendNotification);
 refreshing=false;
 stepInfo.setText("PAT "+juce::String("ABCDEFGH").substring(pat,pat+1)
  +"  /  "+padNames[selectedPad]+"  /  STEP "+juce::String(selectedStep+1)
  +"  /  "+juce::String(velocity)+"/127",juce::dontSendNotification);
}
void InversoDrumEditor::refreshStepEffects(){
 const int pat=juce::jlimit(0,7,processor.engine.selectedPattern.load());
 const char* titles[]={"STEP DRIVE","STEP REVERB","STEP DELAY"};
 const float defaults[]={processor.engine.drive[selectedPad].load(),
                         processor.engine.sendVerb[selectedPad].load(),
                         processor.engine.sendDelay[selectedPad].load()};
 refreshing=true;
 for(int i=0;i<3;++i){
  const int overrideValue=processor.engine.getEventFx(pat,selectedPad,selectedStep,i);
  const bool inherited=overrideValue==inverso_drum::Engine::inheritFx;
  stepFxSliders[i].setValue(inherited?std::round(defaults[i]*100.f):overrideValue,
                            juce::dontSendNotification);
  stepFxLabels[i].setText(juce::String(titles[i])+
      (inherited?" (PAD DEFAULT)":" (THIS HIT ONLY)"),juce::dontSendNotification);
 }
 refreshing=false;
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
 refreshStepVelocity();
 refreshStepEffects();
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
// Timeline axis is musical time, not an arbitrary 16-cell page. All lanes align to Cubase PPQ.
// A 16-bar view is 64 quarter notes in 4/4; 128 eighth-notes cover all 16 bars.
double InversoDrumEditor::viewStartPPQ() const noexcept {
 const auto& e=processor.engine;
 const auto duration=inverso_drum::divisionPPQ(e.division[selectedPad].load(),e.feel[selectedPad].load());
 const double desired=currentPage*16.0*duration;
 const double viewLength=4.0*displayedBars;
 // A quarter-note lane with 128 steps spans 32 bars; triplet and dotted
 // lanes have other durations. Never clip the viewport at 16 bars or the
 // final steps become impossible to select and edit.
 const double selectedLaneMaxPPQ=128.0*duration;
 const double navigablePPQ=std::max(64.0,selectedLaneMaxPPQ);
 return std::clamp(std::floor(desired/4.0)*4.0,0.0,
                   std::max(0.0,navigablePPQ-viewLength));
}
void InversoDrumEditor::StepGrid::paint(juce::Graphics& g){
 const auto& engine=editor.processor.engine;
 const int pat=engine.selectedPattern.load();
 const bool playingPat=engine.currentPattern.load()==pat;
 const double from=editor.viewStartPPQ(), lengthPPQ=4.0*editor.displayedBars;
 const float w=getWidth(),h=getHeight()/16.f;
 // Alternating bars and vertical bar-lines make up to 16 bars legible.
 for(int bar=0;bar<editor.displayedBars;++bar){
  const float x=bar*w/editor.displayedBars;
  if(bar%2){g.setColour(juce::Colour::fromRGB(29,31,35));g.fillRect(x,0.f,w/editor.displayedBars,getHeight()*1.f);}
  g.setColour(juce::Colour::fromRGB(83,87,90));g.drawVerticalLine(static_cast<int>(x),0.f,static_cast<float>(getHeight()));
 }
 for(int lane=0;lane<16;++lane){
  const float y=lane*h+1.f;
  const int len=engine.laneLength[lane].load();
  const double dur=inverso_drum::divisionPPQ(engine.division[lane].load(),engine.feel[lane].load());
  const int first=std::max(0,static_cast<int>(std::floor(from/dur)));
  const int last=std::min(20000,static_cast<int>(std::ceil((from+lengthPPQ)/dur)));
  const float width=std::max(1.5f,static_cast<float>(dur/lengthPPQ*w));
  for(int index=first;index<last;++index){
   const double at=index*dur;
   if(at<from-1e-8 || at>=from+lengthPPQ-1e-8)continue;
   const int step=index%len;
   const int vel=engine.get(pat,lane,step);
   const float x=static_cast<float>((at-from)/lengthPPQ*w);
   if(vel>0){
    g.setColour(orange.withAlpha(.35f+.65f*vel/127.f));
    g.fillRoundedRectangle(x+1.f,y,std::max(1.f,width-2.f),h-3.f,2.5f);
    if(width>8.f){
     g.setColour(orange.brighter(.8f));
     g.fillRect(x+2.f,y+h-5.f,std::max(1.f,(width-5.f)*vel/127.f),2.f);
    }
    if(width>10.f){
     const int verb=engine.getEventFx(pat,lane,step,1);
     const int del=engine.getEventFx(pat,lane,step,2);
     const int drv=engine.getEventFx(pat,lane,step,0);
     if(verb!=inverso_drum::Engine::inheritFx && verb>0){
      g.setColour(juce::Colours::lightblue);g.fillEllipse(x+width-10.f,y+2.f,3.f,3.f);
     }
     if(del!=inverso_drum::Engine::inheritFx && del>0){
      g.setColour(juce::Colours::yellow);g.fillEllipse(x+width-6.f,y+2.f,3.f,3.f);
     }
     if(drv!=inverso_drum::Engine::inheritFx && drv>0){
      g.setColour(juce::Colours::red);g.fillEllipse(x+width-14.f,y+2.f,3.f,3.f);
     }
    }
    if(width>=38.f){
     g.setColour(juce::Colours::black.withAlpha(.9f));
     g.setFont(juce::Font(juce::FontOptions(10.f,juce::Font::bold)));
     g.drawText(juce::String(vel),static_cast<int>(x+2.f),static_cast<int>(y),static_cast<int>(width-4.f),
       static_cast<int>(h-4.f),juce::Justification::centred);
    }
   } else if(width>6.f){
    g.setColour(juce::Colour::fromRGB(45,47,51));
    g.fillRoundedRectangle(x+1.f,y,std::max(1.f,width-2.f),h-3.f,2.5f);
   }
   if(lane==editor.selectedPad && step==editor.selectedStep && width>5.f){
    g.setColour(juce::Colours::white);
    g.drawRoundedRectangle(x+.6f,y-.2f,std::max(2.f,width-1.2f),h-2.f,2.5f,1.8f);
   }
   if(engine.currentLaneStep[lane].load()==step&&playingPat&&width>5.f){
    g.setColour(juce::Colours::white.withAlpha(.7f));
    g.drawRoundedRectangle(x+1.f,y,std::max(1.f,width-2.f),h-3.f,2.5f,1.f);
   }
  }
  if(lane==editor.selectedPad){
   g.setColour(orange.withAlpha(.70f));
   g.drawRect(0.f,y-1.f,w,h,1.1f);
  }
 }
}
bool InversoDrumEditor::StepGrid::locate(const juce::Point<float>& pos,int& lane,int& step) const noexcept {
 if(pos.x<0.f||pos.x>=static_cast<float>(getWidth())||pos.y<0.f||pos.y>=static_cast<float>(getHeight()))
  return false;
 lane=static_cast<int>(pos.y/(getHeight()/16.f));
 if(lane<0||lane>=16)return false;
 const auto& e=editor.processor.engine;
 const double duration=inverso_drum::divisionPPQ(e.division[lane].load(),e.feel[lane].load());
 const double ppq=editor.viewStartPPQ()+4.0*editor.displayedBars*pos.x/getWidth();
 const int length=std::clamp(e.laneLength[lane].load(),4,128);
 const int absolute=std::max(0,static_cast<int>(std::floor(ppq/duration+1e-9)));
 step=absolute%length;
 return true;
}
void InversoDrumEditor::StepGrid::mouseDown(const juce::MouseEvent& ev){
 previousCell=-1;
 int lane=0,step=0;
 if(!locate(ev.position,lane,step))return;
 editor.selectStep(lane,step);
 if(ev.mods.isRightButtonDown()){
  dragStartVelocity=editor.processor.engine.get(editor.processor.engine.selectedPattern.load(),lane,step);
  dragStartY=ev.position.y;
 }else mouseDrag(ev);
}
void InversoDrumEditor::StepGrid::mouseDrag(const juce::MouseEvent& ev){
 const int pat=editor.processor.engine.selectedPattern.load();
 if(ev.mods.isRightButtonDown()){
  const float sensitivity=ev.mods.isShiftDown()?.25f:1.5f;
  const int velocity=std::clamp(dragStartVelocity+
   static_cast<int>(std::round((dragStartY-ev.position.y)*sensitivity)),0,127);
  editor.processor.engine.set(pat,editor.selectedPad,editor.selectedStep,velocity);
  editor.refreshStepVelocity();
  repaint();
  return;
 }
 int lane=0,step=0;
 if(!locate(ev.position,lane,step))return;
 const int cell=lane*128+step;
 if(cell==previousCell)return;
 previousCell=cell;
 const int previous=editor.processor.engine.get(pat,lane,step);
 editor.processor.engine.set(pat,lane,step,previous?0:105);
 editor.selectStep(lane,step);
 repaint();
}
void InversoDrumEditor::StepGrid::mouseWheelMove(const juce::MouseEvent& ev,
                                                  const juce::MouseWheelDetails& wheel){
 if(wheel.deltaY==0.f)return;
 int lane=0,step=0;
 if(!locate(ev.position,lane,step))return;
 const int pat=editor.processor.engine.selectedPattern.load();
 const int old=editor.processor.engine.get(pat,lane,step);
 const int delta=(wheel.deltaY>0.f?1:-1)*(ev.mods.isShiftDown()?1:5);
 editor.processor.engine.set(pat,lane,step,std::clamp(old+delta,0,127));
 editor.selectStep(lane,step);
 repaint();
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
void InversoDrumEditor::filesDropped(const juce::StringArray& files,int x,int y){
 dragOver=false;
 const juce::Point<int> drop(x,y);
 int target=selectedPad;
 for(int lane=0;lane<16;++lane){
  if(padButtons[lane].getBounds().contains(drop) ||
     (grid.getBounds().contains(drop) &&
      static_cast<int>((y-grid.getY())/(grid.getHeight()/16.f))==lane)) {
   target=lane;break;
  }
 }
 // Multiple selected files populate consecutive pads, never overwrite outside 16 pads.
 int inserted=0;
 for(const auto& filename:files){
  const juce::File f(filename);
  if(!f.existsAsFile())continue;
  const auto ext=f.getFileExtension().toLowerCase();
  if(ext!=".wav" && ext!=".aif" && ext!=".aiff")continue;
  const int pad=target+inserted;
  if(pad>=16)break;
  if(processor.loadPadFile(pad,f))++inserted;
 }
 if(inserted>0){selectPad(target);updateUi();}
}
void InversoDrumEditor::loadSelected(){
 chooser=std::make_unique<juce::FileChooser>("Import WAV/AIFF sample",
   juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.wav;*.wave;*.aif;*.aiff",false);
 const int pad=selectedPad;
 juce::Component::SafePointer<InversoDrumEditor> safe(this);
 chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
 [safe,pad](const juce::FileChooser& fc){
  if(safe==nullptr)return;
  const auto file=fc.getResult();
  if(!file.existsAsFile())return;
  if(safe->processor.loadPadFile(pad,file)){
   safe->selectPad(pad);
   safe->processor.auditionPad(pad);
  }else juce::AlertWindow::showMessageBoxAsync(
     juce::MessageBoxIconType::WarningIcon,"INVERSO DRUM",
     "The selected file could not be imported: "+file.getFullPathName());
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

void InversoDrumEditor::fileDragEnter(const juce::StringArray&,int x,int y){
 dragOver=true;dragX=x;dragY=y;repaint();
}
void InversoDrumEditor::fileDragMove(const juce::StringArray&,int x,int y){
 dragOver=true;dragX=x;dragY=y;repaint();
}
void InversoDrumEditor::fileDragExit(const juce::StringArray&){dragOver=false;repaint();}
