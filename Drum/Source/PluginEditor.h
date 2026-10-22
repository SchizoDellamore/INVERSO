#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SampleLibraries.h"
#include "LaneActions.h"
#include <array>
#include <memory>
#include <functional>

class InversoMidiDragButton final : public juce::TextButton {
public:
 using juce::TextButton::TextButton;
 std::function<void(juce::Component*)> onStartExternalDrag;
 void mouseDown(const juce::MouseEvent& e) override {
  dragStarted=false;
  juce::TextButton::mouseDown(e);
 }
 void mouseDrag(const juce::MouseEvent& e) override {
  if(!dragStarted && e.getDistanceFromDragStart()>8){
   dragStarted=true;
   if(onStartExternalDrag)onStartExternalDrag(this);
  }
 }
private:
 bool dragStarted=false;
};

class InversoLaneButton final : public juce::TextButton {
public:
 using juce::TextButton::TextButton;
 std::function<void(juce::Component*)> onContextMenu, onShiftSelect;
 void mouseDown(const juce::MouseEvent& event) override {
  if(event.mods.isRightButtonDown()){
   if(onContextMenu)onContextMenu(this);
   return;
  }
  if(event.mods.isLeftButtonDown() && event.mods.isShiftDown()){
   if(onShiftSelect)onShiftSelect(this);
   return;
  }
  juce::TextButton::mouseDown(event);
 }
 void mouseUp(const juce::MouseEvent& event) override {
  if(event.mods.isRightButtonDown() || event.mods.isShiftDown())return;
  juce::TextButton::mouseUp(event);
 }
};

class InversoDrumEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer, public juce::FileDragAndDropTarget {
public:
 explicit InversoDrumEditor(InversoDrumProcessor&);
 ~InversoDrumEditor() override=default;
 void paint(juce::Graphics&) override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
 void fileDragEnter(const juce::StringArray&,int,int) override;
 void fileDragMove(const juce::StringArray&,int,int) override;
 void fileDragExit(const juce::StringArray&) override;
private:
 class StepGrid final : public juce::Component {
 public:
  explicit StepGrid(InversoDrumEditor& e):editor(e){}
  void paint(juce::Graphics&) override;
  void mouseDown(const juce::MouseEvent&) override;
  void mouseDrag(const juce::MouseEvent&) override;
  void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
 private:
  bool locate(const juce::Point<float>&,int& lane,int& step) const noexcept;
  InversoDrumEditor& editor;
  int previousCell=-1;
 };
 class Waveform final : public juce::Component {
 public:
  explicit Waveform(InversoDrumEditor& e):editor(e){}
  void paint(juce::Graphics&) override;
 private:
  InversoDrumEditor& editor;
 };
 void timerCallback() override;
 void selectPad(int);
 void selectStep(int pad,int step);
 void refreshStepVelocity();
 void refreshStepEffects();
 void showLaneMenu(int pad,juce::Component* source);
 void loadSampleOnPad(int pad);
 void captureLaneUndo(int pad);
 void undoLaneEdit(int pad);
 void clearSelections();
 void selectAllLaneSteps(int pad);
 void syncLaneLength();
 void updateUi();
 void updateTabs();
 void loadSelected();
 void exportMidi();
 double viewStartPPQ() const noexcept;
 void createSlider(juce::Slider&,juce::Label&,const char*,double,double,double);
 InversoDrumProcessor& processor;
 StepGrid grid;
 Waveform waveform;
 std::array<juce::TextButton,8> patternButtons,pageButtons;
 std::array<InversoLaneButton,16> padButtons;
 juce::ToggleButton chain{"CHAIN A-H"};
 juce::Slider swing,level,pan,laneLength,padVelocity,stepVelocity;
 juce::Label padVelocityLabel;
 juce::Label stepVelocityLabel,stepInfo;
 juce::TextButton clearStep{"CLEAR STEP"},resetStepFx{"RESET STEP FX"};
 std::array<juce::Slider,3> stepFxSliders;
 std::array<juce::Label,3> stepFxLabels;
 juce::ComboBox laneRate,laneFeel,delayRate,delayFeel,viewBars;
 juce::Label swingLabel,levelLabel,panLabel,lengthLabel,rateLabel,feelLabel;
 juce::Label selectedSample,tempoLabel,delayRateLabel,delayFeelLabel,viewLabel;
 juce::TextButton loadButton{"LOAD SAMPLE"},libraryButton{"DAW LIBRARIES"};
 InversoMidiDragButton exportButton{"EXPORT / DRAG MIDI"};
 InversoSampleLibraries sampleLibrary;
 juce::TextButton sampleTab{"SAMPLE EDIT"},fxTab{"EFFECTS"};
 juce::ToggleButton reverse{"REVERSE"};
 std::array<juce::Slider,5> sampleSliders;
 std::array<juce::Label,5> sampleLabels;
 std::array<juce::Slider,7> fxSliders;
 std::array<juce::Label,7> fxLabels;
 std::unique_ptr<juce::FileChooser> chooser;
 inverso_drum::LaneUndoHistory laneUndo;
 inverso_drum::StepSelection stepSelection;
 int selectionPattern=0;
 bool velocityDrag=false,padVelocityDrag=false,stepFxDrag=false;
 int selectedPad=0,selectedStep=0,currentPage=0,displayedBars=4;
 bool dragOver=false;int dragX=0,dragY=0;
 bool showFx=false,refreshing=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoDrumEditor)
};
