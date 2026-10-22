#pragma once
#include <JuceHeader.h>
#include "AudioEdit.h"
#include <atomic>
#include <deque>

class WaveMoverProcessor final:public juce::AudioProcessor {
public:
    struct Snapshot {wavemover::Audio audio,original;std::vector<std::size_t>markers;wavemover::Range selection;juce::String name="Sem audio",action;std::uint64_t revision=0;};
    WaveMoverProcessor();
    ~WaveMoverProcessor()override=default;
    const juce::String getName()const override{return "INVERSO WaveMover";}
    void prepareToPlay(double,int)override;
    void releaseResources()override;
    bool isBusesLayoutSupported(const BusesLayout&)const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
    juce::AudioProcessorEditor*createEditor()override;
    bool hasEditor()const override{return true;}
    // The editor can audition a sample while the track input is silent.
    double getTailLengthSeconds()const override{return std::numeric_limits<double>::infinity();}
    bool acceptsMidi()const override{return false;}
    bool producesMidi()const override{return false;}
    bool isMidiEffect()const override{return false;}
    int getNumPrograms()override{return 1;}
    int getCurrentProgram()override{return 0;}
    void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return {};}
    void changeProgramName(int,const juce::String&)override{}
    void getStateInformation(juce::MemoryBlock&)override;
    void setStateInformation(const void*,int)override;
    Snapshot snapshot()const;
    void load(wavemover::Clip,juce::String);
    void commit(wavemover::Clip,std::vector<std::size_t>,wavemover::Range,juce::String);
    void setSelection(wavemover::Range);
    void setMarkers(std::vector<std::size_t>,juce::String);
    void undo();void redo();void restoreOriginal();
    bool canUndo()const;bool canRedo()const;
    void play(bool original=false,bool loop=false);void stop();
    bool playing()const{return isPlaying.load();}
    double playbackFraction()const{return playFraction.load();}
    void startCapture();bool finishCapture();
    bool recording()const{return isRecording.load();}
    bool captureReady()const{return readyCapture.load();}
    double capturedSeconds()const{return static_cast<double>(capturedFrames.load())/captureRate.load();}
    double hostBpm()const{return bpm.load();}
    static wavemover::Clip readAudio(const juce::File&);
    static void writeAudio(const juce::File&,const wavemover::Clip&);
private:
    mutable juce::CriticalSection modelLock;
    Snapshot model;
    std::deque<Snapshot> undoStack,redoStack;
    void remember();void publish();
    wavemover::Audio audition;
    std::unique_ptr<wavemover::Clip>capture;
    double playPosition=0;wavemover::Range playRange;bool loopPlay=false;
    std::atomic<bool>isPlaying{false},isRecording{false},readyCapture{false};
    std::atomic<std::size_t>capturedFrames{0};
    std::atomic<double>hostRate{48000},captureRate{48000},playFraction{0},bpm{120};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveMoverProcessor)
};
