#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const auto background = juce::Colour::fromRGB(16,19,23);
const auto panel = juce::Colour::fromRGB(24,29,34);
const auto graph = juce::Colour::fromRGB(18,23,28);
const auto ink = juce::Colour::fromRGB(223,218,203);
const auto secondary = juce::Colour::fromRGB(146,158,162);
const auto grid = juce::Colour::fromRGB(48,57,63);
const auto aqua = juce::Colour::fromRGB(122,178,174);
const auto amber = juce::Colour::fromRGB(222,163,96);

float levelDb(float x) { return static_cast<float>(inverso::linearToDb(x)); }
}

void InversoFineSlider::mouseWheelMove(const juce::MouseEvent& event,
                                       const juce::MouseWheelDetails& wheel) {
    if (!isEnabled() || wheel.deltaY == 0.f) return;
    const double steps = juce::jlimit(-4.0, 4.0,
        static_cast<double>(wheel.deltaY) * (wheel.isReversed ? -5.0 : 5.0));
    const bool fine = event.mods.isShiftDown();
    const double current = getValue();
    double proposed = current;
    switch (kind) {
        case ParameterKind::Frequency:
            proposed *= std::pow(1.01, steps * (fine ? 0.2 : 1.0));
            break;
        case ParameterKind::Gain:
            proposed += steps * (fine ? 0.02 : 0.1);
            break;
        case ParameterKind::Q:
            proposed += steps * (fine ? 0.01 : 0.05);
            break;
    }
    // Each event is a complete automation gesture. SliderAttachment handles host notification.
    setValue(juce::jlimit(getMinimum(), getMaximum(), proposed), juce::sendNotificationSync);
}

InversoPlot::InversoPlot(InversoEQAudioProcessor& p) : processor(p) {
    preLevels.fill(-90.f); postLevels.fill(-90.f);
    for (int i=0; i<4096; ++i)
        hannWindow[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(static_cast<float>(2.0*inverso::kPi*i/4095.0));
    processor.setSpectrumCaptureEnabled(true);
    startTimerHz(20);
}

InversoPlot::~InversoPlot() {
    stopTimer();
    processor.setSpectrumCaptureEnabled(false);
}

juce::Rectangle<float> InversoPlot::plotBounds() const noexcept {
    return getLocalBounds().toFloat().reduced(15.f, 18.f);
}

double InversoPlot::xToHz(float x) const noexcept {
    const auto bounds = plotBounds();
    const double t = juce::jlimit(0.0, 1.0, static_cast<double>((x-bounds.getX()) / bounds.getWidth()));
    return 20.0 * std::pow(1000.0, t);
}
float InversoPlot::hzToX(double hz) const noexcept {
    const auto bounds = plotBounds();
    return bounds.getX() + bounds.getWidth() * static_cast<float>(std::log(std::clamp(hz,20.,20000.)/20.)/std::log(1000.));
}
float InversoPlot::gainToY(double gain) const noexcept {
    const auto bounds = plotBounds();
    return bounds.getY() + bounds.getHeight() * static_cast<float>((18.0-std::clamp(gain,-18.0,18.0))/36.0);
}
double InversoPlot::yToGain(float y) const noexcept {
    const auto bounds = plotBounds();
    return std::clamp(18.0 - 36.0 * (y-bounds.getY())/bounds.getHeight(), -18.0,18.0);
}

void InversoPlot::analyse(const std::array<float,4096>& signal, std::array<float,2049>& levels) {
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.f);
    for (int i=0; i<4096; ++i)
        fftBuffer[static_cast<std::size_t>(i)] = signal[static_cast<std::size_t>(i)] * hannWindow[static_cast<std::size_t>(i)];
    fft.performFrequencyOnlyForwardTransform(fftBuffer.data());
    for (int k=1; k<=2048; ++k) {
        // Decibels relative to full-scale sine, not LUFS.
        const auto amp = fftBuffer[static_cast<std::size_t>(k)] * (4.f/4096.f);
        const auto db = juce::jlimit(-90.f, 6.f, levelDb(amp));
        // Fast attack, slower release; spectrum follows the music without flickering.
        auto& previous = levels[static_cast<std::size_t>(k)];
        previous += (db > previous ? 0.42f : 0.15f) * (db - previous);
    }
}

void InversoPlot::timerCallback() {
    if (spectrumMode != 0) {
        std::array<float,4096> input {}, output {};
        processor.spectrumSnapshot(input,output);
        if (spectrumMode == 1 || spectrumMode == 3) analyse(input,preLevels);
        if (spectrumMode == 2 || spectrumMode == 3) analyse(output,postLevels);
    }
    repaint();
}

void InversoPlot::drawSpectrum(juce::Graphics& g, const std::array<float,2049>& data, juce::Colour colour) {
    const auto bounds = plotBounds();
    const auto sampleRate = processor.getSampleRate();
    if (sampleRate <= 0) return;
    juce::Path path;
    bool started = false;
    for (int x=0; x<static_cast<int>(bounds.getWidth()); x+=2) {
        const double hz = xToHz(bounds.getX() + static_cast<float>(x));
        if (hz > sampleRate * 0.5) break;
        // Interpolate adjacent FFT bins rather than snapping frequencies to a staircase.
        const double bin = juce::jlimit(1.0, 2048.0, hz * 4096.0 / sampleRate);
        const int left = juce::jlimit(1, 2048, static_cast<int>(bin));
        const int right = juce::jmin(2048, left+1);
        const float val = juce::jmap(static_cast<float>(bin-left), data[static_cast<std::size_t>(left)], data[static_cast<std::size_t>(right)]);
        const auto y = bounds.getBottom() - bounds.getHeight() * juce::jlimit(0.f,1.f,(val+90.f)/90.f);
        if (!started) {path.startNewSubPath(bounds.getX()+static_cast<float>(x),y); started=true;}
        else path.lineTo(bounds.getX()+static_cast<float>(x),y);
    }
    g.setColour(colour.withAlpha(0.46f));
    g.strokePath(path,juce::PathStrokeType(1.25f));
}

void InversoPlot::paint(juce::Graphics& g) {
    g.fillAll(graph);
    const auto b = plotBounds();
    g.setFont(11.f);
    const double ticks[] = {20,50,100,200,500,1000,2000,5000,10000,20000};
    for (const auto hz : ticks) {
        const auto x = hzToX(hz);
        g.setColour(grid); g.drawVerticalLine(static_cast<int>(x),b.getY(),b.getBottom());
        g.setColour(secondary.withAlpha(0.85f));
        const auto label = hz < 1000 ? juce::String(static_cast<int>(hz)) : juce::String(hz/1000., hz>=10000 ? 0:1)+"k";
        g.drawText(label,static_cast<int>(x)-18,static_cast<int>(b.getBottom()+2),36,14,juce::Justification::centred);
    }
    for (int db=-18; db<=18; db+=6) {
        const auto y = gainToY(db);
        g.setColour(db == 0 ? secondary.withAlpha(0.3f) : grid);
        g.drawHorizontalLine(static_cast<int>(y), b.getX(), b.getRight());
        g.setColour(secondary.withAlpha(0.75f));
        g.drawText(juce::String(db),0,static_cast<int>(y)-8,24,16,juce::Justification::centred);
    }

    if (spectrumMode==1 || spectrumMode==3) drawSpectrum(g,preLevels,juce::Colour::fromRGB(125,130,135));
    if (spectrumMode==2 || spectrumMode==3) drawSpectrum(g,postLevels,aqua);

    const auto settings = processor.currentSettings();
    const double sampleRate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000.;
    juce::Path response;
    for (int px=0; px<=static_cast<int>(b.getWidth()); px+=2) {
        const auto x = b.getX() + static_cast<float>(px);
        const double mag = inverso::responseDb(settings,sampleRate,xToHz(x));
        const auto y = gainToY(mag);
        if (px==0) response.startNewSubPath(x,y);
        else response.lineTo(x,y);
    }
    g.setColour(ink);
    g.strokePath(response,juce::PathStrokeType(2.2f,juce::PathStrokeType::curved));

    for (int i=0; i<inverso::kBands; ++i) {
        const auto& band = settings.bands[static_cast<std::size_t>(i)];
        if (!band.enabled) continue;
        const auto x = hzToX(band.frequency), y = gainToY(band.gainDb);
        g.setColour(ink);
        g.fillEllipse(x-5.f,y-5.f,10.f,10.f);
        g.setColour(graph);
        g.fillEllipse(x-2.f,y-2.f,4.f,4.f);
    }
    g.setColour(secondary);
    g.setFont(11.f);
    g.drawText("Hz",static_cast<int>(b.getRight()-28),static_cast<int>(b.getY()-15),26,14,juce::Justification::right);
}

void InversoPlot::mouseDown(const juce::MouseEvent& event) {
    dragging = -1;
    const auto settings = processor.currentSettings();
    float best = 18.f;
    for (int i=0; i<inverso::kBands; ++i) {
        const auto& band = settings.bands[static_cast<std::size_t>(i)];
        if (!band.enabled) continue;
        const auto dx = event.position.x - hzToX(band.frequency);
        const auto dy = event.position.y - gainToY(band.gainDb);
        const auto distance = std::sqrt(dx*dx + dy*dy);
        if (distance < best) {best=distance; dragging=i;}
    }
    if (dragging>=0) {
        if (chooseBand) chooseBand(dragging);
        if (editGesture) editGesture(dragging,true);
    }
}
void InversoPlot::mouseDrag(const juce::MouseEvent& event) {
    if (dragging>=0 && moveBand)
        moveBand(dragging,xToHz(event.position.x),yToGain(event.position.y));
}
void InversoPlot::mouseUp(const juce::MouseEvent&) {
    if (dragging>=0 && editGesture) editGesture(dragging,false);
    dragging=-1;
}
void InversoPlot::mouseDoubleClick(const juce::MouseEvent& event) {
    if (plotBounds().contains(event.position) && createBand)
        createBand(xToHz(event.position.x), yToGain(event.position.y));
}

void InversoPlot::mouseWheelMove(const juce::MouseEvent& event,
                                  const juce::MouseWheelDetails& wheel) {
    if (wheel.deltaY == 0.f || !wheelBand) return;
    const auto settings = processor.currentSettings();
    int hovered = -1;
    float closest = 22.f;
    for (int i=0; i<inverso::kBands; ++i) {
        const auto& band = settings.bands[static_cast<std::size_t>(i)];
        if (!band.enabled) continue;
        const float dx = event.position.x - hzToX(band.frequency);
        const float dy = event.position.y - gainToY(band.gainDb);
        const float distance = std::sqrt(dx*dx + dy*dy);
        if (distance < closest) { closest = distance; hovered = i; }
    }
    if (hovered < 0) return;
    if (chooseBand) chooseBand(hovered);
    const int target = event.mods.isAltDown() ? 1 : event.mods.isCtrlDown() ? 2 : 0;
    const double steps = juce::jlimit(-4.0, 4.0,
        static_cast<double>(wheel.deltaY) * (wheel.isReversed ? -5.0 : 5.0));
    wheelBand(hovered, target, steps, event.mods.isShiftDown());
}

InversoEQAudioProcessorEditor::InversoEQAudioProcessorEditor(InversoEQAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), plot(p) {
    setSize(960,650);
    theme.setColour(juce::Slider::thumbColourId,ink);
    theme.setColour(juce::Slider::trackColourId,aqua);
    theme.setColour(juce::Slider::backgroundColourId,grid);
    theme.setColour(juce::Slider::textBoxTextColourId,ink);
    theme.setColour(juce::Slider::textBoxBackgroundColourId,panel);
    theme.setColour(juce::Slider::textBoxOutlineColourId,grid);
    theme.setColour(juce::ComboBox::backgroundColourId,panel);
    theme.setColour(juce::ComboBox::textColourId,ink);
    theme.setColour(juce::ComboBox::outlineColourId,grid);
    theme.setColour(juce::ToggleButton::textColourId,ink);
    setLookAndFeel(&theme);

    addAndMakeVisible(plot);
    plot.chooseBand = [this](int i){chooseBand(i);};
    plot.moveBand = [this](int i,double hz,double db){moveBand(i,hz,db);};
    plot.editGesture = [this](int i,bool begin){gestureBand(i,begin);};
    plot.createBand = [this](double hz,double db){createBand(hz,db);};
    plot.wheelBand = [this](int i,int target,double steps,bool fine){adjustBandWithWheel(i,target,steps,fine);};

    styleLabel(heading,"INVERSO  /  EQ");
    heading.setFont(juce::Font(juce::FontOptions(19.f,juce::Font::bold)));
    styleLabel(statusLabel,"Monitoring");
    statusLabel.setJustificationType(juce::Justification::centredRight);
    styleLabel(hpLabel,"HIGH-PASS"); styleLabel(lpLabel,"LOW-PASS");
    styleLabel(bandLabel,"BAND 4");
    styleLabel(frequencyLabel,"FREQUENCY"); styleLabel(gainLabel,"GAIN"); styleLabel(qLabel,"Q");
    styleLabel(outputLabel,"OUTPUT"); styleLabel(ceilingLabel,"CEILING");
    styleLabel(hpCutoffLabel,"CUTOFF"); styleLabel(lpCutoffLabel,"CUTOFF");
    styleLabel(analysisLabel,"SPECTRUM");

    for (auto* slider : {&hpHz,&lpHz,&bandHz,&bandDb,&bandQ,&outputDb,&ceilingDb})
        styleSlider(*slider);
    for (auto* slider : {&hpHz,&lpHz,&bandHz}) slider->setTextValueSuffix(" Hz");
    bandDb.setTextValueSuffix(" dB");
    outputDb.setTextValueSuffix(" dB");
    ceilingDb.setTextValueSuffix(" dBFS");
    // Custom text formatting affects display only; internal parameters retain full precision.
    for (auto* slider : {&hpHz, &lpHz, &bandHz}) {
        slider->textFromValueFunction = [](double v) {
            return v >= 1000.0 ? juce::String(v/1000.0, 2) + " kHz"
                               : juce::String(v, 1) + " Hz";
        };
        slider->valueFromTextFunction = [](const juce::String& value) {
            auto text = value.trim().toLowerCase();
            const bool kilo = text.containsChar('k');
            const double v = text.getDoubleValue();
            return v * (kilo ? 1000.0 : 1.0);
        };
    }
    for (auto* slider : {&bandDb, &outputDb, &ceilingDb}) {
        slider->textFromValueFunction = [this, slider](double v) {
            return juce::String(v, 1) + (slider == &ceilingDb ? " dBFS" : " dB");
        };
        slider->valueFromTextFunction = [](const juce::String& s) {return s.getDoubleValue();};
    }
    bandQ.textFromValueFunction = [](double v) {return juce::String(v, 2);};
    bandQ.valueFromTextFunction = [](const juce::String& s) {return s.getDoubleValue();};

    hpEnabled.setButtonText("ON"); lpEnabled.setButtonText("ON");
    bandEnabled.setButtonText("ON"); safetyEnabled.setButtonText("PROTECT");
    for (auto* b : {&hpEnabled,&lpEnabled,&bandEnabled,&safetyEnabled}) addAndMakeVisible(*b);
    for (auto* box : {&hpSlope,&lpSlope}) {
        box->addItem("12 dB/oct",1); box->addItem("24 dB/oct",2); box->addItem("48 dB/oct",3);
        addAndMakeVisible(*box);
    }
    spectrumMode.addItem("Off",1); spectrumMode.addItem("Pre",2);
    spectrumMode.addItem("Post",3); spectrumMode.addItem("Pre + Post",4);
    spectrumMode.setSelectedId(3, juce::dontSendNotification);
    spectrumMode.onChange = [this] {plot.setSpectrumMode(spectrumMode.getSelectedId()-1);};
    addAndMakeVisible(spectrumMode);

    hpAttachment = std::make_unique<SliderAttachment>(p.state,"hp_hz",hpHz);
    lpAttachment = std::make_unique<SliderAttachment>(p.state,"lp_hz",lpHz);
    outputAttachment = std::make_unique<SliderAttachment>(p.state,"out_db",outputDb);
    ceilingAttachment = std::make_unique<SliderAttachment>(p.state,"ceiling_db",ceilingDb);
    hpButtonAttachment = std::make_unique<ButtonAttachment>(p.state,"hp_on",hpEnabled);
    lpButtonAttachment = std::make_unique<ButtonAttachment>(p.state,"lp_on",lpEnabled);
    safetyButtonAttachment = std::make_unique<ButtonAttachment>(p.state,"safe_on",safetyEnabled);
    hpSlopeAttachment = std::make_unique<ComboAttachment>(p.state,"hp_slope",hpSlope);
    lpSlopeAttachment = std::make_unique<ComboAttachment>(p.state,"lp_slope",lpSlope);

    for (int i=0; i<inverso::kBands; ++i) {
        auto& button = bandButtons[static_cast<std::size_t>(i)];
        button.setButtonText(juce::String(i+1));
        button.onClick = [this,i] {chooseBand(i);};
        addAndMakeVisible(button);
    }
    chooseBand(3);
    startTimerHz(15);
}

InversoEQAudioProcessorEditor::~InversoEQAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void InversoEQAudioProcessorEditor::styleLabel(juce::Label& l, const juce::String& name) {
    l.setText(name,juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, secondary);
    l.setFont(juce::Font(juce::FontOptions(11.f,juce::Font::bold)));
    addAndMakeVisible(l);
}
void InversoEQAudioProcessorEditor::styleSlider(juce::Slider& s, const juce::String& suffix) {
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,20);
    s.setTextValueSuffix(suffix);
    addAndMakeVisible(s);
}

void InversoEQAudioProcessorEditor::chooseBand(int index) {
    index = juce::jlimit(0,inverso::kBands-1,index);
    if (selectedBand == index && freqAttachment && dbAttachment && qAttachment) return;
    selectedBand = index;
    freqAttachment.reset(); dbAttachment.reset(); qAttachment.reset(); bandButtonAttachment.reset();
    const auto prefix = "b" + juce::String(selectedBand+1);
    freqAttachment = std::make_unique<SliderAttachment>(processor.state,prefix+"_hz",bandHz);
    dbAttachment = std::make_unique<SliderAttachment>(processor.state,prefix+"_db",bandDb);
    qAttachment = std::make_unique<SliderAttachment>(processor.state,prefix+"_q",bandQ);
    bandButtonAttachment = std::make_unique<ButtonAttachment>(processor.state,prefix+"_on",bandEnabled);
    bandLabel.setText("BAND " + juce::String(selectedBand+1),juce::dontSendNotification);
    for (int i=0; i<inverso::kBands; ++i) {
        auto& button = bandButtons[static_cast<std::size_t>(i)];
        button.setColour(juce::TextButton::buttonColourId,i==selectedBand ? aqua.darker(0.7f) : panel);
        button.setColour(juce::TextButton::textColourOffId,ink);
    }
}

void InversoEQAudioProcessorEditor::setParameter(const juce::String& key, float value) {
    if (auto* p = processor.state.getParameter(key))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}
void InversoEQAudioProcessorEditor::moveBand(int index, double hz, double db) {
    const auto prefix="b"+juce::String(index+1);
    setParameter(prefix+"_hz",static_cast<float>(hz));
    setParameter(prefix+"_db",static_cast<float>(db));
}
void InversoEQAudioProcessorEditor::gestureBand(int index, bool beginning) {
    const auto prefix="b"+juce::String(index+1);
    for (const auto& suffix : {"_hz","_db"})
        if (auto* p=processor.state.getParameter(prefix+suffix)) {
            if (beginning) p->beginChangeGesture();
            else p->endChangeGesture();
        }
}
void InversoEQAudioProcessorEditor::createBand(double hz, double db) {
    const auto settings=processor.currentSettings();
    int newBand=-1;
    for (int i=0; i<inverso::kBands; ++i)
        if (!settings.bands[static_cast<std::size_t>(i)].enabled) {newBand=i;break;}
    if (newBand<0) return;
    const auto prefix="b"+juce::String(newBand+1);
    setParameter(prefix+"_hz",static_cast<float>(hz));
    setParameter(prefix+"_db",static_cast<float>(db));
    setParameter(prefix+"_on",1.f);
    chooseBand(newBand);
}

void InversoEQAudioProcessorEditor::adjustBandWithWheel(int index, int target,
                                                          double steps, bool fine) {
    const auto prefix = "b" + juce::String(index+1);
    const auto id = prefix + (target==1 ? "_q" : target==2 ? "_hz" : "_db");
    if (auto* parameter = processor.state.getParameter(id)) {
        const auto& b = processor.currentSettings().bands[static_cast<std::size_t>(index)];
        const double value = target==1 ? b.q : target==2 ? b.frequency : b.gainDb;
        const double next = target==2 ? value*std::pow(1.01, steps*(fine ? 0.2 : 1.0))
                                   : value+steps*(target==1 ? (fine ? 0.01 : 0.05) : (fine ? 0.02 : 0.1));
        const auto& range = parameter->getNormalisableRange();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(
            static_cast<float>(juce::jlimit(static_cast<double>(range.start),
                                            static_cast<double>(range.end),next))));
        parameter->endChangeGesture();
    }
}

void InversoEQAudioProcessorEditor::timerCallback() {
    const auto pre=processor.lastPreProtectionPeak.load(std::memory_order_relaxed);
    const auto in=processor.lastInputPeak.load(std::memory_order_relaxed);
    const auto out=processor.lastOutputPeak.load(std::memory_order_relaxed);
    const auto red=processor.lastReductionDb.load(std::memory_order_relaxed);
    holdPre=std::max(pre,holdPre*0.80f);
    holdOut=std::max(out,holdOut*0.80f);
    holdReduction=std::max(red,holdReduction*0.75f);
    const auto s=processor.currentSettings();
    const double warnLevel = inverso::dbToLinear(s.protectionCeilingDb - 2.0);
    const juce::String status = (holdPre >= warnLevel) ? "NEAR LIMIT" : "LEVEL OK";
    statusLabel.setColour(juce::Label::textColourId,
                          holdPre >= warnLevel ? amber : aqua);
    statusLabel.setText("IN " + juce::String(levelDb(in),1) + "   PRE " + juce::String(levelDb(holdPre),1)
        + "   OUT " + juce::String(levelDb(holdOut),1) + " dBFS"
        + "    |    " + status + (s.protectionEnabled ? "    /    GR " + juce::String(holdReduction,1) + " dB" : "  /  PROTECT OFF"),
        juce::dontSendNotification);
}

void InversoEQAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(grid);
    g.drawHorizontalLine(50,20.f,940.f);
    g.drawRoundedRectangle(juce::Rectangle<float>(18.f,412.f,216.f,150.f),8.f,1.f);
    g.drawRoundedRectangle(juce::Rectangle<float>(245.f,412.f,470.f,150.f),8.f,1.f);
    g.drawRoundedRectangle(juce::Rectangle<float>(726.f,412.f,216.f,150.f),8.f,1.f);
    g.drawHorizontalLine(575,20.f,940.f);
    g.setColour(secondary.withAlpha(0.7f));
    g.setFont(11.f);
    g.drawText("SAMPLE PEAK ONLY / not true peak",20,630,365,16,juce::Justification::left);
    g.drawText("WHEEL: fine    SHIFT: extra fine    POINT + ALT: Q    POINT + CTRL: Hz",
               395,630,545,16,juce::Justification::right);
}

void InversoEQAudioProcessorEditor::resized() {
    heading.setBounds(20,12,230,30);
    statusLabel.setBounds(260,10,682,32);
    plot.setBounds(18,60,924,338);
    hpLabel.setBounds(32,424,110,18);
    hpEnabled.setBounds(168,422,60,22);
    hpCutoffLabel.setBounds(32,454,110,18);
    hpHz.setBounds(26,478,199,29);
    hpSlope.setBounds(32,520,186,26);

    bandLabel.setBounds(259,421,110,20);
    bandEnabled.setBounds(650,419,60,23);
    for (int i=0; i<inverso::kBands; ++i)
        bandButtons[static_cast<std::size_t>(i)].setBounds(336+i*37,419,31,24);
    frequencyLabel.setBounds(258,459,110,15);
    bandHz.setBounds(354,452,350,29);
    gainLabel.setBounds(258,496,110,15);
    bandDb.setBounds(354,488,350,29);
    qLabel.setBounds(258,533,100,15);
    bandQ.setBounds(354,525,350,29);

    lpLabel.setBounds(740,424,100,18);
    lpEnabled.setBounds(876,422,59,22);
    lpCutoffLabel.setBounds(740,454,100,18);
    lpHz.setBounds(733,478,199,29);
    lpSlope.setBounds(739,520,185,26);

    outputLabel.setBounds(22,585,100,18);
    outputDb.setBounds(102,582,192,31);
    safetyEnabled.setBounds(317,584,105,24);
    ceilingLabel.setBounds(433,585,65,18);
    ceilingDb.setBounds(491,582,181,31);
    analysisLabel.setBounds(691,585,90,18);
    spectrumMode.setBounds(781,584,153,26);
}
