#include "PluginEditor.h"
#include <cmath>
namespace {
const auto bg=juce::Colour::fromRGB(14,17,20);
const auto panel=juce::Colour::fromRGB(24,29,33);
const auto text=juce::Colour::fromRGB(235,228,215);
const auto muted=juce::Colour::fromRGB(154,160,161);
const auto orange=juce::Colour::fromRGB(245,132,42);
const char* padNames[]={"KICK","SNARE","CLOSED HH","OPEN HH","TOM 1","TOM 2","CLAP","PERC 1",
 "KICK 2","SNARE 2","HAT 2","HAT 3","TOM 3","PERC 2","PERC 3","PERC 4"};
}
InversoDrumEditor::InversoDrumEditor(InversoDrumProcessor& p)
:AudioProcessorEditor(&p),processor(p),grid(*this){
 setSize(1060,735);
 addAndMakeVisible(grid);
 for(int i=0;i<8;++i){
  auto& b=patternButtons[static_cast<size_t>(i)];
  b.setButtonText(juce::String::charToString('A'+i));
  b.onClick=[this,i]{processor.engine.selectedPattern.store(i);updateUi();};
  addAndMakeVisible(b);
 }
 for(int i=0;i<16;++i){
  auto& b=padButtons[static_cast<size_t>(i)];
  b.setButtonText(padNames[i]);
  b.onClick=[this,i]{selectPad(i);processor.auditionPad(i);};
  addAndMakeVisible(b);
 }
 chain.setToggleState(p.engine.chain.load(),juce::dontSendNotification);
 chain.onClick=[this]{processor.engine.chain.store(chain.getToggleState());updateUi();};
 addAndMakeVisible(chain);
 swing.setRange(0,100,1);swing.setValue(p.engine.swing.load()*100,juce::dontSendNotification);
 swing.setSliderStyle(juce::Slider::LinearHorizontal);
 swing.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,24);
 swing.onValueChange=[this]{processor.engine.swing.store(static_cast<float>(swing.getValue()/100.));};
 swing.setColour(juce::Slider::trackColourId,orange);addAndMakeVisible(swing);
 swingLabel.setText("SWING",juce::dontSendNotification);addAndMakeVisible(swingLabel);
 level.setRange(0,200,1);level.setValue(80,juce::dontSendNotification);
 level.setSliderStyle(juce::Slider::LinearHorizontal);
 level.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,24);
 level.setColour(juce::Slider::trackColourId,orange);
 level.onValueChange=[this]{processor.engine.gain[selectedPad].store(static_cast<float>(level.getValue()/100.));};
 addAndMakeVisible(level);
 levelLabel.setText("PAD LEVEL",juce::dontSendNotification);addAndMakeVisible(levelLabel);
 pan.setRange(-100,100,1);pan.setValue(0,juce::dontSendNotification);
 pan.setSliderStyle(juce::Slider::LinearHorizontal);
 pan.setTextBoxStyle(juce::Slider::TextBoxRight,false,54,24);
 pan.setColour(juce::Slider::trackColourId,orange);
 pan.onValueChange=[this]{processor.engine.pan[selectedPad].store(static_cast<float>(pan.getValue()/100.));};
 addAndMakeVisible(pan);
 panLabel.setText("PAD PAN",juce::dontSendNotification);addAndMakeVisible(panLabel);
 selectedSample.setColour(juce::Label::textColourId,text);addAndMakeVisible(selectedSample);
 tempoLabel.setColour(juce::Label::textColourId,muted);addAndMakeVisible(tempoLabel);
 loadButton.onClick=[this]{loadSelected();};addAndMakeVisible(loadButton);
 exportButton.onClick=[this]{exportMidi();};addAndMakeVisible(exportButton);
 selectPad(0);updateUi();startTimerHz(16);
}
void InversoDrumEditor::paint(juce::Graphics& g){
 g.fillAll(bg);
 g.setColour(text);g.setFont(juce::Font(juce::FontOptions(25.f,juce::Font::bold)));
 g.drawText("INVERSO  /  DRUM",24,16,630,44,juce::Justification::centredLeft);
 g.setColour(orange);g.fillRoundedRectangle(25.f,65.f,98.f,3.f,1.5f);
 g.setColour(muted);g.setFont(12.f);
 g.drawText("16 PADS  /  16 STEPS  /  8 PATTERNS  /  HOST SYNC  /  NO CLOUD ACCOUNT",24,70,850,22,juce::Justification::centredLeft);
 g.setColour(muted);g.drawText("PATTERN",24,104,120,18,juce::Justification::centredLeft);
 g.drawText("BEAT GRID  ·  LEFT CLICK TO TOGGLE  /  RIGHT CLICK FOR VELOCITY",186,172,830,18,juce::Justification::centredLeft);
 g.setColour(panel);g.fillRoundedRectangle(19.f,566.f,1023.f,126.f,9.f);
 g.setColour(orange);g.setFont(13.f);
 g.drawText("SAMPLE / PAD CONTROLS",28,577,420,20,juce::Justification::centredLeft);
 g.setColour(muted);g.setFont(11.5f);
 g.drawText("C1–D#2: TRIGGER PADS  |  CUBASE PLAY: RUN SEQUENCER  |  MIDI EXPORT: SELECTED PATTERN OR CHAIN",23,703,1010,18,juce::Justification::centredLeft);
}
void InversoDrumEditor::resized(){
 for(int i=0;i<8;++i)patternButtons[i].setBounds(115+i*58,100,52,30);
 chain.setBounds(600,100,100,30);
 swingLabel.setBounds(712,103,65,24);swing.setBounds(773,100,260,30);
 for(int i=0;i<16;++i)padButtons[i].setBounds(22,202+i*22,154,21);
 grid.setBounds(185,202,848,352);
 selectedSample.setBounds(30,605,320,27);
 loadButton.setBounds(30,645,220,32);
 levelLabel.setBounds(300,603,110,24);level.setBounds(300,634,198,30);
 panLabel.setBounds(520,603,100,24);pan.setBounds(520,634,194,30);
 exportButton.setBounds(798,640,228,34);
 tempoLabel.setBounds(795,600,231,24);
}
void InversoDrumEditor::selectPad(int pad){
 selectedPad=juce::jlimit(0,15,pad);
 level.setValue(processor.engine.gain[selectedPad].load()*100.,juce::dontSendNotification);
 pan.setValue(processor.engine.pan[selectedPad].load()*100.,juce::dontSendNotification);
 selectedSample.setText("PAD "+juce::String(selectedPad+1)+": "+processor.sampleName(selectedPad),juce::dontSendNotification);
 updateUi();
}
void InversoDrumEditor::updateUi(){
 const int active=processor.engine.selectedPattern.load();
 for(int i=0;i<8;++i){
  patternButtons[i].setColour(juce::TextButton::buttonColourId,i==active?orange:panel);
  patternButtons[i].setColour(juce::TextButton::textColourOffId,i==active?juce::Colours::black:text);
 }
 for(int i=0;i<16;++i){
  padButtons[i].setColour(juce::TextButton::buttonColourId,i==selectedPad?orange:panel);
  padButtons[i].setColour(juce::TextButton::textColourOffId,i==selectedPad?juce::Colours::black:text);
 }
 grid.repaint();
}
void InversoDrumEditor::timerCallback(){
 grid.repaint();
 tempoLabel.setText("HOST "+juce::String(processor.hostBpm.load(),1)+" BPM  /  PEAK "
 +juce::String(juce::Decibels::gainToDecibels(processor.outputPeak.load(),-100.f),1)+" dBFS",juce::dontSendNotification);
}
void InversoDrumEditor::StepGrid::paint(juce::Graphics& g){
 const int pat=editor.processor.engine.selectedPattern.load();
 const int play=editor.processor.engine.currentStep.load();
 const bool playingPattern=editor.processor.engine.currentPattern.load()==pat;
 constexpr float w=53.f,h=22.f;
 for(int lane=0;lane<16;++lane)for(int step=0;step<16;++step){
  const float x=static_cast<float>(step)*w+2.f,y=static_cast<float>(lane)*h+1.f;
  const int vel=editor.processor.engine.get(pat,lane,step);
  const bool on=vel>0;
  const bool highlight=play==step&&playingPattern;
  juce::Colour color=on?orange.withAlpha(.34f+.66f*vel/127.f):juce::Colour::fromRGB((step%4)==0?49:34,40,42);
  g.setColour(color);g.fillRoundedRectangle(x,y,w-4.f,h-3.f,3.f);
  if(highlight){g.setColour(juce::Colours::white.withAlpha(.9f));g.drawRoundedRectangle(x,y,w-4.f,h-3.f,3.f,1.3f);}
  if(lane==editor.selectedPad && !highlight){g.setColour(orange.withAlpha(.55f));g.drawRoundedRectangle(x,y,w-4.f,h-3.f,3.f,1.f);}
 }
}
void InversoDrumEditor::StepGrid::mouseDown(const juce::MouseEvent& ev){
 previousCell=-1;mouseDrag(ev);
}
void InversoDrumEditor::StepGrid::mouseDrag(const juce::MouseEvent& ev){
 const int step=static_cast<int>(ev.position.x/53.f);
 const int lane=static_cast<int>(ev.position.y/22.f);
 if(step<0||step>=16||lane<0||lane>=16)return;
 const int cell=lane*16+step;if(cell==previousCell)return;previousCell=cell;
 const int pat=editor.processor.engine.selectedPattern.load();
 const int previous=editor.processor.engine.get(pat,lane,step);
 const int next=ev.mods.isRightButtonDown()?(previous==0?55:previous<80?90:previous<115?127:0):(previous?0:105);
 editor.processor.engine.set(pat,lane,step,next);
 editor.selectedPad=lane;editor.selectPad(lane);repaint();
}
bool InversoDrumEditor::isInterestedInFileDrag(const juce::StringArray& files){
 for(const auto& file:files){const auto ext=juce::File(file).getFileExtension().toLowerCase();if(ext==".wav"||ext==".aif"||ext==".aiff")return true;}
 return false;
}
void InversoDrumEditor::filesDropped(const juce::StringArray& files,int,int){
 if(files.size()>0 && processor.loadPadFile(selectedPad,juce::File(files[0])))selectPad(selectedPad);
}
void InversoDrumEditor::loadSelected(){
 chooser=std::make_unique<juce::FileChooser>("Load drum sample",juce::File{},"*.wav;*.aif;*.aiff");
 const int pad=selectedPad;
 juce::Component::SafePointer<InversoDrumEditor> safe(this);
 chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
 [safe,pad](const juce::FileChooser& fc){
  if(safe==nullptr)return;
  const auto file=fc.getResult();
  if(file.existsAsFile() && safe->processor.loadPadFile(pad,file))safe->selectPad(pad);
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
