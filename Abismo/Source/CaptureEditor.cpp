#include "CaptureEditor.h"
#include "AbismoUI.h"
#include <cmath>

using namespace inverso_abismo::capture;
namespace c = soft::colour;

namespace {
const juce::Colour accent{0xff3ccfe0}, hot{0xffef5350};
const char* const* shortNames() {
 static const char* names[] = {"START", "END", "GAIN", "PITCH", "REVERSE", "LOOP", "FADE IN", "FADE OUT", "TRACK THROUGH", "CLIP LEVEL", "CAPTURE THE LAST"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
juce::String timeText(double seconds) { return juce::String(seconds, seconds < 10.0 ? 2 : 1) + " s"; }
} // namespace

// ---------------------------------------------------------------- waveform
CaptureWave::CaptureWave(InversoCaptureProcessor& p) : processor(p) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

float CaptureWave::percentOf(int p) const { return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(p)].id)->load(); }
void CaptureWave::setParam(int p, float percent) {
 if (auto* param = processor.apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) param->setValueNotifyingHost(param->convertTo0to1(juce::jlimit(0.f, 100.f, percent)));
}

void CaptureWave::refresh() {
 const unsigned v = processor.clipVersion();
 const unsigned key = v * 4096u + static_cast<unsigned>(getWidth());
 if (key != peaksFor) {
  peaksFor = key; shownVersion = v;
  peaks.clear();
  if (const auto clip = processor.clip(); clip && !clip->empty() && getWidth() > 4) {
   const int w = getWidth() - 8;
   peaks.resize(static_cast<std::size_t>(w));
   const double per = static_cast<double>(clip->frames()) / w;
   for (int x = 0; x < w; ++x) {
    const auto a = static_cast<std::size_t>(x * per), b = std::min(clip->frames(), static_cast<std::size_t>((x + 1) * per) + 1);
    float lo = 0.f, hi = 0.f;
    const std::size_t step = std::max<std::size_t>(1, (b - a) / 64);
    for (std::size_t i = a; i < b; i += step) { const float s = .5f * (clip->l[i] + clip->r[i]); lo = std::min(lo, s); hi = std::max(hi, s); }
    peaks[static_cast<std::size_t>(x)] = {lo, hi};
   }
  }
 }
 repaint();
}

void CaptureWave::paint(juce::Graphics& g) {
 auto r = getLocalBounds().toFloat();
 g.setGradientFill(juce::ColourGradient(c::screenTop, 0.f, r.getY(), c::screenBottom, 0.f, r.getBottom(), false));
 g.fillRoundedRectangle(r, 10.f);
 auto a = r.reduced(4.f, 8.f);
 g.setColour(c::screenMuted.withAlpha(.25f));
 g.drawHorizontalLine(juce::roundToInt(a.getCentreY()), a.getX(), a.getRight());
 g.setFont(soft::monoFont(11.f));
 if (processor.isRecording()) {
  g.setColour(hot);
  g.drawText("RECORDING  -  press REC again to keep what passed", a, juce::Justification::centredTop);
  const float lvl = juce::jlimit(0.f, 1.f, processor.inputLevel());
  g.fillRoundedRectangle(a.getX(), a.getCentreY() - lvl * a.getHeight() * .4f, a.getWidth() * (static_cast<float>(std::fmod(juce::Time::getMillisecondCounter() / 4000.0, 1.0))), lvl * a.getHeight() * .8f, 3.f);
  return;
 }
 if (peaks.empty()) {
  g.setColour(c::screenMuted);
  g.drawText("PLAY THE TRACK, THEN PRESS CAPTURE  -  or REC / REC to take exactly what you play", a, juce::Justification::centred);
  g.setFont(soft::monoFont(10.f));
  g.drawText("the last " + timeText(processor.ringSeconds()) + " of the track are kept while the DAW plays", a.withTrimmedTop(a.getHeight() * .6f), juce::Justification::centredTop);
  return;
 }
 const float x0 = a.getX() + 0.f, w = static_cast<float>(peaks.size());
 const float sx = percentOf(P::Start), ex = percentOf(P::End);
 const float lo = std::min(sx, ex) * .01f, hi = std::max(sx, ex) * .01f;
 for (std::size_t i = 0; i < peaks.size(); ++i) {
  const float t = static_cast<float>(i) / w;
  const bool inside = t >= lo && t <= hi;
  g.setColour((inside ? accent : c::screenMuted).withAlpha(inside ? .95f : .4f));
  const float h = a.getHeight() * .5f;
  g.drawVerticalLine(juce::roundToInt(x0 + static_cast<float>(i) + 4.f), a.getCentreY() - peaks[i].second * h, a.getCentreY() - peaks[i].first * h + 1.f);
 }
 for (float p : {lo, hi}) {
  const float x = x0 + 4.f + p * w;
  g.setColour(juce::Colours::white.withAlpha(.9f));
  g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
  g.fillRoundedRectangle(x - 5.f, a.getY(), 10.f, 14.f, 3.f);
 }
 if (processor.isPlaying()) {
  const float x = x0 + 4.f + static_cast<float>(processor.playPosition()) * w;
  g.setColour(c::orange);
  g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
 }
 if (const auto clip = processor.clip()) {
  const double total = static_cast<double>(clip->frames()) / clip->sampleRate;
  g.setColour(c::screenMuted);
  g.setFont(soft::monoFont(10.f));
  g.drawText("CLIP " + timeText(total) + "    REGION " + timeText(total * (hi - lo)), a.withTrimmedTop(a.getHeight() - 14.f), juce::Justification::bottomLeft);
 }
}

void CaptureWave::mouseDown(const juce::MouseEvent& e) {
 dragging = -1;
 if (peaks.empty()) return;
 const float w = static_cast<float>(peaks.size());
 const float xs = 4.f + percentOf(P::Start) * .01f * w, xe = 4.f + percentOf(P::End) * .01f * w;
 const float ds = std::abs(e.position.x - xs), de = std::abs(e.position.x - xe);
 if (std::min(ds, de) < 14.f) dragging = ds <= de ? P::Start : P::End;
 else dragging = e.position.x < (xs + xe) * .5f ? P::Start : P::End;   // a click elsewhere moves the nearer end there
 if (auto* param = processor.apvts.getParameter(specs()[static_cast<std::size_t>(dragging)].id)) param->beginChangeGesture();
 mouseDrag(e);
}
void CaptureWave::mouseDrag(const juce::MouseEvent& e) {
 if (dragging < 0 || peaks.empty()) return;
 setParam(dragging, (e.position.x - 4.f) / static_cast<float>(peaks.size()) * 100.f);
}
void CaptureWave::mouseUp(const juce::MouseEvent&) {
 if (dragging >= 0) if (auto* param = processor.apvts.getParameter(specs()[static_cast<std::size_t>(dragging)].id)) param->endChangeGesture();
 dragging = -1;
}

// ---------------------------------------------------------------- drag zone
void CaptureDragZone::paint(juce::Graphics& g) {
 auto r = getLocalBounds().toFloat();
 const bool ready = processor.clip() != nullptr;
 g.setColour(ready ? accent.withAlpha(.16f) : c::track);
 g.fillRoundedRectangle(r, 10.f);
 g.setColour(ready ? accent : c::muted);
 g.drawRoundedRectangle(r.reduced(1.f), 10.f, 1.5f);
 g.setFont(soft::monoFont(12.f));
 g.drawText(message.isNotEmpty() ? message : (ready ? juce::String("DRAG TO A TRACK") : juce::String("DRAG (capture first)")), r, juce::Justification::centred);
}
void CaptureDragZone::mouseUp(const juce::MouseEvent&) { dragged = false; }
void CaptureDragZone::mouseDrag(const juce::MouseEvent& e) {
 if (dragged || e.getDistanceFromDragStart() < 6) return;
 dragged = true;
 const auto file = processor.makeDragFile();
 if (file == juce::File()) { message = "NOTHING TO DRAG YET"; dragged = false; repaint(); return; }
 message = "DROP IT ON A TRACK";
 repaint();
 juce::DragAndDropContainer::performExternalDragDropOfFiles({file.getFullPathName()}, false, this, [this] { dragged = false; message = {}; repaint(); });
}

// ---------------------------------------------------------------- editor
InversoCaptureEditor::InversoCaptureEditor(InversoCaptureProcessor& p) : AudioProcessorEditor(&p), processor(p), wave(p), drag(p) {
 setLookAndFeel(&look);
 body.title = "CAPTURE";
 body.subtitle = juce::String::fromUTF8("TAKE A PIECE OF THIS TRACK   \xc2\xb7   EDIT IT HERE   \xc2\xb7   DRAG IT TO ANOTHER TRACK");
 addAndMakeVisible(body);
 body.addAndMakeVisible(wave);
 body.addAndMakeVisible(drag);
 for (auto* b : {&captureButton, &recButton, &playButton, &undoButton, &bakeButton, &normalizeButton, &saveButton}) body.addAndMakeVisible(*b);
 captureButton.setTooltip("Keep the last seconds of the track (set below). The DAW has to have been playing it");
 recButton.setTooltip("Press, play the part you want, press again: that is the clip");
 playButton.setTooltip("Play the clip as edited, on top of the track");
 bakeButton.setTooltip("Make the clip what you hear now (region, pitch, reverse, fades, gain) and start editing from there");
 undoButton.setTooltip("Back to the clip and settings before the last BAKE or NORMALIZE");
 normalizeButton.setTooltip("Raise the clip so its highest peak is at -1 dB");
 saveButton.setTooltip("Save the edited clip as a WAV file anywhere");
 captureButton.onClick = [this] { if (!processor.capture()) status.setText("Nothing to capture yet: play the track in the DAW first", juce::dontSendNotification); else status.setText({}, juce::dontSendNotification); wave.refresh(); };
 recButton.onClick = [this] {
  if (processor.isRecording()) { if (!processor.stopRecording()) status.setText("Nothing was recorded: the DAW has to play the track", juce::dontSendNotification); else status.setText({}, juce::dontSendNotification); }
  else processor.startRecording();
  wave.refresh();
 };
 playButton.onClick = [this] { if (processor.isPlaying()) processor.stopPlaying(); else processor.play(); };
 bakeButton.onClick = [this] { processor.bake(); wave.refresh(); };
 undoButton.onClick = [this] { processor.undo(); wave.refresh(); };
 normalizeButton.onClick = [this] { processor.normalizeClip(); wave.refresh(); };
 saveButton.onClick = [this] {
  if (processor.clip() == nullptr) { status.setText("Nothing to save yet", juce::dontSendNotification); return; }
  chooser = std::make_unique<juce::FileChooser>("Save the clip as", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("INVERSO capture.wav"), "*.wav");
  chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
   const auto f = fc.getResult();
   if (f != juce::File()) status.setText(processor.exportWav(f.hasFileExtension("wav") ? f : f.withFileExtension(f.getFileExtension() + ".wav")) ? "Saved " + f.getFileName() : juce::String("Could not save the file"), juce::dontSendNotification);
  });
 };
 status.setFont(soft::monoFont(11.f));
 status.setColour(juce::Label::textColourId, c::muted);
 body.addAndMakeVisible(status);

 std::vector<const char*> names;
 for (int i = 0; i < P::Count; ++i) names.push_back(shortNames()[i]);
 using Page = soft::Module::Page;
 edit = std::make_unique<soft::Module>(processor.apvts, specs().data(), names.data(), "EDIT", accent,
                                       std::vector<Page>{{"Edit", {P::Gain, P::Pitch, P::FadeIn, P::FadeOut, P::Reverse, P::Loop}}}, 0, std::vector<int>{});
 mix = std::make_unique<soft::Module>(processor.apvts, specs().data(), names.data(), "MIX", c::orange,
                                      std::vector<Page>{{"Mix", {P::Dry, P::Sample, P::CapLen}}}, 0, std::vector<int>{});
 body.addAndMakeVisible(*edit);
 body.addAndMakeVisible(*mix);
 setResizable(true, true);
 soft::sizeWindow(*this, "CAPTURE", designW, designH, 0.6f);
 startTimerHz(30);
}

InversoCaptureEditor::~InversoCaptureEditor() { setLookAndFeel(nullptr); }

void InversoCaptureEditor::timerCallback() {
 processor.housekeeping();
 wave.refresh();
 const bool has = processor.clip() != nullptr;
 recButton.setButtonText(processor.isRecording() ? "STOP REC" : "REC");
 recButton.setColour(juce::TextButton::buttonColourId, processor.isRecording() ? hot : c::card);
 playButton.setButtonText(processor.isPlaying() ? "STOP" : "PLAY");
 for (auto* b : {&playButton, &bakeButton, &normalizeButton, &saveButton}) b->setEnabled(has && !processor.isRecording());
 undoButton.setEnabled(processor.canUndo());
 captureButton.setEnabled(!processor.isRecording());
 drag.repaint();
}

void InversoCaptureEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this, "CAPTURE", designW);
 const int x0 = 14, w = designW - 28;
 wave.setBounds(x0, 92, w, 220);
 int x = x0;
 const int y = 322, h = 34;
 for (auto* b : {&captureButton, &recButton, &playButton}) { b->setBounds(x, y, 100, h); x += 106; }
 x += 8;
 for (auto* b : {&undoButton, &bakeButton, &normalizeButton, &saveButton}) { b->setBounds(x, y, 104, h); x += 110; }
 drag.setBounds(designW - 14 - 190, y, 190, h);
 status.setBounds(x0, y + h + 4, w, 18);
 edit->setBounds(x0 - 4, 388, 640, designH - 388 - 10);
 mix->setBounds(x0 - 4 + 640, 388, w - 640 + 8, designH - 388 - 10);
}
