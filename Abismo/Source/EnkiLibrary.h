#pragma once
// ENKI's own sound library: a folder (Documents/INVERSO ENKI/Sounds) that the
// user fills by importing (copying) files and folders from the computer, or by
// downloading free sounds from Freesound. The library is shared by every
// instance; copying and network work run on background threads.
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class EnkiLibrary final : private juce::Thread {
public:
 static EnkiLibrary& instance();
 ~EnkiLibrary() override;

 static juce::File home();        // Documents/INVERSO ENKI
 static juce::File soundsFolder(); // .../Sounds
 static juce::File favoritesFolder();
 static bool isAudio(const juce::File&);

 // ---- the index (message thread)
 void rescan();
 int size() const;
 juce::File file(int index) const;
 // Top-level folders inside Sounds (each import becomes one).
 juce::StringArray folders() const;
 // Files inside a top-level folder ("" = everything).
 std::vector<int> filesIn(const juce::String& folder) const;
 juce::File randomFile(const juce::String& folder, juce::Random&) const;
 int version() const { return indexVersion.load(); }

 // ---- importing: copies files and whole folders into Sounds, keeping their structure.
 void import(const juce::Array<juce::File>& sources);
 bool busy() const { return working.load(); }
 juce::String status() const;

 // ---- Freesound (needs a free API key from freesound.org/apiv2/apply)
 struct Result { int id = 0; juce::String name, user, license, preview; double seconds = 0.0; };
 void setApiKey(const juce::String&);
 juce::String apiKey() const;
 void search(const juce::String& query);            // CC0 sounds only
 std::vector<Result> results() const;
 int resultsVersion() const { return searchVersion.load(); }
 void download(const Result&);                      // to Sounds/Freesound, then rescan

private:
 EnkiLibrary();
 void run() override;
 struct Job { int kind = 0; juce::Array<juce::File> sources; juce::String query; Result result; };
 void doImport(const juce::Array<juce::File>&);
 void doSearch(const juce::String&);
 void doDownload(const Result&);
 void setStatus(const juce::String&);
 juce::File settingsFile() const;

 mutable std::mutex lock;
 std::vector<juce::File> files;
 std::vector<Job> jobs;
 std::vector<Result> found;
 juce::String statusText, key;
 std::atomic<bool> working{false};
 std::atomic<int> indexVersion{0}, searchVersion{0};
 juce::WaitableEvent wake;
};
