#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <functional>

// Fine wheel adjustments remain attached to JUCE host parameters through SliderAttachment.
// Hovering the slider is sufficient; Shift grants an additional 5x precision.
class InversoFineSlider final : public juce::Slider {
public:
    enum class ParameterKind { Frequency, Gain, Q };
    explicit InversoFineSlider(ParameterKind kind) : kind(kind) {}
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    ParameterKind kind;
};

class InversoPlot : public juce::Component, private juce::Timer {
public:
    explicit InversoPlot(InversoEQAudioProcessor&);
    ~InversoPlot() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setSpectrumMode(int mode) { spectrumMode = mode; processor.setSpectrumCaptureEnabled(mode != 0); repaint(); }

    std::function<void(int)> chooseBand;
    std::function<void(int, double, double)> moveBand;
    std::function<void(int, bool)> editGesture;
    std::function<void(double, double)> createBand;
    // target 0=gain 1=Q 2=frequency. The wheel only applies near an existing point.
    std::function<void(int, int, double, bool)> wheelBand;

private:
    void timerCallback() override;
    void analyse(const std::array<float,4096>& samples, std::array<float,2049>& levels);
    double xToHz(float x) const noexcept;
    float hzToX(double hz) const noexcept;
    float gainToY(double gain) const noexcept;
    double yToGain(float y) const noexcept;
    juce::Rectangle<float> plotBounds() const noexcept;
    void drawSpectrum(juce::Graphics&, const std::array<float,2049>&, juce::Colour);
    InversoEQAudioProcessor& processor;
    juce::dsp::FFT fft {12}; // 4096 points, better low-frequency discrimination
    std::array<float,8192> fftBuffer {};
    std::array<float,4096> hannWindow {};
    std::array<float,2049> preLevels {}, postLevels {};
    int spectrumMode = 2; // 0:off 1:pre 2:post 3:both
    int dragging = -1;
};

class InversoEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer {
public:
    explicit InversoEQAudioProcessorEditor(InversoEQAudioProcessor&);
    ~InversoEQAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void chooseBand(int index);
    void moveBand(int index, double frequency, double gain);
    void gestureBand(int index, bool beginning);
    void createBand(double frequency, double gain);
    void adjustBandWithWheel(int index, int target, double steps, bool fine);
    void setParameter(const juce::String&, float realValue);
    void timerCallback() override;
    void styleSlider(juce::Slider&, const juce::String& suffix = "");
    void styleLabel(juce::Label&, const juce::String&);

    InversoEQAudioProcessor& processor;
    InversoPlot plot;
    juce::LookAndFeel_V4 theme;

    juce::Label heading, statusLabel, hpLabel, lpLabel, bandLabel,
                frequencyLabel, gainLabel, qLabel, outputLabel, ceilingLabel,
                analysisLabel, hpCutoffLabel, lpCutoffLabel;
    InversoFineSlider hpHz {InversoFineSlider::ParameterKind::Frequency};
    InversoFineSlider lpHz {InversoFineSlider::ParameterKind::Frequency};
    InversoFineSlider bandHz {InversoFineSlider::ParameterKind::Frequency};
    InversoFineSlider bandDb {InversoFineSlider::ParameterKind::Gain};
    InversoFineSlider bandQ {InversoFineSlider::ParameterKind::Q};
    InversoFineSlider outputDb {InversoFineSlider::ParameterKind::Gain};
    InversoFineSlider ceilingDb {InversoFineSlider::ParameterKind::Gain};
    juce::ToggleButton hpEnabled, lpEnabled, bandEnabled, safetyEnabled;
    juce::ComboBox hpSlope, lpSlope, spectrumMode;
    std::array<juce::TextButton, inverso::kBands> bandButtons {};
    std::unique_ptr<SliderAttachment> hpAttachment, lpAttachment, outputAttachment,
                                      ceilingAttachment, freqAttachment, dbAttachment, qAttachment;
    std::unique_ptr<ButtonAttachment> hpButtonAttachment, lpButtonAttachment,
                                      safetyButtonAttachment, bandButtonAttachment;
    std::unique_ptr<ComboAttachment> hpSlopeAttachment, lpSlopeAttachment;
    int selectedBand = 3;
    float holdPre = 0.f, holdOut = 0.f, holdReduction = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoEQAudioProcessorEditor)
};
