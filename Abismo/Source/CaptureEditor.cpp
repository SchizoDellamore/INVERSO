#include "CaptureEditor.h"
#include "AbismoUI.h"
#include <cmath>

using namespace inverso_abismo::capture;
namespace c = soft::colour;

namespace {
const juce::Colour accent{0xff3ccfe0}, hot{0xffef5350};
const char* const* shortNames() {
 static const char* names[] = {
  "START", "END", "GAIN", "PITCH", "REVERSE", "LOOP", "FADE IN", "FADE OUT", "TRACK", "CLIP", "LENGTH",
  "STRETCH", "MODE", "TAPE", "TAPE STOP", "DIRECTION",
  "FILTER", "TYPE", "FROM", "TO", "RESONANCE",
  "DRIVE", "TYPE", "AMOUNT", "MIX",
  "CRUSH", "BITS", "RATE", "MIX",
  "RING", "FREQUENCY", "MIX",
  "CHOP", "MODE", "SLICE", "DEPTH",
  "ECHO", "TIME", "FEEDBACK", "DARK", "MIX",
  "REVERB", "SIZE", "DARK", "MIX",
  "ORIGINAL"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}
juce::String timeText(double seconds) { return juce::String(seconds, seconds < 10.0 ? 2 : 1) + " s"; }
} // namespace

// ---------------------------------------------------------------- waveform
CaptureWave::CaptureWave(InversoCaptureProcessor& p, bool resultView) : processor(p), result(resultView) { if (!result) setMouseCursor(juce::MouseCursor::PointingHandCursor); }

float CaptureWave::percentOf(int p) const { return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(p)].id)->load(); }
void CaptureWave::setParam(int p, float percent) {
 if (auto* param = processor.apvts.getParameter(specs()[static_cast<std::size_t>(p)].id)) param->setValueNotifyingHost(param->convertTo0to1(juce::jlimit(0.f, 100.f, percent)));
}

void CaptureWave::refresh() {
 const unsigned key = (result ? processor.renderedVersion() : processor.clipVersion()) * 4096u + static_cast<unsigned>(getWidth());
 if (key != peaksFor) {
  peaksFor = key;
  peaks.clear();
  const auto clip = result ? processor.rendered() : processor.clip();
  if (clip && !clip->empty() && getWidth() > 4) {
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
 auto a = r.reduced(4.f, result ? 6.f : 10.f);
 g.setColour(c::screenMuted.withAlpha(.25f));
 g.drawHorizontalLine(juce::roundToInt(a.getCentreY()), a.getX(), a.getRight());
 g.setFont(soft::monoFont(11.f));
 if (!result && processor.isRecording()) {
  g.setColour(hot);
  g.drawText("RECORDING  -  press STOP REC to keep what passed", a, juce::Justification::centredTop);
  const float lvl = juce::jlimit(0.f, 1.f, processor.inputLevel());
  g.fillRoundedRectangle(a.getX(), a.getCentreY() - lvl * a.getHeight() * .4f, a.getWidth() * (static_cast<float>(std::fmod(juce::Time::getMillisecondCounter() / 4000.0, 1.0))), lvl * a.getHeight() * .8f, 3.f);
  return;
 }
 if (peaks.empty()) {
  g.setColour(c::screenMuted);
  if (!result) {
   g.drawText("PLAY THE TRACK, THEN CAPTURE   -   or REC and STOP REC   -   or OPEN / drop an audio file here", a, juce::Justification::centred);
   g.setFont(soft::monoFont(10.f));
   g.drawText("the last " + timeText(processor.ringSeconds()) + " of the track are kept while the DAW plays", a.withTrimmedTop(a.getHeight() * .62f), juce::Justification::centredTop);
  } else g.drawText(processor.clip() ? "..." : "", a, juce::Justification::centred);
  return;
 }
 const float x0 = a.getX(), w = static_cast<float>(peaks.size());
 float lo = 0.f, hi = 1.f;
 if (!result) { const float sx = percentOf(P::Start), ex = percentOf(P::End); lo = std::min(sx, ex) * .01f; hi = std::max(sx, ex) * .01f; }
 for (std::size_t i = 0; i < peaks.size(); ++i) {
  const float t = static_cast<float>(i) / w;
  const bool inside = t >= lo && t <= hi;
  g.setColour((inside ? (result ? c::orange : accent) : c::screenMuted).withAlpha(inside ? .95f : .4f));
  const float h = a.getHeight() * .5f;
  g.drawVerticalLine(juce::roundToInt(x0 + static_cast<float>(i) + 4.f), a.getCentreY() - peaks[i].second * h, a.getCentreY() - peaks[i].first * h + 1.f);
 }
 if (!result) for (float p : {lo, hi}) {
  const float x = x0 + 4.f + p * w;
  g.setColour(juce::Colours::white.withAlpha(.9f));
  g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
  g.fillRoundedRectangle(x - 5.f, a.getY(), 10.f, 14.f, 3.f);
 }
 // the playhead: on the result while the edit is heard, on the source while the original is
 const bool original = percentOf(P::Original) > .5f;
 if (processor.isPlaying() && original != result) {
  const float pos = static_cast<float>(processor.playPosition());
  const float x = x0 + 4.f + pos * w;
  g.setColour(result ? c::orange : juce::Colours::white);
  g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
 }
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 if (const auto clip = result ? processor.rendered() : processor.clip()) {
  const double total = static_cast<double>(clip->frames()) / clip->sampleRate;
  g.drawText(result ? "RESULT " + timeText(total) + (processor.isRendering() ? "   ... RENDERING" : juce::String()) : "CLIP " + timeText(total) + "    REGION " + timeText(total * (hi - lo)),
             a.withTrimmedTop(a.getHeight() - 14.f), juce::Justification::bottomLeft);
 }
}

void CaptureWave::mouseDown(const juce::MouseEvent& e) {
 dragging = -1;
 if (result || peaks.empty()) return;
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
InversoCaptureEditor::InversoCaptureEditor(InversoCaptureProcessor& p) : AudioProcessorEditor(&p), processor(p), source(p, false), result(p, true), drag(p) {
 setLookAndFeel(&look);
 body.title = "CAPTURE";
 body.subtitle = juce::String::fromUTF8("TAKE A PIECE OF A TRACK   \xc2\xb7   STRETCH, BEND, TWIST IT   \xc2\xb7   DRAG IT TO ANOTHER TRACK");
 addAndMakeVisible(body);
 body.addAndMakeVisible(source);
 body.addAndMakeVisible(result);
 body.addAndMakeVisible(drag);
 for (auto* b : {&openButton, &captureButton, &recButton, &playButton, &originalButton, &undoButton, &bakeButton, &normalizeButton, &saveButton}) body.addAndMakeVisible(*b);
 for (auto* l : {&sourceTitle, &resultTitle}) { l->setFont(soft::monoFont(10.f)); l->setColour(juce::Label::textColourId, c::muted); body.addAndMakeVisible(*l); }
 sourceTitle.setText("THE CLIP  -  drag the two ends to choose the part", juce::dontSendNotification);
 resultTitle.setText("WHAT YOU GET  -  this is what PLAY and the file hold", juce::dontSendNotification);
 openButton.setTooltip("Open an audio file (WAV, AIFF, FLAC, MP3) - or drop one on the plugin");
 captureButton.setTooltip("Keep the last seconds of the track (set under MIX). The DAW has to have been playing it");
 recButton.setTooltip("Press, play the part you want, press again: that is the clip");
 playButton.setTooltip("Play the edited clip on top of the track");
 originalButton.setTooltip("Hear the part you chose, as it was, to compare with the edit");
 originalButton.setClickingTogglesState(true);
 originalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, specs()[P::Original].id, originalButton);
 bakeButton.setTooltip("Make the clip what you hear now, and keep editing from there");
 undoButton.setTooltip("Back to the clip and settings before the last BAKE or NORMALIZE");
 normalizeButton.setTooltip("Raise the clip so its highest peak is at -1 dB");
 saveButton.setTooltip("Save the edited clip as a WAV file anywhere");
 captureButton.onClick = [this] { if (!processor.capture()) say("Nothing to capture yet: play the track in the DAW first", true); else say({}); source.refresh(); };
 recButton.onClick = [this] {
  if (processor.isRecording()) { if (!processor.stopRecording()) say("Nothing was recorded: the DAW has to play the track", true); else say({}); }
  else processor.startRecording();
  source.refresh();
 };
 openButton.onClick = [this] {
  chooser = std::make_unique<juce::FileChooser>("Open an audio file", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
  chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
   const auto f = fc.getResult();
   if (f == juce::File()) return;
   juce::String message; const bool ok = processor.loadFile(f, message);
   say(ok ? (message.isNotEmpty() ? message : "Opened " + f.getFileName()) : message, !ok);
  });
 };
 playButton.onClick = [this] { if (processor.isPlaying()) processor.stopPlaying(); else processor.play(); };
 bakeButton.onClick = [this] { processor.bake(); source.refresh(); };
 undoButton.onClick = [this] { processor.undo(); source.refresh(); };
 normalizeButton.onClick = [this] { processor.normalizeClip(); source.refresh(); };
 saveButton.onClick = [this] {
  if (processor.clip() == nullptr) { say("Nothing to save yet", true); return; }
  chooser = std::make_unique<juce::FileChooser>("Save the clip as", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("INVERSO capture.wav"), "*.wav");
  chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& fc) {
   const auto f = fc.getResult();
   if (f == juce::File()) return;
   const auto target = f.hasFileExtension("wav") ? f : f.withFileExtension(f.getFileExtension() + ".wav");
   if (processor.exportWav(target)) say("Saved " + target.getFileName()); else say("Could not save the file", true);
  });
 };
 status.setFont(soft::monoFont(11.f));
 status.setColour(juce::Label::textColourId, c::muted);
 body.addAndMakeVisible(status);

 std::vector<const char*> names;
 for (int i = 0; i < P::Count; ++i) names.push_back(shortNames()[i]);
 using Page = soft::Module::Page;
 auto card = [&](const char* title, juce::Colour col, std::vector<Page> pages) {
  modules.push_back(std::make_unique<soft::Module>(processor.apvts, specs().data(), names.data(), title, col, std::move(pages), 0, std::vector<int>{}));
  body.addAndMakeVisible(*modules.back());
 };
 card("TIME & PITCH", accent, {{"Time", {P::Stretch, P::Pitch, P::Mode, P::Tape}}, {"Shape", {P::Reverse, P::Warp, P::WarpDir, P::Gain, P::FadeIn, P::FadeOut}}});
 card("TONE", c::violet, {{"Filter", {P::FilterOn, P::FilterType, P::FilterFrom, P::FilterTo, P::FilterReso}}, {"Drive", {P::DriveOn, P::DriveType, P::DriveAmount, P::DriveMix}},
                          {"Crush", {P::CrushOn, P::CrushBits, P::CrushRate, P::CrushMix}}, {"Ring", {P::RingOn, P::RingFreq, P::RingMix}}});
 card("SPACE & RHYTHM", c::pink, {{"Echo", {P::EchoOn, P::EchoTime, P::EchoFeedback, P::EchoDark, P::EchoMix}}, {"Reverb", {P::ReverbOn, P::ReverbSize, P::ReverbDamp, P::ReverbMix}},
                                  {"Chop", {P::ChopOn, P::ChopMode, P::ChopSlice, P::ChopDepth}}});
 card("MIX", c::orange, {{"Mix", {P::Dry, P::Sample, P::Loop, P::CapLen}}});
 setResizable(true, true);
 soft::sizeWindow(*this, "CAPTURE", designW, designH, 0.6f);
 startTimerHz(30);
}

InversoCaptureEditor::~InversoCaptureEditor() { setLookAndFeel(nullptr); }

bool InversoCaptureEditor::isInterestedInFileDrag(const juce::StringArray& files) {
 for (const auto& f : files) { const auto ext = juce::File(f).getFileExtension().toLowerCase(); if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg") return true; }
 return false;
}
void InversoCaptureEditor::filesDropped(const juce::StringArray& files, int, int) {
 if (files.isEmpty()) return;
 juce::String message; const bool ok = processor.loadFile(juce::File(files[0]), message);
 say(ok ? (message.isNotEmpty() ? message : "Opened " + juce::File(files[0]).getFileName()) : message, !ok);
}
void InversoCaptureEditor::say(const juce::String& text, bool problem) {
 status.setColour(juce::Label::textColourId, problem ? hot : c::muted);
 status.setText(text, juce::dontSendNotification);
}

void InversoCaptureEditor::timerCallback() {
 processor.housekeeping();
 source.refresh();
 result.refresh();
 const bool has = processor.clip() != nullptr;
 recButton.setButtonText(processor.isRecording() ? "STOP REC" : "REC");
 recButton.setColour(juce::TextButton::buttonColourId, processor.isRecording() ? hot : c::card);
 playButton.setButtonText(processor.isPlaying() ? "STOP" : "PLAY");
 originalButton.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(.5f));
 for (auto* b : {&playButton, &originalButton, &bakeButton, &normalizeButton, &saveButton}) b->setEnabled(has && !processor.isRecording());
 undoButton.setEnabled(processor.canUndo());
 captureButton.setEnabled(!processor.isRecording());
 openButton.setEnabled(!processor.isRecording());
 drag.repaint();
}

void InversoCaptureEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 soft::rememberWindow(*this, "CAPTURE", designW);
 const int x0 = 14, w = designW - 28;
 sourceTitle.setBounds(x0, 88, w, 14);
 source.setBounds(x0, 104, w, 150);
 resultTitle.setBounds(x0, 260, w, 14);
 result.setBounds(x0, 276, w, 92);
 int x = x0;
 const int y = 380, h = 34;
 auto put = [&](juce::Component& b, int width) { b.setBounds(x, y, width, h); x += width + 6; };
 put(openButton, 74); put(captureButton, 92); put(recButton, 84);
 x += 10;
 put(playButton, 84); put(originalButton, 124);
 x += 10;
 put(undoButton, 76); put(bakeButton, 76); put(normalizeButton, 106);
 x += 10;
 put(saveButton, 92);
 drag.setBounds(designW - 14 - 190, y, 190, h);
 status.setBounds(x0, y + h + 4, w, 18);
 const int top = 438, hgt = designH - top - 10;
 const int widths[4] = {350, 330, 320, 170};
 int cx = x0 - 4;
 for (std::size_t i = 0; i < modules.size(); ++i) { modules[i]->setBounds(cx, top, widths[i] + 4, hgt); cx += widths[i] + 4; }
}
