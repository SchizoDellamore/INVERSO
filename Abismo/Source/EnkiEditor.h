#pragma once
#include <JuceHeader.h>
#include "SoftUI.h"
#include "EnkiProcessor.h"

// The generator strip: category, GENERATE, MUTATE (and how much), history, favourites.
class EnkiGeneratorBar final : public juce::Component {
public:
 explicit EnkiGeneratorBar(InversoEnkiProcessor&);
 void paint(juce::Graphics&) override;
 void resized() override;
 void refresh();
private:
 InversoEnkiProcessor& processor;
 juce::ComboBox category;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> categoryAttach;
 juce::TextButton generateButton{"GENERATE"}, mutateButton{"MUTATE"}, backButton{"<"}, forwardButton{">"};
 juce::TextButton saveButton{"SAVE"}, favoritesButton{"FAVORITES"};
 soft::Knob amount;
 std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach;
 juce::String position;
};

// ENKI's sound library: your sounds (imported from the computer, copied into
// ENKI's own folder) and Freesound (free CC0 sounds by search).
class EnkiSoundsPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
 explicit EnkiSoundsPanel(InversoEnkiProcessor&);
 ~EnkiSoundsPanel() override;
 void paint(juce::Graphics&) override;
 void paintOverChildren(juce::Graphics&) override;
 void resized() override;
 void mouseUp(const juce::MouseEvent&) override;
private:
 int getNumRows() override;
 void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
 void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
 void timerCallback() override;
 void setTab(int);
 void refreshFolders();
 void refreshList();
 void sendToSource(int source);
 int recordingRows() const;
 juce::Rectangle<int> tabArea(int) const;
 InversoEnkiProcessor& processor;
 int tab = 0, seenIndex = -1, seenResults = -1;
 std::vector<int> shown; // library indices in the list (tab 0)
 juce::ListBox list{"sounds", this};
 juce::ComboBox draw;
 juce::TextButton importFiles{"IMPORT FILES"}, importFolder{"IMPORT FOLDER"}, openFolder{"OPEN FOLDER"};
 juce::TextButton toA{"TO A"}, toB{"TO B"};
 juce::TextEditor query, key;
 juce::TextButton searchButton{"SEARCH"}, saveKey{"SAVE KEY"}, getKey{"GET A FREE KEY"};
 juce::TextButton getToA{"GET > A"}, getToB{"GET > B"}, getOnly{"DOWNLOAD"};
 juce::String status;
 struct Waiting { int id = 0; int source = -1; double since = 0.0; } waiting;
 std::unique_ptr<juce::FileChooser> chooser;
 juce::Image cardCache;
};

class InversoEnkiEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
 explicit InversoEnkiEditor(InversoEnkiProcessor&);
 ~InversoEnkiEditor() override;
 void resized() override;
private:
 void timerCallback() override;
 float value(int param) const;
 void chooseSample(int source);
 InversoEnkiProcessor& processor;
 soft::Look look;
 soft::Body body;
 EnkiGeneratorBar bar;
 EnkiSoundsPanel sounds;
 std::vector<std::unique_ptr<soft::Module>> modules;
 std::unique_ptr<soft::Master> master;
 std::array<juce::TextButton, 5> locks;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 5> lockAttach;
 std::array<juce::TextButton, 2> sampleButtons;
 struct Recipe final : public juce::Component { juce::String text; int at = 0, of = 0; void paint(juce::Graphics&) override; } recipe;
 juce::TooltipWindow tooltips{this, 600};
 float animation = 0.f;
 int seenVersion = -1;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoEnkiEditor)
};
