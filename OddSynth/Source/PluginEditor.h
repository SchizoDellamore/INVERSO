#pragma once
#include "PluginProcessor.h"

class OddLook final:public juce::LookAndFeel_V4{
public:OddLook();void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&)override;
};
class OddKnob final:public juce::Component{
public:
    OddKnob(OddProcessor&,const juce::String&,const juce::String&);
    void resized()override;
    juce::Slider slider;
private:juce::Label label;std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
class OddRoute final:public juce::Component{
public:
    OddRoute(OddProcessor&,size_t);void resized()override;void paint(juce::Graphics&)override;void refresh();
private:
    OddProcessor& processor;size_t index;juce::Label name,liveValue;juce::Slider base,low,high;juce::ToggleButton lock{"Travar"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> baseA,lowA,highA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockA;
};
class OddSynthPanel final:public juce::Component{
public:explicit OddSynthPanel(OddProcessor&);void resized()override;void paint(juce::Graphics&)override;void refresh();
private:
    OddProcessor& processor;
    std::array<std::unique_ptr<OddRoute>,odd::dimensions> routes;
    juce::ComboBox mode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeA;
    OddKnob amount,rate;juce::ToggleButton freeze{"Congelar"},repeat{"Repetir ao iniciar DAW"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeA,repeatA;
    juce::TextButton random{"Variar timbre"},restart{"Reiniciar percurso"};
    juce::Slider seed;juce::Label seedLabel,help;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedA;
};
class OddWave final:public juce::Component{
public:explicit OddWave(OddProcessor&);void paint(juce::Graphics&)override;void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void refresh();
private:OddProcessor& processor;std::shared_ptr<const odd::Sample> sample;juce::Path waveform;float anchor=0;void select(float);
};
class OddSamplePanel final:public juce::Component,public juce::FileDragAndDropTarget{
public:explicit OddSamplePanel(OddProcessor&);~OddSamplePanel()override;void resized()override;void paint(juce::Graphics&)override;void refresh();
    bool isInterestedInFileDrag(const juce::StringArray&)override;void filesDropped(const juce::StringArray&,int,int)override;
private:
    void choose(bool exporting);void importFile(const juce::File&);
    OddProcessor& processor;OddWave wave;OddKnob sampleMix,speed,grain;
    juce::TextButton load{"Carregar audio"},save{"Exportar WAV"},all{"Selecionar tudo"},preview{"Tocar / parar C4"};
    juce::Label status,help;
    juce::ToggleButton granular{"Granular"},loop{"Loop"},reverse{"Reverse na reproducao"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> granularA,loopA,reverseA;
    std::array<juce::TextButton,7> edits;
    juce::Slider start,end;juce::Label startLabel,endLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startA,endA;
    std::unique_ptr<juce::FileChooser> chooser;bool previewing=false;
};
class OddEditor final:public juce::AudioProcessorEditor,private juce::Timer{
public:explicit OddEditor(OddProcessor&);~OddEditor()override;void paint(juce::Graphics&)override;void resized()override;
private:
    void timerCallback()override;void fileAction(int);
    OddProcessor& processor;OddLook look;juce::TooltipWindow tooltip{this,650};
    OddSynthPanel synth;OddSamplePanel samples;juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    std::array<std::unique_ptr<OddKnob>,4> sources;
    std::array<std::unique_ptr<OddKnob>,7> bottom;
    juce::ComboBox presets;juce::TextButton undo{"Desfazer"},redo{"Refazer"},save{"Salvar"},load{"Abrir"},capture{"Guardar audio recente"},panic{"PARAR"};
    juce::TextButton storeA{"Guardar A"},storeB{"Guardar B"},recallA{"Ouvir A"},recallB{"Ouvir B"};
    juce::Slider morph;juce::Label morphLabel,meter;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OddEditor)
};
