#include "EnkiEditor.h"
#include "EnkiLibrary.h"
#include "DSP/EnkiEngine.h"
#include <cmath>

using namespace inverso_abismo::enki;
namespace c = soft::colour;

namespace {
constexpr int designW = 1320, designH = 1000;

const char* const* shortNames() {
 static const char* names[] = {
  "TYPE","LEVEL","PITCH","FINE","CHARACTER","MOTION","SAMPLE",
  "TYPE","LEVEL","PITCH","FINE","CHARACTER","MOTION","SAMPLE",
  "RING",
  "TYPE","CUTOFF","RESONANCE","ENVELOPE","KEYTRACK","DRIVE","DRIVE TYPE","CRUSH",
  "ATTACK","DECAY","SUSTAIN","RELEASE","MOD ATTACK","MOD DECAY","PITCH ENV","ONE-SHOT",
  "RATE","SHAPE","TIMING","PITCH","FILTER","LEVEL","PAN","CHARACTER",
  "GATE","STEPS","PATTERN","GLITCH","REVERSE",
  "ECHO","SYNC","TIME","REPEATS","REVERB","SIZE","DECAY",
  "OCTAVE","GLIDE","VELOCITY","BEND","AT KNOB","WIDTH","OUTPUT",
  "CATEGORY","MUTATE","LOCK","LOCK","LOCK","LOCK","LOCK"};
 static_assert(sizeof(names) / sizeof(names[0]) == P::Count, "one short name per parameter");
 return names;
}

juce::Path curve(juce::Rectangle<float> r, int points, const std::function<float(float)>& f) {
 juce::Path p;
 for (int i = 0; i <= points; ++i) {
  const float t = static_cast<float>(i) / static_cast<float>(points);
  const float y = juce::jlimit(-1.2f, 1.2f, f(t));
  const juce::Point<float> pt(r.getX() + t * r.getWidth(), r.getCentreY() - y * r.getHeight() * .5f);
  if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
 }
 return p;
}
void caption(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& left, const juce::String& right = {}) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 g.drawText(left, r.withHeight(12.f), juce::Justification::topLeft);
 if (right.isNotEmpty()) g.drawText(right, r.withHeight(12.f), juce::Justification::topRight);
}
void notice(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text) {
 g.setFont(soft::uiFont(11.f, true));
 const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 24.f;
 g.setColour(c::screenBottom.withAlpha(.9f));
 g.fillRoundedRectangle(juce::Rectangle<float>(w, 22.f).withCentre(r.getCentre()), 11.f);
 g.setColour(c::screenMuted);
 g.drawText(text, r, juce::Justification::centred);
}
juce::Path adsr(juce::Rectangle<float> r, float a, float d, float s, float rel) {
 auto w = [](float ms) { return std::log10(1.f + ms / 4.f); };
 const float total = w(a) + w(d) + 1.2f + w(rel);
 const float k = r.getWidth() / total;
 const float x1 = r.getX() + w(a) * k, x2 = x1 + w(d) * k, x3 = x2 + 1.2f * k, x4 = x3 + w(rel) * k;
 const float top = r.getY(), bottom = r.getBottom(), sy = bottom - s * r.getHeight();
 juce::Path p;
 p.startNewSubPath(r.getX(), bottom);
 p.quadraticTo((r.getX() + x1) * .5f, top + (bottom - top) * .15f, x1, top);
 p.quadraticTo(x1 + (x2 - x1) * .25f, sy, x2, sy);
 p.lineTo(x3, sy);
 p.quadraticTo(x3 + (x4 - x3) * .2f, bottom, x4, bottom);
 return p;
}
// What CHARACTER and MOTION do for each source type (tooltips and the screen).
juce::String charMeaning(int t) {
 static const char* n[] = {"SHAPE: SINE > TRIANGLE > SAW > SQUARE", "FM RATIO (BETWEEN STEPS: INHARMONIC)", "COLOUR: DARK > BRIGHT",
                           "START POINT", "POSITION IN THE RECORDING", "MATERIAL (24)", "PULSE WIDTH"};
 return n[juce::jlimit(0, 6, t)];
}
juce::String motionMeaning(int t) {
 static const char* n[] = {"UNISON DETUNE", "FM INDEX", "BROAD > NARROW BAND ON THE NOTE", "LOOP LENGTH (0 = ONE-SHOT)",
                           "SMALLER, MORE SCATTERED GRAINS", "STRUCK > BOWED", "BREATH"};
 return n[juce::jlimit(0, 6, t)];
}
}

// ---------------------------------------------------------------- generator bar

EnkiGeneratorBar::EnkiGeneratorBar(InversoEnkiProcessor& p) : processor(p), amount(c::orange) {
 category.addItemList(juce::StringArray::fromTokens(specs()[P::Category].choices, "|", ""), 1);
 categoryAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, specs()[P::Category].id, category);
 amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, specs()[P::Mutate].id, amount);
 for (auto* comp : std::initializer_list<juce::Component*>{&category, &generateButton, &mutateButton, &amount, &backButton, &forwardButton, &saveButton, &favoritesButton})
  addAndMakeVisible(comp);
 category.setTooltip("What GENERATE invents: musical sounds, textures, hits, rhythmic sounds, or anything at all");
 generateButton.setTooltip("Invent a new sound in the chosen category (locked sections stay)");
 mutateButton.setTooltip("Change the current sound a little or a lot (the knob sets how much)");
 amount.setTooltip("How far MUTATE moves the sound");
 backButton.setTooltip("Previous sound");
 forwardButton.setTooltip("Next sound");
 saveButton.setTooltip("Keep this sound in your favourites");
 favoritesButton.setTooltip("Your saved sounds");
 generateButton.onClick = [this] { processor.generate(); refresh(); };
 mutateButton.onClick = [this] { processor.mutate(); refresh(); };
 backButton.onClick = [this] { processor.back(); refresh(); };
 forwardButton.onClick = [this] { processor.forward(); refresh(); };
 saveButton.onClick = [this] {
  auto* window = new juce::AlertWindow("Save sound", "A name for this sound:", juce::MessageBoxIconType::NoIcon, this);
  window->addTextEditor("name", "Sound " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H.%M"));
  window->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
  window->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
  juce::Component::SafePointer<EnkiGeneratorBar> safe(this);
  window->enterModalState(true, juce::ModalCallbackFunction::create([safe, window](int result) {
   if (safe != nullptr && result == 1) safe->processor.saveFavorite(window->getTextEditorContents("name"));
  }), true);
 };
 favoritesButton.onClick = [this] {
  const auto files = InversoEnkiProcessor::favorites();
  juce::PopupMenu menu;
  if (files.isEmpty()) menu.addItem(-1, "No favourites yet: press SAVE on a sound you like", false);
  for (int i = 0; i < files.size(); ++i) menu.addItem(i + 1, files[i].getFileNameWithoutExtension());
  menu.addSeparator();
  menu.addItem(10000, "Open the favourites folder");
  juce::Component::SafePointer<EnkiGeneratorBar> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&favoritesButton), [safe, files](int chosen) {
   if (safe == nullptr || chosen <= 0) return;
   if (chosen == 10000) { EnkiLibrary::favoritesFolder().createDirectory(); EnkiLibrary::favoritesFolder().revealToUser(); return; }
   safe->processor.loadFavorite(files[chosen - 1]);
   safe->refresh();
  });
 };
 refresh();
}

void EnkiGeneratorBar::refresh() {
 position = juce::String(processor.historyPosition() + 1) + " / " + juce::String(processor.historySize());
 backButton.setEnabled(processor.historyPosition() > 0);
 forwardButton.setEnabled(processor.historyPosition() + 1 < processor.historySize());
 repaint();
}

void EnkiGeneratorBar::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(8.f);
 juce::Path p;
 p.addRoundedRectangle(r, 22.f);
 juce::DropShadow(c::shadow.withAlpha(.4f), 12, {3, 4}).drawForPath(g, p);
 juce::DropShadow(juce::Colours::white, 8, {-3, -3}).drawForPath(g, p);
 g.setGradientFill(juce::ColourGradient(juce::Colours::white, 0.f, r.getY(), c::card, 0.f, r.getBottom(), false));
 g.fillPath(p);
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::muted);
 g.drawText("SOUND", backButton.getBounds().getUnion(forwardButton.getBounds()).translated(0, -14).withHeight(12), juce::Justification::centred);
 g.setColour(c::ink);
 g.setFont(soft::monoFont(11.f));
 g.drawText(position, backButton.getRight(), backButton.getY(), forwardButton.getX() - backButton.getRight(), backButton.getHeight(), juce::Justification::centred);
 g.setColour(c::muted);
 g.setFont(soft::uiFont(9.f, true));
 g.drawText("CATEGORY", category.getBounds().translated(0, -14).withHeight(12), juce::Justification::centred);
 g.drawText("AMOUNT", amount.getBounds().translated(0, -12).withHeight(12), juce::Justification::centred);
}

void EnkiGeneratorBar::resized() {
 auto r = getLocalBounds().reduced(26, 12);
 const int mid = r.getCentreY();
 auto take = [&](int w) { auto a = r.removeFromLeft(w); r.removeFromLeft(12); return a; };
 category.setBounds(take(170).withSizeKeepingCentre(170, 30).withY(mid - 10));
 generateButton.setBounds(take(200).withSizeKeepingCentre(200, 46));
 mutateButton.setBounds(take(120).withSizeKeepingCentre(120, 38));
 amount.setBounds(take(48).withSizeKeepingCentre(44, 44).translated(0, 6));
 auto hist = take(170);
 backButton.setBounds(hist.removeFromLeft(44).withSizeKeepingCentre(44, 36).translated(0, 6));
 forwardButton.setBounds(hist.removeFromRight(44).withSizeKeepingCentre(44, 36).translated(0, 6));
 saveButton.setBounds(take(100).withSizeKeepingCentre(100, 38));
 favoritesButton.setBounds(r.withSizeKeepingCentre(std::min(r.getWidth(), 140), 38));
}

// ---------------------------------------------------------------- sounds panel

EnkiSoundsPanel::EnkiSoundsPanel(InversoEnkiProcessor& p) : processor(p) {
 list.setRowHeight(22);
 list.setColour(juce::ListBox::backgroundColourId, c::screenBottom);
 list.setColour(juce::ListBox::outlineColourId, c::shadow.withAlpha(.3f));
 list.setOutlineThickness(1);
 for (auto* comp : std::initializer_list<juce::Component*>{&list, &draw, &importFiles, &importFolder, &openFolder, &toA, &toB, &query, &key,
                                                         &searchButton, &saveKey, &getKey, &getToA, &getToB, &getOnly})
  addChildComponent(comp);
 for (auto* ed : {&query, &key}) {
  ed->setFont(soft::uiFont(13.f));
  ed->setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
  ed->setColour(juce::TextEditor::textColourId, c::ink);
  ed->setColour(juce::TextEditor::outlineColourId, c::faint);
  ed->setColour(juce::TextEditor::focusedOutlineColourId, c::orange);
 }
 query.setTextToShowWhenEmpty("metal, rain, choir, door, whisper...", c::muted);
 key.setTextToShowWhenEmpty("paste your Freesound API key", c::muted);
 key.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
 key.setText(EnkiLibrary::instance().apiKey(), false);
 query.onReturnKey = [this] { searchButton.triggerClick(); };
 importFiles.setTooltip("Copy audio files into ENKI's own folder (the originals stay where they are)");
 importFolder.setTooltip("Copy a whole folder (a sample library) into ENKI's own folder, keeping its subfolders");
 openFolder.setTooltip("Show ENKI's sound folder (Documents/INVERSO ENKI/Sounds)");
 draw.setTooltip("Where GENERATE takes recordings from");
 toA.setTooltip("Play the selected sound in source A");
 toB.setTooltip("Play the selected sound in source B");
 getKey.setTooltip("Opens freesound.org/apiv2/apply: a free account and key, once");
 getToA.setTooltip("Download the selected sound into your library and play it in source A");
 importFiles.onClick = [this] {
  chooser = std::make_unique<juce::FileChooser>("Sounds to copy into ENKI", juce::File(), "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
  chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                       [](const juce::FileChooser& fc) { if (!fc.getResults().isEmpty()) EnkiLibrary::instance().import(fc.getResults()); });
 };
 importFolder.onClick = [this] {
  chooser = std::make_unique<juce::FileChooser>("A folder (sample library) to copy into ENKI");
  chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                       [](const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) EnkiLibrary::instance().import({fc.getResult()}); });
 };
 openFolder.onClick = [] { EnkiLibrary::soundsFolder().createDirectory(); EnkiLibrary::soundsFolder().revealToUser(); };
 draw.onChange = [this] {
  const int id = draw.getSelectedId();
  processor.drawFolder = id == 1 ? juce::String() : id == 2 ? juce::String("-") : draw.getText();
  refreshList();
 };
 toA.onClick = [this] { sendToSource(0); };
 toB.onClick = [this] { sendToSource(1); };
 saveKey.onClick = [this] { EnkiLibrary::instance().setApiKey(key.getText()); status = "KEY SAVED"; repaint(); };
 getKey.onClick = [] { juce::URL("https://freesound.org/apiv2/apply").launchInDefaultBrowser(); };
 searchButton.onClick = [this] {
  if (key.getText().trim() != EnkiLibrary::instance().apiKey()) EnkiLibrary::instance().setApiKey(key.getText());
  if (query.getText().trim().isNotEmpty()) EnkiLibrary::instance().search(query.getText().trim());
 };
 auto get = [this](int source) {
  const auto results = EnkiLibrary::instance().results();
  const int row = list.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(results.size())) return;
  waiting = {results[static_cast<std::size_t>(row)].id, source, juce::Time::getMillisecondCounterHiRes()};
  EnkiLibrary::instance().download(results[static_cast<std::size_t>(row)]);
 };
 getToA.onClick = [get] { get(0); };
 getToB.onClick = [get] { get(1); };
 getOnly.onClick = [get] { get(-1); };
 refreshFolders();
 setTab(0);
 startTimerHz(5);
}

EnkiSoundsPanel::~EnkiSoundsPanel() { list.setModel(nullptr); }

void EnkiSoundsPanel::setTab(int t) {
 tab = t;
 for (auto* comp : std::initializer_list<juce::Component*>{&draw, &importFiles, &importFolder, &openFolder, &toA, &toB}) comp->setVisible(tab == 0);
 for (auto* comp : std::initializer_list<juce::Component*>{&query, &key, &searchButton, &saveKey, &getKey, &getToA, &getToB, &getOnly}) comp->setVisible(tab == 1);
 list.setVisible(true);
 list.deselectAllRows();
 list.updateContent();
 resized();
 repaint();
}

void EnkiSoundsPanel::refreshFolders() {
 const auto keep = processor.drawFolder;
 draw.clear(juce::dontSendNotification);
 draw.addItem("DRAW FROM: ALL SOUNDS", 1);
 draw.addItem("DRAW FROM: ENKI RECORDINGS ONLY", 2);
 const auto folders = EnkiLibrary::instance().folders();
 for (int i = 0; i < folders.size(); ++i) draw.addItem(folders[i], 3 + i);
 if (keep.isEmpty()) draw.setSelectedId(1, juce::dontSendNotification);
 else if (keep == "-") draw.setSelectedId(2, juce::dontSendNotification);
 else for (int i = 0; i < folders.size(); ++i) if (folders[i] == keep) draw.setSelectedId(3 + i, juce::dontSendNotification);
 refreshList();
}

void EnkiSoundsPanel::refreshList() {
 const auto folder = processor.drawFolder == "-" ? juce::String() : processor.drawFolder;
 shown = EnkiLibrary::instance().filesIn(folder);
 list.updateContent();
 list.repaint();
}

int EnkiSoundsPanel::getNumRows() {
 return tab == 0 ? static_cast<int>(shown.size()) : static_cast<int>(EnkiLibrary::instance().results().size());
}

void EnkiSoundsPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) {
 if (selected) { g.setColour(c::orange.withAlpha(.35f)); g.fillRect(0, 0, width, height); }
 g.setFont(soft::monoFont(11.f));
 if (tab == 0) {
  if (row < 0 || row >= static_cast<int>(shown.size())) return;
  const auto f = EnkiLibrary::instance().file(shown[static_cast<std::size_t>(row)]);
  g.setColour(c::screenInk);
  g.drawText(f.getRelativePathFrom(EnkiLibrary::soundsFolder()).replaceCharacter('\\', '/'), 8, 0, width - 12, height, juce::Justification::centredLeft);
 } else {
  const auto results = EnkiLibrary::instance().results();
  if (row < 0 || row >= static_cast<int>(results.size())) return;
  const auto& r = results[static_cast<std::size_t>(row)];
  g.setColour(c::screenInk);
  g.drawText(r.name, 8, 0, width - 140, height, juce::Justification::centredLeft);
  g.setColour(c::screenMuted);
  g.drawText(juce::String(r.seconds, 1) + " s  " + r.user, width - 130, 0, 124, height, juce::Justification::centredRight);
 }
}

void EnkiSoundsPanel::listBoxItemDoubleClicked(int, const juce::MouseEvent&) {
 if (tab == 0) sendToSource(0);
 else getToA.triggerClick();
}

void EnkiSoundsPanel::sendToSource(int source) {
 const int row = list.getSelectedRow();
 if (row < 0 || row >= static_cast<int>(shown.size())) return;
 const auto f = EnkiLibrary::instance().file(shown[static_cast<std::size_t>(row)]);
 status = processor.loadSourceFile(source, f) ? juce::String("PLAYING IN SOURCE ") + (source == 0 ? "A" : "B") + ": " + f.getFileNameWithoutExtension().toUpperCase()
                                               : juce::String("COULD NOT READ THAT FILE");
 repaint();
}

void EnkiSoundsPanel::timerCallback() {
 auto& lib = EnkiLibrary::instance();
 if (lib.version() != seenIndex) { seenIndex = lib.version(); refreshFolders(); }
 if (lib.resultsVersion() != seenResults) { seenResults = lib.resultsVersion(); if (tab == 1) { list.updateContent(); list.repaint(); } }
 const auto s = lib.status();
 if (s.isNotEmpty() && s != status) { status = s; repaint(); }
 // A download asked for a source: load it as soon as it lands.
 if (waiting.id > 0 && waiting.source >= 0) {
  for (const auto& e : juce::RangedDirectoryIterator(EnkiLibrary::soundsFolder().getChildFile("Freesound"), false, juce::String(waiting.id) + " *", juce::File::findFiles)) {
   if (processor.loadSourceFile(waiting.source, e.getFile())) status = juce::String("PLAYING IN SOURCE ") + (waiting.source == 0 ? "A" : "B") + ": " + e.getFile().getFileNameWithoutExtension().toUpperCase();
   waiting = {};
   repaint();
   break;
  }
  if (waiting.id > 0 && juce::Time::getMillisecondCounterHiRes() - waiting.since > 60000.0) waiting = {};
 }
}

juce::Rectangle<int> EnkiSoundsPanel::tabArea(int index) const {
 auto inner = getLocalBounds().reduced(10).reduced(16, 12);
 return {inner.getX() + 90 + index * 110, inner.getY(), 104, 20};
}

void EnkiSoundsPanel::mouseUp(const juce::MouseEvent& e) {
 for (int i = 0; i < 2; ++i) if (tabArea(i).expanded(2, 4).contains(e.getPosition())) { setTab(i); return; }
}

void EnkiSoundsPanel::paint(juce::Graphics& g) {
 const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
 const int w = juce::roundToInt(static_cast<float>(getWidth()) * scale), h = juce::roundToInt(static_cast<float>(getHeight()) * scale);
 if (cardCache.getWidth() != w || cardCache.getHeight() != h) {
  cardCache = juce::Image(juce::Image::ARGB, std::max(1, w), std::max(1, h), true);
  juce::Graphics cg(cardCache);
  cg.addTransform(juce::AffineTransform::scale(scale));
  soft::paintCard(cg, getLocalBounds().toFloat().reduced(10.f));
 }
 g.drawImageTransformed(cardCache, juce::AffineTransform::scale(1.f / scale));
 const auto inner = getLocalBounds().reduced(10).reduced(16, 12);
 g.setColour(c::orange);
 g.fillEllipse(static_cast<float>(inner.getX()), static_cast<float>(inner.getY()) + 6.f, 8.f, 8.f);
 g.setColour(c::ink);
 g.setFont(soft::titleFont(12.5f));
 g.drawText("SOUNDS", inner.getX() + 16, inner.getY(), 80, 20, juce::Justification::centredLeft);
 const char* names[] = {"MY SOUNDS", "FREESOUND"};
 for (int i = 0; i < 2; ++i) {
  const auto t = tabArea(i);
  g.setColour(i == tab ? c::ink : c::muted.withAlpha(.8f));
  g.setFont(soft::uiFont(10.f, true).withExtraKerningFactor(.05f));
  g.drawText(names[i], t, juce::Justification::centred);
  if (i == tab) { g.setColour(c::orange); g.fillRoundedRectangle(static_cast<float>(t.getCentreX()) - 7.f, static_cast<float>(t.getBottom()), 14.f, 2.5f, 1.25f); }
 }
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::muted);
 const auto foot = inner.withTrimmedTop(inner.getHeight() - 16);
 g.setColour(c::ink.withAlpha(.75f));
 g.drawFittedText(status, foot, juce::Justification::centredLeft, 1, .8f);
}

void EnkiSoundsPanel::paintOverChildren(juce::Graphics& g) {
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::screenMuted);
 if (tab == 0 && shown.empty())
  g.drawFittedText("Your library is empty. IMPORT a folder (a sample library) or files: ENKI copies them into Documents/INVERSO ENKI/Sounds.",
                   list.getBounds().reduced(20), juce::Justification::centred, 4);
 if (tab == 1 && EnkiLibrary::instance().results().empty())
  g.drawFittedText(EnkiLibrary::instance().apiKey().isEmpty()
                       ? "Freesound needs a free key, once: GET A FREE KEY, sign in, copy the key, paste it above and SAVE KEY. Only free (CC0) sounds are searched."
                       : "Search for anything: rain, metal, choir, door, whisper, engine...",
                   list.getBounds().reduced(20), juce::Justification::centred, 4);
}

void EnkiSoundsPanel::resized() {
 cardCache = {};
 auto inner = getLocalBounds().reduced(10).reduced(16, 12);
 inner.removeFromTop(30);
 inner.removeFromBottom(20);
 if (tab == 0) {
  auto row = inner.removeFromTop(32);
  const int bw = (row.getWidth() - 16) / 3;
  importFiles.setBounds(row.removeFromLeft(bw)); row.removeFromLeft(8);
  importFolder.setBounds(row.removeFromLeft(bw)); row.removeFromLeft(8);
  openFolder.setBounds(row);
  inner.removeFromTop(8);
  draw.setBounds(inner.removeFromTop(28));
  inner.removeFromTop(8);
  auto bottom = inner.removeFromBottom(32);
  toA.setBounds(bottom.removeFromLeft(bottom.getWidth() / 2 - 4));
  bottom.removeFromLeft(8);
  toB.setBounds(bottom);
  inner.removeFromBottom(8);
  list.setBounds(inner);
 } else {
  auto row = inner.removeFromTop(30);
  getKey.setBounds(row.removeFromRight(130)); row.removeFromRight(6);
  saveKey.setBounds(row.removeFromRight(90)); row.removeFromRight(6);
  key.setBounds(row);
  inner.removeFromTop(8);
  row = inner.removeFromTop(30);
  searchButton.setBounds(row.removeFromRight(100)); row.removeFromRight(6);
  query.setBounds(row);
  inner.removeFromTop(8);
  auto bottom = inner.removeFromBottom(32);
  const int bw = (bottom.getWidth() - 16) / 3;
  getToA.setBounds(bottom.removeFromLeft(bw)); bottom.removeFromLeft(8);
  getToB.setBounds(bottom.removeFromLeft(bw)); bottom.removeFromLeft(8);
  getOnly.setBounds(bottom);
  inner.removeFromBottom(8);
  list.setBounds(inner);
 }
}

// ---------------------------------------------------------------- editor

float InversoEnkiEditor::value(int param) const {
 return processor.apvts.getRawParameterValue(specs()[static_cast<std::size_t>(param)].id)->load();
}

void InversoEnkiEditor::Recipe::paint(juce::Graphics& g) {
 const auto r = getLocalBounds().toFloat().reduced(10.f);
 soft::paintCard(g, r);
 auto inner = r.reduced(16.f, 12.f);
 g.setColour(c::pink);
 g.fillEllipse(inner.getX(), inner.getY() + 6.f, 8.f, 8.f);
 g.setColour(c::ink);
 g.setFont(soft::titleFont(12.5f));
 g.drawText("WHAT ENKI MADE", inner.withHeight(20.f).withTrimmedLeft(16.f), juce::Justification::centredLeft);
 g.setFont(soft::monoFont(10.f));
 g.setColour(c::muted);
 g.drawText("SOUND " + juce::String(at) + " OF " + juce::String(of), inner.withHeight(20.f), juce::Justification::centredRight);
 inner.removeFromTop(32.f);
 soft::paintScreen(g, inner);
 auto box = inner.reduced(16.f, 14.f);
 const auto lines = juce::StringArray::fromLines(text);
 g.setColour(c::screenInk);
 g.setFont(soft::uiFont(17.f, true));
 g.drawFittedText(lines[0], box.removeFromTop(box.getHeight() * .55f).toNearestInt(), juce::Justification::centredLeft, 3, .8f);
 g.setColour(c::screenMuted);
 g.setFont(soft::monoFont(12.f));
 g.drawFittedText(lines[1].toUpperCase(), box.toNearestInt(), juce::Justification::topLeft, 3, .8f);
}

InversoEnkiEditor::InversoEnkiEditor(InversoEnkiProcessor& p) : AudioProcessorEditor(&p), processor(p), bar(p), sounds(p) {
 setLookAndFeel(&look);
 body.title = "ENKI";
 body.subtitle = "SOUND IDEA GENERATOR";
 addAndMakeVisible(body);
 body.addAndMakeVisible(bar);
 body.addAndMakeVisible(sounds);
 body.addAndMakeVisible(recipe);

 auto& apvts = processor.apvts;
 using Page = soft::Module::Page;
 auto add = [&](juce::String title, juce::Colour accent, std::vector<Page> pages, int screenH, std::vector<int> choices) -> soft::Module& {
  modules.push_back(std::make_unique<soft::Module>(apvts, specs().data(), shortNames(), title, accent, std::move(pages), screenH, std::move(choices)));
  body.addAndMakeVisible(*modules.back());
  return *modules.back();
 };
 for (int k = 0; k < 2; ++k) {
  const int b = k * P::SourceStride;
  add(k == 0 ? "SOURCE A" : "SOURCE B", k == 0 ? c::amber : c::cyan,
      {{"Sound", {P::ALevel + b, P::APitch + b, P::AFine + b, P::AChar + b, P::AMotion + b}, {P::ALevel + b}}}, 130, {P::AType + b});
 }
 add("SHAPE", c::violet, {{"Filter", {P::Cutoff, P::Resonance, P::FilterEnv, P::Keytrack}, {P::Cutoff}}, {"Drive", {P::Drive, P::Crush, P::Ring}}}, 96,
     {P::FilterMode, P::DriveType});
 add("ENVELOPES", c::green, {{"Level", {P::Attack, P::Decay, P::Sustain, P::Release, P::Hold}}, {"Mod", {P::ModAttack, P::ModDecay, P::PitchEnv}}}, 96, {});
 add("MOTION", c::blue, {{"LFO", {P::LfoRate, P::LfoPitch, P::LfoFilter, P::LfoAmp, P::LfoPan, P::LfoChar}, {P::LfoRate}},
                          {"Rhythm", {P::Gate, P::GatePattern, P::Glitch, P::Reverse}, {P::Gate}}}, 96, {P::LfoShape, P::LfoSync, P::GateDiv});
 add("SPACE", c::blue.interpolatedWith(c::violet, .5f), {{"Echo", {P::EchoMix, P::EchoTime, P::EchoFeedback}, {P::EchoMix}},
                                                         {"Reverb", {P::RevMix, P::RevSize, P::RevDecay}, {P::RevMix}}}, 96, {P::EchoSync});
 add("PLAY", c::pink, {{"Play", {P::Octave, P::Glide, P::VelSens, P::BendRange}}}, 0, {});
 master = std::make_unique<soft::Master>(processor.apvts, specs()[P::Output], &specs()[P::Width], juce::String("WIDTH"), c::ink);
 body.addAndMakeVisible(*master);

 // Lock buttons in the module headers: GENERATE and MUTATE leave a locked section alone.
 const int lockParams[5] = {P::LockA, P::LockB, P::LockShape, P::LockMotion, P::LockSpace};
 const int lockModules[5] = {0, 1, 2, 4, 5};
 for (int i = 0; i < 5; ++i) {
  auto& b = locks[static_cast<std::size_t>(i)];
  b.setClickingTogglesState(true);
  b.setButtonText("LOCK");
  b.setTooltip(i == 2 ? "Keep the filter, drive and envelopes when generating" : "Keep this section when generating or mutating");
  b.onStateChange = [&b] {
   const auto text = b.getToggleState() ? "LOCKED" : "LOCK";
   if (b.getButtonText() != text) b.setButtonText(text);
  };
  lockAttach[static_cast<std::size_t>(i)] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, specs()[static_cast<std::size_t>(lockParams[i])].id, b);
  modules[static_cast<std::size_t>(lockModules[i])]->addHeaderComponent(b, 76);
 }
 for (int k = 0; k < 2; ++k) {
  auto& b = sampleButtons[static_cast<std::size_t>(k)];
  b.setTooltip("The recording this source plays (RECORDING and GRAINS types): ENKI's own or one from your library");
  b.onClick = [this, k] { chooseSample(k); };
  modules[static_cast<std::size_t>(k)]->addHeaderComponent(b, 170);
 }

 auto& m = modules;
 for (int k = 0; k < 2; ++k) {
  m[static_cast<std::size_t>(k)]->screen.draw = [this, k](juce::Graphics& g, juce::Rectangle<float> r, int) {
   const int b = k * P::SourceStride;
   const int type = juce::roundToInt(value(P::AType + b));
   const float level = value(P::ALevel + b), ch = value(P::AChar + b) * .01f, mo = value(P::AMotion + b) * .01f;
   const auto colour = (k == 0 ? c::amber : c::cyan).withAlpha(level > 0.f ? 1.f : .3f);
   auto area = r.withTrimmedBottom(34.f);
   caption(g, area, charMeaning(type), motionMeaning(type));
   auto plot = area.withTrimmedTop(16.f).reduced(0.f, 2.f);
   if (type == 3 || type == 4) {
    const auto s = processor.sourceDisplay(k);
    if (!s || s->data.empty()) { notice(g, plot, "NO RECORDING: CHOOSE ONE ABOVE"); return; }
    const int columns = static_cast<int>(plot.getWidth());
    const auto n = s->data.size();
    juce::Path wave;
    for (int x = 0; x < columns; ++x) {
     const auto from = n * static_cast<std::size_t>(x) / static_cast<std::size_t>(columns), to = std::max(from + 1, n * static_cast<std::size_t>(x + 1) / static_cast<std::size_t>(columns));
     float pk = 0.f;
     for (auto i = from; i < to && i < n; i += 1 + (to - from) / 64) pk = std::max(pk, std::abs(s->data[i]));
     const float px = plot.getX() + static_cast<float>(x);
     wave.addRectangle(px, plot.getCentreY() - pk * plot.getHeight() * .48f, 1.f, std::max(1.f, pk * plot.getHeight() * .96f));
    }
    g.setColour(colour.withAlpha(.75f));
    g.fillPath(wave);
    const float at = plot.getX() + ch * plot.getWidth();
    g.setColour(c::screenInk);
    g.fillRect(at - 1.f, plot.getY(), 2.f, plot.getHeight());
    if (type == 4) { g.setColour(colour.withAlpha(.18f)); g.fillRect(at, plot.getY(), plot.getWidth() * (.02f + .3f * mo), plot.getHeight()); }
    else if (mo > .02f) { g.setColour(colour.withAlpha(.18f)); g.fillRect(at, plot.getY(), (plot.getRight() - at) * (1.f - mo), plot.getHeight()); }
    return;
   }
   juce::Random rnd(11);
   soft::glowStroke(g, curve(plot, 220, [&](float t) {
    const float ph = t * 3.f - std::floor(t * 3.f);
    switch (type) {
     case 0: {
      const float sh = ch * 3.f;
      const float w[4] = {std::sin(ph * 6.2831853f), 1.f - 4.f * std::abs(ph - .5f), 2.f * ph - 1.f, ph < .5f ? 1.f : -1.f};
      const int a = juce::jlimit(0, 2, static_cast<int>(sh));
      return .8f * (w[a] + (sh - static_cast<float>(a)) * (w[a + 1] - w[a]));
     }
     case 1: return .8f * std::sin(ph * 6.2831853f + mo * 6.f * std::sin(ph * 6.2831853f * (1.f + std::floor(ch * 8.f))));
     case 2: return (rnd.nextFloat() * 2.f - 1.f) * (.3f + .5f * (1.f - mo)) * (.5f + .5f * ch) + mo * .6f * std::sin(t * 6.2831853f * 12.f);
     case 5: {
      const auto& ratios = inverso_abismo::materialRatios(juce::jlimit(0, inverso_abismo::numMaterials - 1, static_cast<int>(ch * 23.99f)));
      float x = 0.f;
      for (int i = 0; i < 6; ++i) x += std::sin(t * 6.2831853f * 4.f * ratios[static_cast<std::size_t>(i)]) / (1.f + static_cast<float>(i));
      return .55f * x * std::exp(-t * (3.f - 2.5f * mo));
     }
     default: { const float duty = .08f + .4f * ch; return .7f * (ph < duty ? 1.f : -duty / (1.f - duty)); }
    }
   }), colour);
  };
 }
 m[2]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto area = r.withTrimmedBottom(34.f);
  if (page == 1) {
   const float drive = value(P::Drive) * .01f;
   caption(g, area, "DRIVE " + juce::String(juce::roundToInt(drive * 100.f)) + "%", "RING " + juce::String(juce::roundToInt(value(P::Ring))) + "%");
   inverso_abismo::relicario::Drive preview;
   const int type = juce::roundToInt(value(P::DriveType));
   soft::glowStroke(g, curve(area.withTrimmedTop(14.f), 160, [&](float t) {
    const float x = std::sin(t * 6.2831853f) * .6f;
    return .85f * juce::jlimit(-1.f, 1.f, drive > 0.f ? preview.process(x, type == 4 ? 0 : type, drive) : x);
   }), c::violet.withAlpha(drive > 0.f ? 1.f : .35f));
   return;
  }
  const int mode = juce::roundToInt(value(P::FilterMode));
  const float fc = value(P::Cutoff), k = 2.f - 1.94f * value(P::Resonance) * .01f, env = value(P::FilterEnv) * .01f;
  caption(g, area, abismo_ui::formatValue(specs()[P::Cutoff], fc), mode == 3 ? juce::String("COMB ON THE NOTE") : "ENV " + juce::String(juce::roundToInt(env * 100.f)) + "%");
  auto plot = area.withTrimmedTop(14.f);
  auto response = [&](float cutoff) {
   return curve(plot, 180, [&, cutoff](float t) {
    const float f = 20.f * std::pow(1000.f, t);
    if (mode == 3) return .4f * std::cos(t * 60.f) * std::exp(-t * (1.f - k * .4f)) + .1f;
    const float w = f / cutoff, re = 1.f - w * w, im = k * w, den = std::sqrt(re * re + im * im);
    const float mag = mode == 0 ? 1.f / den : mode == 1 ? (k * w) / den : (w * w) / den;
    return (juce::jlimit(-42.f, 24.f, 20.f * std::log10(std::max(1e-6f, mag))) + 9.f) / 33.f;
   });
  };
  if (mode < 3 && std::abs(env) > .001f) {
   g.setColour(c::violet.withAlpha(.35f));
   g.strokePath(response(juce::jlimit(20.f, 20000.f, fc * std::exp2(env * 6.f))), juce::PathStrokeType(1.f));
  }
  soft::glowStroke(g, response(fc), c::violet);
 };
 m[3]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  if (page == 1) {
   caption(g, r, "MOD ENVELOPE", "PITCH " + abismo_ui::formatValue(specs()[P::PitchEnv], value(P::PitchEnv)));
   soft::glowStroke(g, adsr(r.withTrimmedTop(14.f).reduced(2.f, 4.f), value(P::ModAttack), value(P::ModDecay), 0.f, value(P::ModDecay)), c::green);
   return;
  }
  const bool hold = value(P::Hold) >= .5f;
  caption(g, r, hold ? juce::String("ONE-SHOT: PLAYS OUT") : juce::String("LEVEL"));
  soft::glowStroke(g, adsr(r.withTrimmedTop(14.f).reduced(2.f, 4.f), value(P::Attack), value(P::Decay), hold ? 0.f : value(P::Sustain) * .01f, value(P::Release)), c::green);
 };
 m[4]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto area = r.withTrimmedBottom(34.f);
  if (page == 1) {
   const float gate = value(P::Gate) * .01f;
   const int pattern = juce::roundToInt(value(P::GatePattern)) & 255;
   caption(g, area, "GATE " + juce::String(juce::roundToInt(gate * 100.f)) + "%", value(P::Glitch) > 0.f ? juce::String("GLITCH") : juce::String());
   auto grid = area.withTrimmedTop(16.f);
   const float w = grid.getWidth() / 16.f;
   for (int i = 0; i < 16; ++i) {
    const bool on = ((pattern >> (i & 7)) & 1) != 0 || i == 0;
    auto cell = juce::Rectangle<float>(grid.getX() + w * static_cast<float>(i), grid.getY(), w, grid.getHeight()).reduced(2.f, 4.f);
    g.setColour(c::screenMuted.withAlpha(.18f));
    g.fillRoundedRectangle(cell, 2.f);
    g.setColour(c::blue.withAlpha(gate <= 0.f ? .3f : on ? .9f : .9f * (1.f - gate) + .05f));
    g.fillRoundedRectangle(cell.withWidth(cell.getWidth() * .8f), 2.f);
   }
   return;
  }
  const float rate = value(P::LfoRate);
  static const char* shapes[] = {"SINE", "TRIANGLE", "SQUARE", "RANDOM", "DRIFT"};
  caption(g, area, juce::String(shapes[juce::jlimit(0, 4, juce::roundToInt(value(P::LfoShape)))]) + " " + abismo_ui::formatValue(specs()[P::LfoRate], rate));
  const float now = processor.lfoNow.load();
  auto plot = area.withTrimmedTop(14.f);
  soft::glowStroke(g, curve(plot, 160, [&](float t) { return .75f * std::sin((t * 2.f - animation * .5f) * 6.2831853f); }), c::blue.withAlpha(.35f), 1.2f);
  g.setColour(c::screenInk);
  g.fillEllipse(juce::Rectangle<float>(8.f, 8.f).withCentre({plot.getRight() - 10.f, plot.getCentreY() - now * plot.getHeight() * .38f}));
 };
 m[5]->screen.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, int page) {
  auto area = r.withTrimmedBottom(34.f);
  if (page == 1) {
   const float mix = value(P::RevMix) * .01f, decay = value(P::RevDecay);
   caption(g, area, abismo_ui::formatValue(specs()[P::RevDecay], decay), "MIX " + juce::String(juce::roundToInt(mix * 100.f)) + "%");
   const float span = juce::jlimit(.05f, 1.f, std::log10(1.f + decay) / 1.4f);
   soft::glowStroke(g, curve(area.withTrimmedTop(14.f), 160, [&](float t) { return 1.6f * std::exp(-t / (span * .35f)) - 1.f; }), c::blue.withAlpha(mix > 0.f ? 1.f : .35f));
   return;
  }
  const float mix = value(P::EchoMix) * .01f, fb = value(P::EchoFeedback) * .01f;
  caption(g, area, "ECHO " + juce::String(juce::roundToInt(mix * 100.f)) + "%", "REPEATS " + juce::String(juce::roundToInt(fb * 100.f)) + "%");
  auto plot = area.withTrimmedTop(16.f).reduced(2.f, 2.f);
  float amp = 1.f;
  const float time = juce::roundToInt(value(P::EchoSync)) > 0 ? 400.f : value(P::EchoTime);
  for (int k = 0; k < 24; ++k) {
   const float x = plot.getX() + plot.getWidth() * juce::jmin(1.f, static_cast<float>(k) * time / 2400.f);
   if (x >= plot.getRight()) break;
   const float h = plot.getHeight() * (k == 0 ? 1.f : amp * (.35f + .65f * mix));
   juce::Path tick; tick.startNewSubPath(x, plot.getBottom()); tick.lineTo(x, plot.getBottom() - h);
   soft::glowStroke(g, tick, c::blue.withAlpha(k == 0 ? .5f : 1.f), 2.f);
   amp *= fb * .95f;
   if (k > 0 && amp < .01f) break;
  }
 };

 setResizable(true, true);
 setResizeLimits(designW * 6 / 10, designH * 6 / 10, designW * 3 / 2, designH * 3 / 2);
 getConstrainer()->setFixedAspectRatio(static_cast<double>(designW) / designH);
 setSize(designW * 9 / 10, designH * 9 / 10);
 timerCallback();
 startTimerHz(30);
}

InversoEnkiEditor::~InversoEnkiEditor() { setLookAndFeel(nullptr); }

void InversoEnkiEditor::chooseSample(int source) {
 juce::PopupMenu menu, recordings, mine;
 for (int i = 0; i < numLibrarySamples; ++i) recordings.addItem(1 + i, inverso_abismo::drums::sampleObjectName(i));
 auto& lib = EnkiLibrary::instance();
 std::vector<juce::File> files;
 const auto folders = lib.folders();
 for (const auto& folder : folders) {
  juce::PopupMenu sub;
  int count = 0;
  for (int index : lib.filesIn(folder)) {
   if (++count > 400) { sub.addItem(-1, "(more: use the SOUNDS panel)", false); break; }
   files.push_back(lib.file(index));
   sub.addItem(1000 + static_cast<int>(files.size()) - 1, lib.file(index).getRelativePathFrom(EnkiLibrary::soundsFolder().getChildFile(folder)));
  }
  mine.addSubMenu(folder, sub);
 }
 menu.addSubMenu("ENKI recordings (scrapyard)", recordings);
 if (folders.isEmpty()) menu.addItem(-1, "Your sounds: empty (IMPORT them in the SOUNDS panel)", false);
 else menu.addSubMenu("Your sounds", mine);
 juce::Component::SafePointer<InversoEnkiEditor> safe(this);
 menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sampleButtons[static_cast<std::size_t>(source)]).withMaximumNumColumns(3),
                    [safe, files, source](int chosen) {
  if (safe == nullptr || chosen <= 0) return;
  if (chosen <= numLibrarySamples) { safe->processor.useLibrarySample(source, chosen - 1); return; }
  const auto at = static_cast<std::size_t>(chosen - 1000);
  if (at < files.size()) safe->processor.loadSourceFile(source, files[at]);
 });
}

// A control whose effect depends on another one is dimmed while it does nothing.
static bool enkiControlActive(const juce::AudioProcessorValueTreeState& apvts, int p) {
 auto v = [&](int i) { return apvts.getRawParameterValue(specs()[static_cast<std::size_t>(i)].id)->load(); };
 if (p >= P::APitch && p <= P::AMotion) return v(P::ALevel) > 0.f || v(P::Ring) > 0.f;
 if (p >= P::BPitch && p <= P::BMotion) return v(P::BLevel) > 0.f || v(P::Ring) > 0.f;
 switch (p) {
  case P::Sustain: case P::Release: return v(P::Hold) < .5f;
  case P::Decay: return v(P::Sustain) < 100.f || v(P::Hold) >= .5f;
  case P::DriveType: return v(P::Drive) > 0.f;
  case P::LfoRate: return juce::roundToInt(v(P::LfoSync)) == 0;
  case P::GateDiv: case P::GatePattern: return v(P::Gate) > 0.f;
  case P::EchoTime: return v(P::EchoMix) > 0.f && juce::roundToInt(v(P::EchoSync)) == 0;
  case P::EchoFeedback: case P::EchoSync: return v(P::EchoMix) > 0.f;
  case P::RevSize: case P::RevDecay: return v(P::RevMix) > 0.f;
  default: return true;
 }
}

void InversoEnkiEditor::timerCallback() {
 animation += 1.f / 30.f;
 for (auto& m : modules) {
  m->dimInactive([this](int p) { return enkiControlActive(processor.apvts, p); });
  m->screen.repaint();
 }
 for (int k = 0; k < 2; ++k) {
  const int type = juce::roundToInt(value(P::AType + k * P::SourceStride));
  auto& b = sampleButtons[static_cast<std::size_t>(k)];
  const auto text = (type == 3 || type == 4 ? juce::String() : juce::String("(")) + processor.sourceSampleName(k).toUpperCase()
                    + (type == 3 || type == 4 ? juce::String() : juce::String(")")) + juce::String::fromUTF8(" \xe2\x96\xbe");
  if (b.getButtonText() != text) b.setButtonText(text);
 }
 if (processor.soundVersion.load() != seenVersion || recipe.at != processor.historyPosition() + 1 || recipe.of != processor.historySize()) {
  seenVersion = processor.soundVersion.load();
  recipe.text = processor.recipe();
  recipe.at = processor.historyPosition() + 1;
  recipe.of = processor.historySize();
  recipe.repaint();
  bar.refresh();
 }
 master->setReadout(processor.outputPeak.load(), juce::String("VOICES ") + juce::String(processor.voicesSounding.load()) + " / 8");
}

void InversoEnkiEditor::resized() {
 body.setBounds(0, 0, designW, designH);
 body.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW));
 bar.setBounds(290, 4, designW - 300, 84);
 const int x0 = 10, y0 = 88;
 const int r1 = 330, r2 = 290, r3 = designH - y0 - r1 - r2 - 12;
 auto& m = modules;
 m[0]->setBounds(x0, y0, 380, r1);
 m[1]->setBounds(x0 + 380, y0, 380, r1);
 sounds.setBounds(x0 + 760, y0, designW - (x0 + 760) - 10, r1);
 m[2]->setBounds(x0, y0 + r1, 380, r2);
 m[3]->setBounds(x0 + 380, y0 + r1, 380, r2);
 m[4]->setBounds(x0 + 760, y0 + r1, 390, r2);
 m[5]->setBounds(x0, y0 + r1 + r2, 380, r3);
 m[6]->setBounds(x0 + 380, y0 + r1 + r2, 240, r3);
 recipe.setBounds(x0 + 620, y0 + r1 + r2, 530, r3);
 master->setBounds(x0 + 1150, y0 + r1, designW - (x0 + 1150) - 10, r2 + r3);
}
