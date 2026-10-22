#pragma once
#include "PluginProcessor.h"
#include <functional>

class WaveView final:public juce::Component {
public:
    explicit WaveView(WaveMoverProcessor&p):processor(p){setMouseCursor(juce::MouseCursor::IBeamCursor);}
    void refresh();
    void paint(juce::Graphics&)override;
    void mouseDown(const juce::MouseEvent&)override;
    void mouseDrag(const juce::MouseEvent&)override;
    void mouseUp(const juce::MouseEvent&)override;
    void mouseDoubleClick(const juce::MouseEvent&)override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override;
    std::function<void()>onContext;
    std::function<void(bool,double)>onFade;
    std::function<void()>onSelection;
    std::size_t cursor=0;
    double viewStart=0,viewSize=1;
    bool editable=true;
private:
    WaveMoverProcessor&processor;
    struct Peak{float low=0,high=0;};
    std::vector<std::vector<Peak>>peaks;
    wavemover::Audio cachedAudio;
    std::size_t at(float x)const;
    std::size_t anchor=0;
    int fadeMode=0;double fadeMs=0;bool dragged=false;
};
class ExportDragButton final:public juce::TextButton {
public:
    ExportDragButton():TextButton("Preparar arraste"){}
    juce::File readyFile;
    void mouseDrag(const juce::MouseEvent&e)override{if(readyFile.existsAsFile()&&e.getDistanceFromDragStart()>5&&!started){started=true;juce::DragAndDropContainer::performExternalDragDropOfFiles({readyFile.getFullPathName()},false,this,[safe=juce::Component::SafePointer<ExportDragButton>(this)]{if(safe)safe->started=false;});}}
    void mouseUp(const juce::MouseEvent&e)override{if(!started)juce::TextButton::mouseUp(e);started=false;}
private:bool started=false;
};
class WaveMoverEditor final:public juce::AudioProcessorEditor,private juce::Timer,public juce::FileDragAndDropTarget {
public:
    explicit WaveMoverEditor(WaveMoverProcessor&);
    ~WaveMoverEditor()override;
    void paint(juce::Graphics&)override;
    void resized()override;
    bool keyPressed(const juce::KeyPress&)override;
    bool isInterestedInFileDrag(const juce::StringArray&)override;
    void filesDropped(const juce::StringArray&,int,int)override;
private:
    WaveMoverProcessor&processor;WaveView wave;
    juce::LookAndFeel_V4 style;
    juce::TooltipWindow tips{this,650};juce::ThreadPool pool{1};
    juce::TextButton open{"Abrir audio"},capture{"Capturar pista"},play{"Ouvir"},stop{"Parar"},original{"Ouvir original"};
    juce::ToggleButton loop{"Loop"},exportSelection{"Exportar selecao"};
    juce::TextButton all{"Selecionar tudo"},split{"Split no cursor"},grid{"Split grade"},attacks{"Split ataques"},clear{"Limpar splits"},undo{"Desfazer"},redo{"Refazer"};
    juce::TextButton cut{"Apagar"},copy{"Copiar"},paste{"Colar"},duplicate{"Duplicar"},trim{"Manter selecao"},crossfade{"Crossfade no cursor"},restore{"Restaurar original"};
    juce::ComboBox effects,gridDivision;juce::Slider a,b,bpm,seamMs;juce::Label labelA,labelB,labelBpm,seamLabel,information,status,details;
    juce::TextEditor seed;
    juce::TextButton apply{"Aplicar a selecao"},save{"Exportar WAV"},fit{"Ver tudo"};ExportDragButton drag;
    std::unique_ptr<juce::FileChooser>chooser;
    wavemover::Audio clipboard;
    std::vector<wavemover::Op>operations;
    bool busy=false;std::uint64_t seenRevision=std::numeric_limits<std::uint64_t>::max();
    void timerCallback()override;void updateControls();void configureEffect();void openFile(const juce::File&);
    void work(juce::String,std::function<std::function<void()>()>);
    void applyOperation(wavemover::Op,wavemover::Parameters,juce::String);
    void copyAudio();void pasteAudio();void contextMenu();void exportAudio(bool);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveMoverEditor)
};
