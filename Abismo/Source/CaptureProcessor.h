#pragma once
#include <JuceHeader.h>
#include "DSP/CaptureFx.h"
#include "DSP/CaptureParams.h"
#include <array>
#include <atomic>
#include <memory>

// INVERSO CAPTURE: an insert on the track. It keeps the last seconds of what passes (while the host plays,
// or while REC is on), or opens a file; you edit it in place (stretch, pitch, reverse, tape stop and effects),
// hear it on top of the track, and take the edited sound away as a WAV file to drag onto another track.
// The edit is rendered in the background whenever a control moves: what you hear is what the file holds.
class InversoCaptureProcessor final : public juce::AudioProcessor, private juce::Thread {
public:
 InversoCaptureProcessor();
 ~InversoCaptureProcessor() override;
 void prepareToPlay(double sampleRate,int samplesPerBlock) override;
 void releaseResources() override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 using AudioProcessor::processBlock;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "INVERSO CAPTURE";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0.;}
 int getNumPrograms() override {return 1;}
 int getCurrentProgram() override {return 0;}
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override {return "Default";}
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;

 // ---- from the editor (message thread)
 bool capture();                       // the last CAPLEN seconds of the track
 void startRecording();
 bool stopRecording();                 // the clip is what passed since startRecording()
 bool isRecording() const { return recording.load(); }
 void play();
 void stopPlaying();
 bool isPlaying() const { return playing.load(); }
 double playPosition() const { return position.load(); }
 void bake();                          // keep the edit: the clip becomes what you hear
 bool canUndo() const;
 void undo();
 void normalizeClip();
 std::shared_ptr<const inverso_abismo::capture::Clip> clip() const;      // what was captured or opened
 std::shared_ptr<const inverso_abismo::capture::Clip> rendered() const;  // what the edit makes of it (may be a moment behind)
 std::shared_ptr<const inverso_abismo::capture::Clip> renderedNow();     // the edit as it is now (renders here if the background one is behind)
 unsigned clipVersion() const { return version.load(); }
 unsigned renderedVersion() const { return renderedVer.load(); }
 bool isRendering() const { return rendering.load() || renderedSerial.load()!=requestSerial.load(); }
 inverso_abismo::capture::RenderParams currentRender() const;
 bool loadFile(const juce::File&,juce::String& message);  // false: message says why; true: message may say what was left out
 bool exportWav(const juce::File&);    // the edited clip as a 24-bit WAV (nothing written if there is no clip)
 juce::File makeDragFile();            // a fresh WAV in the temp folder, ready to be dragged out
 float inputLevel() const { return level.load(); }
 double ringSeconds() const;
 void housekeeping();                  // frees the clips the audio thread is done with (the editor's timer)

 juce::AudioProcessorValueTreeState apvts;
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
private:
 class Listener;
 void run() override;                  // the background renderer
 void publishRendered(std::shared_ptr<inverso_abismo::capture::Clip>, unsigned serial);
 void setClip(std::shared_ptr<inverso_abismo::capture::Clip>, bool resetRegion);
 float raw(int p) const { return rawParams[static_cast<std::size_t>(p)]->load(std::memory_order_relaxed); }
 void resetEditParams();

 std::array<std::atomic<float>*,inverso_abismo::capture::P::Count> rawParams{};
 inverso_abismo::capture::Ring ring;
 // The message thread owns the clips (the source, the rendered one, and the ones they replaced until the audio
 // thread is done with them); the audio thread is handed plain pointers, lock-free, and says when it is in a block.
 mutable juce::CriticalSection clipLock;
 std::shared_ptr<inverso_abismo::capture::Clip> source, renderedClip;
 std::vector<std::shared_ptr<inverso_abismo::capture::Clip>> retired;
 std::atomic<const inverso_abismo::capture::Clip*> audioSource{nullptr}, audioRendered{nullptr};
 std::atomic<unsigned> audioGen{0};   // bumped with every pointer handed over (a new clip may sit where an old one was)
 std::atomic<bool> blockActive{false};
 juce::CriticalSection ringLock;       // the ring is reallocated by prepareToPlay and read by capture()
 juce::String cachedFlac; unsigned cachedVersion=~0u; // the saved clip, encoded once per version
 struct Undo { std::shared_ptr<inverso_abismo::capture::Clip> clip; std::array<float,inverso_abismo::capture::P::Count> params{}; };
 std::unique_ptr<Undo> undoState;
 inverso_abismo::capture::Player player;
 const inverso_abismo::capture::Clip* playerClip=nullptr;
 unsigned playerGen=0;
 bool playerOriginal=false;
 std::atomic<unsigned> version{0}, renderedVer{0};
 std::atomic<unsigned> requestSerial{1}, renderedSerial{0};   // a control moved / what the rendered clip was made for
 std::atomic<juce::int64> lastRequestMs{0};
 std::atomic<bool> rendering{false};
 std::atomic<int> command{0};          // 1 play, 2 stop (read by the audio thread)
 std::atomic<bool> playing{false}, recording{false};
 std::atomic<std::uint64_t> recStart{0};
 std::atomic<double> position{0.0};
 std::atomic<float> level{0.f};
 std::array<float,2048> scratchL{},scratchR{};   // the audio thread's own
 std::unique_ptr<Listener> listener;
 juce::CriticalSection renderLock;     // one render publishes at a time (background or on demand)
 double rate=48000.0;
 std::atomic<bool> prepared{false};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoCaptureProcessor)
};
