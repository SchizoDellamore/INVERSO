#include "EnkiLibrary.h"

namespace {
const char* const audioPattern = "*.wav;*.wave;*.aif;*.aiff;*.flac;*.ogg;*.mp3";
constexpr int maxImportFiles = 20000;
}

EnkiLibrary& EnkiLibrary::instance() {
 static EnkiLibrary library;
 return library;
}

EnkiLibrary::EnkiLibrary() : juce::Thread("ENKI library") {
 if (auto xml = juce::parseXML(settingsFile())) key = xml->getStringAttribute("freesoundKey");
 rescan();
 startThread(juce::Thread::Priority::low);
}

EnkiLibrary::~EnkiLibrary() {
 signalThreadShouldExit();
 wake.signal();
 stopThread(4000);
}

juce::File EnkiLibrary::home() {
 return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("INVERSO ENKI");
}
juce::File EnkiLibrary::soundsFolder() { return home().getChildFile("Sounds"); }
juce::File EnkiLibrary::favoritesFolder() { return home().getChildFile("Favorites"); }
juce::File EnkiLibrary::settingsFile() const { return home().getChildFile("settings.xml"); }

bool EnkiLibrary::isAudio(const juce::File& f) {
 return f.hasFileExtension("wav;wave;aif;aiff;flac;ogg;mp3");
}

void EnkiLibrary::rescan() {
 std::vector<juce::File> list;
 const auto root = soundsFolder();
 if (root.isDirectory())
  for (const auto& entry : juce::RangedDirectoryIterator(root, true, audioPattern, juce::File::findFiles))
   if (static_cast<int>(list.size()) < 200000) list.push_back(entry.getFile());
 std::sort(list.begin(), list.end(), [](const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural(b.getFullPathName()) < 0; });
 {
  const std::lock_guard<std::mutex> g(lock);
  files = std::move(list);
 }
 ++indexVersion;
}

int EnkiLibrary::size() const { const std::lock_guard<std::mutex> g(lock); return static_cast<int>(files.size()); }
juce::File EnkiLibrary::file(int index) const {
 const std::lock_guard<std::mutex> g(lock);
 return index >= 0 && index < static_cast<int>(files.size()) ? files[static_cast<std::size_t>(index)] : juce::File();
}

juce::StringArray EnkiLibrary::folders() const {
 juce::StringArray out;
 const auto root = soundsFolder();
 const std::lock_guard<std::mutex> g(lock);
 for (const auto& f : files) {
  const auto rel = f.getRelativePathFrom(root).replaceCharacter('\\', '/');
  out.addIfNotAlreadyThere(rel.containsChar('/') ? rel.upToFirstOccurrenceOf("/", false, false) : juce::String("(loose files)"));
 }
 out.sortNatural();
 return out;
}

std::vector<int> EnkiLibrary::filesIn(const juce::String& folder) const {
 std::vector<int> out;
 const auto root = soundsFolder();
 const std::lock_guard<std::mutex> g(lock);
 for (std::size_t i = 0; i < files.size(); ++i) {
  if (folder.isEmpty()) { out.push_back(static_cast<int>(i)); continue; }
  const auto rel = files[i].getRelativePathFrom(root).replaceCharacter('\\', '/');
  const auto top = rel.containsChar('/') ? rel.upToFirstOccurrenceOf("/", false, false) : juce::String("(loose files)");
  if (top == folder) out.push_back(static_cast<int>(i));
 }
 return out;
}

juce::File EnkiLibrary::randomFile(const juce::String& folder, juce::Random& rnd) const {
 const auto list = filesIn(folder);
 if (list.empty()) return {};
 return file(list[static_cast<std::size_t>(rnd.nextInt(static_cast<int>(list.size())))]);
}

juce::String EnkiLibrary::status() const { const std::lock_guard<std::mutex> g(lock); return statusText; }
void EnkiLibrary::setStatus(const juce::String& s) { const std::lock_guard<std::mutex> g(lock); statusText = s; }

void EnkiLibrary::import(const juce::Array<juce::File>& sources) {
 { const std::lock_guard<std::mutex> g(lock); Job j; j.kind = 0; j.sources = sources; jobs.push_back(j); }
 working = true;
 wake.signal();
}

void EnkiLibrary::setApiKey(const juce::String& k) {
 { const std::lock_guard<std::mutex> g(lock); key = k.trim(); }
 home().createDirectory();
 juce::XmlElement xml("ENKI");
 xml.setAttribute("freesoundKey", apiKey());
 xml.writeTo(settingsFile());
}
juce::String EnkiLibrary::apiKey() const { const std::lock_guard<std::mutex> g(lock); return key; }

void EnkiLibrary::search(const juce::String& query) {
 { const std::lock_guard<std::mutex> g(lock); Job j; j.kind = 1; j.query = query; jobs.push_back(j); }
 working = true;
 wake.signal();
}
std::vector<EnkiLibrary::Result> EnkiLibrary::results() const { const std::lock_guard<std::mutex> g(lock); return found; }
void EnkiLibrary::download(const Result& r) {
 { const std::lock_guard<std::mutex> g(lock); Job j; j.kind = 2; j.result = r; jobs.push_back(j); }
 working = true;
 wake.signal();
}

void EnkiLibrary::run() {
 while (!threadShouldExit()) {
  wake.wait(500);
  for (;;) {
   Job job;
   {
    const std::lock_guard<std::mutex> g(lock);
    if (jobs.empty()) break;
    job = jobs.front();
    jobs.erase(jobs.begin());
   }
   working = true;
   if (job.kind == 0) doImport(job.sources);
   else if (job.kind == 1) doSearch(job.query);
   else doDownload(job.result);
   if (threadShouldExit()) return;
  }
  working = false;
 }
}

// Copies every audio file into Sounds/<folder name>/..., keeping subfolders.
// Files already there with the same size are skipped; originals are never touched.
void EnkiLibrary::doImport(const juce::Array<juce::File>& sources) {
 const auto root = soundsFolder();
 root.createDirectory();
 int copied = 0, skipped = 0, failed = 0;
 for (const auto& src : sources) {
  if (threadShouldExit()) return;
  juce::Array<std::pair<juce::File, juce::File>> plan;
  if (src.isDirectory()) {
   const auto dest = root.getChildFile(src.getFileName());
   for (const auto& entry : juce::RangedDirectoryIterator(src, true, audioPattern, juce::File::findFiles)) {
    if (plan.size() >= maxImportFiles) break;
    plan.add({entry.getFile(), dest.getChildFile(entry.getFile().getRelativePathFrom(src))});
   }
  } else if (isAudio(src)) {
   plan.add({src, root.getChildFile("Imported").getChildFile(src.getFileName())});
  }
  for (int i = 0; i < plan.size(); ++i) {
   if (threadShouldExit()) return;
   const auto& [from, to] = plan.getReference(i);
   if (to.existsAsFile() && to.getSize() == from.getSize()) { ++skipped; continue; }
   to.getParentDirectory().createDirectory();
   if (from.copyFileTo(to)) ++copied; else ++failed;
   if ((copied + skipped) % 25 == 0)
    setStatus("IMPORTING " + src.getFileName().toUpperCase() + ": " + juce::String(i + 1) + " / " + juce::String(plan.size()));
  }
 }
 rescan();
 setStatus("IMPORTED " + juce::String(copied) + " FILES" + (skipped > 0 ? ", " + juce::String(skipped) + " ALREADY THERE" : juce::String())
           + (failed > 0 ? ", " + juce::String(failed) + " FAILED" : juce::String()));
}

void EnkiLibrary::doSearch(const juce::String& query) {
 const auto k = apiKey();
 if (k.isEmpty()) { setStatus("FREESOUND NEEDS YOUR FREE API KEY"); return; }
 setStatus("SEARCHING FREESOUND FOR \"" + query.toUpperCase() + "\"...");
 const auto url = juce::URL("https://freesound.org/apiv2/search/text/")
                      .withParameter("query", query)
                      .withParameter("filter", "license:\"Creative Commons 0\" duration:[0.1 TO 120]")
                      .withParameter("fields", "id,name,duration,username,license,previews")
                      .withParameter("page_size", "40")
                      .withParameter("token", k);
 std::unique_ptr<juce::InputStream> in(url.createInputStream(
     juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(15000)));
 if (!in) { setStatus("COULD NOT REACH FREESOUND (CHECK THE INTERNET AND THE KEY)"); return; }
 const auto json = juce::JSON::parse(in->readEntireStreamAsString());
 std::vector<Result> list;
 if (auto* array = json.getProperty("results", {}).getArray())
  for (const auto& r : *array) {
   Result x;
   x.id = static_cast<int>(r.getProperty("id", 0));
   x.name = r.getProperty("name", "").toString();
   x.user = r.getProperty("username", "").toString();
   x.license = r.getProperty("license", "").toString();
   x.seconds = static_cast<double>(r.getProperty("duration", 0.0));
   const auto previews = r.getProperty("previews", {});
   x.preview = previews.getProperty("preview-hq-ogg", previews.getProperty("preview-hq-mp3", "")).toString();
   if (x.id > 0 && x.preview.isNotEmpty()) list.push_back(x);
  }
 if (list.empty() && json.getProperty("detail", {}).toString().isNotEmpty()) {
  setStatus("FREESOUND: " + json.getProperty("detail", {}).toString().toUpperCase());
  return;
 }
 { const std::lock_guard<std::mutex> g(lock); found = std::move(list); }
 ++searchVersion;
 setStatus(juce::String(results().size()) + " FREE (CC0) SOUNDS FOR \"" + query.toUpperCase() + "\"");
}

void EnkiLibrary::doDownload(const Result& r) {
 const auto folder = soundsFolder().getChildFile("Freesound");
 folder.createDirectory();
 const auto ext = r.preview.fromLastOccurrenceOf(".", false, false).toLowerCase();
 const auto safe = juce::File::createLegalFileName(r.name.upToLastOccurrenceOf(".", false, false).isNotEmpty() ? r.name.upToLastOccurrenceOf(".", false, false) : r.name);
 const auto target = folder.getChildFile(juce::String(r.id) + " " + safe + "." + (ext == "mp3" ? "mp3" : "ogg"));
 if (!target.existsAsFile()) {
  setStatus("DOWNLOADING " + r.name.toUpperCase() + "...");
  std::unique_ptr<juce::InputStream> in(juce::URL(r.preview).createInputStream(
      juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(15000)));
  if (!in) { setStatus("DOWNLOAD FAILED"); return; }
  juce::TemporaryFile temp(target);
  {
   juce::FileOutputStream out(temp.getFile());
   if (!out.openedOk() || out.writeFromInputStream(*in, -1) <= 0) { setStatus("DOWNLOAD FAILED"); return; }
  }
  temp.overwriteTargetFileWithTemporary();
  // Credits: CC0 needs none, but it is good to know where a sound came from.
  folder.getChildFile("credits.txt").appendText(target.getFileName() + "  <-  freesound.org/s/" + juce::String(r.id) + "  by " + r.user + "  (" + r.license + ")\n");
 }
 rescan();
 setStatus("DOWNLOADED " + r.name.toUpperCase() + " INTO SOUNDS/FREESOUND");
}
