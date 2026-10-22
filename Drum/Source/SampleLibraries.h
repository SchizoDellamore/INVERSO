#pragma once
// INVERSO DRUM shared-filesystem sample-library browser.
// Catalogues user-authorised WAV/AIFF folders (including those also used by DAWs).
// It does NOT decode Steinberg .vstsound, Studio One .soundset, or Ableton devices.
#include <JuceHeader.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class InversoSampleLibraries final : public juce::Component,
                                    public juce::ListBoxModel,
                                    private juce::Thread {
public:
 using FileHandler=std::function<bool(int,const juce::File&)>;
 InversoSampleLibraries():juce::Thread("INVERSO DRUM library indexer"){
  setName("INVERSO SAMPLE LIBRARIES");
  setOpaque(true);
  title.setText("SAMPLE LIBRARIES  /  SHARED DAW FOLDERS",juce::dontSendNotification);
  title.setColour(juce::Label::textColourId,fg());
  title.setFont(juce::Font(juce::FontOptions(17.f,juce::Font::bold)));
  addAndMakeVisible(title);
  explanation.setText("Indexes WAV / AIFF in folders you choose; packaged VSTSound / SoundSet content is not accessible.",juce::dontSendNotification);
  explanation.setColour(juce::Label::textColourId,muted());
  addAndMakeVisible(explanation);
  source.addItem("All libraries",1);
  source.addItem("Cubase / MediaBay folders",2);
  source.addItem("Studio One / Files",3);
  source.addItem("Ableton / User Library",4);
  source.addItem("Shared / Other",5);
  source.setSelectedId(1,juce::dontSendNotification);
  source.onChange=[this]{refreshFilter();refreshRootChoices();};
  addAndMakeVisible(source);
  for(int i=0;i<16;++i)pad.addItem("PAD "+juce::String(i+1),i+1);
  pad.setSelectedId(1,juce::dontSendNotification);
  addAndMakeVisible(pad);
  search.setTextToShowWhenEmpty("Find kick, snare, 808, hat, ...",muted());
  search.onTextChange=[this]{refreshFilter();};
  addAndMakeVisible(search);
  sampleList.setModel(this);
  sampleList.setColour(juce::ListBox::backgroundColourId,juce::Colour::fromRGB(24,27,32));
  sampleList.setColour(juce::ListBox::outlineColourId,juce::Colour::fromRGB(75,80,84));
  sampleList.setRowHeight(30);
  addAndMakeVisible(sampleList);
  addButton.onClick=[this]{chooseFolder();};
  rescanButton.onClick=[this]{beginScan();};
  removeButton.onClick=[this]{removeSelectedRoot();};
  loadButton.onClick=[this]{loadSelected();};
  closeButton.onClick=[this]{setVisible(false);};
  for(auto* button:{&addButton,&rescanButton,&removeButton,&loadButton,&closeButton}){
   button->setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(53,56,62));
   button->setColour(juce::TextButton::textColourOffId,fg());
   addAndMakeVisible(*button);
  }
  loadButton.setColour(juce::TextButton::buttonColourId,orange());
  loadButton.setColour(juce::TextButton::textColourOffId,juce::Colours::black);
  rootChoice.onChange=[this]{removeButton.setEnabled(rootChoice.getNumItems()>0);};
  addAndMakeVisible(rootChoice);
  status.setColour(juce::Label::textColourId,muted());
  addAndMakeVisible(status);
  readRoots();
  refreshRootChoices();
  refreshFilter();
 }
 ~InversoSampleLibraries() override {
  signalThreadShouldExit();
  stopThread(5000);
  sampleList.setModel(nullptr);
 }
 void setLoadHandler(FileHandler handler){loadHandler=std::move(handler);}
 void setPad(int index){pad.setSelectedId(juce::jlimit(1,16,index+1),juce::dontSendNotification);}
 void open(){
  setVisible(true);
  toFront(true);
  beginScan();
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colour::fromRGB(17,20,24));
  g.setColour(orange());
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f),8.f,2.f);
  g.setColour(juce::Colour::fromRGB(37,42,48));
  g.fillRoundedRectangle(12.f,132.f,static_cast<float>(getWidth()-24),48.f,5.f);
 }
 void resized() override {
  auto w=getWidth(),h=getHeight();
  title.setBounds(22,14,w-230,30);
  closeButton.setBounds(w-114,16,88,27);
  explanation.setBounds(24,50,w-50,25);
  source.setBounds(22,92,255,31);
  pad.setBounds(287,92,121,31);
  search.setBounds(420,92,w-441,31);
  rootChoice.setBounds(22,142,w-395,29);
  addButton.setBounds(w-354,142,112,29);
  removeButton.setBounds(w-235,142,101,29);
  rescanButton.setBounds(w-126,142,100,29);
  sampleList.setBounds(22,191,w-44,h-258);
  status.setBounds(26,h-57,w-273,33);
  loadButton.setBounds(w-227,h-55,200,33);
 }
 int getNumRows() override {return static_cast<int>(filtered.size());}
 void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected) override {
  if(row<0||row>=getNumRows())return;
  const auto& sample=catalog[filtered[static_cast<size_t>(row)]];
  g.fillAll(selected?orange().withAlpha(.22f):
            ((row&1)?juce::Colour::fromRGB(28,32,37):juce::Colour::fromRGB(24,27,32)));
  g.setColour(selected?orange():fg());
  g.setFont(juce::Font(juce::FontOptions(12.5f,juce::Font::bold)));
  g.drawText(sample.file.getFileNameWithoutExtension(),8,1,width/2-8,height-2,juce::Justification::centredLeft,true);
  g.setColour(muted());g.setFont(juce::Font(juce::FontOptions(11.f)));
  g.drawText(sample.group+"  /  "+sample.relative,width/2,1,width/2-8,height-2,juce::Justification::centredLeft,true);
 }
 void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override {
  sampleList.selectRow(row);loadSelected();
 }
 void beginScan(){
  signalThreadShouldExit();
  stopThread(5000);
  // Starting scans only by explicit opening / user action; never from audio callback.
  if(roots.empty()){
   status.setText("Add a folder already used by your DAW. Proprietary packs are not scanned.",juce::dontSendNotification);
   catalog.clear();filtered.clear();sampleList.updateContent();return;
  }
  catalog.clear();filtered.clear();sampleList.updateContent();
  status.setText("Scanning user-selected libraries...",juce::dontSendNotification);
  startThread(juce::Thread::Priority::low);
 }
 void addFolder(const juce::String& group,const juce::File& folder){
  if(!folder.isDirectory())return;
  for(const auto& r:roots)if(r.group==group&&r.folder==folder)return;
  signalThreadShouldExit();stopThread(5000);
  roots.push_back({group,folder});
  writeRoots();refreshRootChoices();beginScan();
 }
private:
 struct Root {juce::String group;juce::File folder;};
 struct Entry {juce::String group,relative;juce::File file;};
 static juce::Colour orange(){return juce::Colour::fromRGB(245,132,42);}
 static juce::Colour fg(){return juce::Colour::fromRGB(235,228,215);}
 static juce::Colour muted(){return juce::Colour::fromRGB(169,177,179);}
 static juce::File rootsFile(){
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
    .getChildFile("INVERSO STUDIOS").getChildFile("DRUM").getChildFile("sample_libraries.txt");
 }
 static juce::String groupForId(int id){
  if(id==2)return "Cubase";
  if(id==3)return "Studio One";
  if(id==4)return "Ableton";
  return "Shared";
 }
 void readRoots(){
  const auto config=rootsFile();
  if(!config.existsAsFile())return;
  juce::StringArray lines;lines.addLines(config.loadFileAsString());
  for(const auto& line:lines){
   const int separator=line.indexOfChar('\t');
   if(separator<1)continue;
   const auto group=line.substring(0,separator);
   const juce::File folder(line.substring(separator+1));
   if(folder.isDirectory()&&roots.size()<48)
    roots.push_back({group,folder});
  }
 }
 void writeRoots(){
  const auto file=rootsFile();
  if(!file.getParentDirectory().createDirectory())return;
  juce::String text;
  for(const auto& r:roots)text+=r.group+"\t"+r.folder.getFullPathName()+"\n";
  file.replaceWithText(text);
 }
 void refreshRootChoices(){
  rootChoice.clear(juce::dontSendNotification);
  for(size_t i=0;i<roots.size();++i){
   rootChoice.addItem(roots[i].group+"  /  "+roots[i].folder.getFileName(),static_cast<int>(i+1));
  }
  if(!roots.empty())rootChoice.setSelectedId(1,juce::dontSendNotification);
  removeButton.setEnabled(!roots.empty());
 }
 void chooseFolder(){
  chooser=std::make_unique<juce::FileChooser>("Choose a sample folder already used by your DAW",
                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*");
  juce::Component::SafePointer<InversoSampleLibraries> safe(this);
  const auto group=groupForId(source.getSelectedId());
  chooser->launchAsync(juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectDirectories,
   [safe,group](const juce::FileChooser& c){
    if(safe==nullptr)return;
    const auto folder=c.getResult();
    if(folder.isDirectory())safe->addFolder(group,folder);
   });
 }
 void removeSelectedRoot(){
  const int i=rootChoice.getSelectedId()-1;
  if(i<0||i>=static_cast<int>(roots.size()))return;
  signalThreadShouldExit();stopThread(5000);
  roots.erase(roots.begin()+i);
  writeRoots();refreshRootChoices();beginScan();
 }
 void refreshFilter(){
  filtered.clear();
  const auto term=search.getText().trim().toLowerCase();
  const auto selectedGroup=source.getSelectedId();
  for(size_t i=0;i<catalog.size();++i){
   const auto& s=catalog[i];
   if(selectedGroup>1&&s.group!=groupForId(selectedGroup))continue;
   if(term.isNotEmpty() && !s.file.getFileName().containsIgnoreCase(term)
      && !s.relative.containsIgnoreCase(term))continue;
   filtered.push_back(i);
   if(filtered.size()>=6000)break; // UI stays responsive in large catalogues.
  }
  sampleList.updateContent();
  if(!filtered.empty())sampleList.selectRow(0);
  status.setText(juce::String(filtered.size())+" shown / "+juce::String(catalog.size())
                 +" indexed. Files only; packaged libraries are excluded.",juce::dontSendNotification);
 }
 void loadSelected(){
  const int i=sampleList.getSelectedRow();
  if(i<0||i>=static_cast<int>(filtered.size()))return;
  const auto file=catalog[filtered[static_cast<size_t>(i)]].file;
  if(!file.existsAsFile())return;
  if(loadHandler && loadHandler(pad.getSelectedId()-1,file)){
   status.setText("Loaded "+file.getFileName()+" into pad "+juce::String(pad.getSelectedId()),
                  juce::dontSendNotification);
  }else{
   status.setText("Cannot read file; verify it's a supported WAV or AIFF.",
                  juce::dontSendNotification);
  }
 }
 void run() override {
  std::vector<Entry> result;
  // Snapshot roots before the worker starts. The message thread stops and joins
  // this thread before it edits roots again.
  for(const auto& root:roots){
   if(threadShouldExit())return;
   juce::RangedDirectoryIterator scan(root.folder,true,"*.wav;*.wave;*.aif;*.aiff",
                                       juce::File::findFiles);
   for(const auto& item:scan){
    if(threadShouldExit())return;
    const auto file=item.getFile();
    if(!file.existsAsFile())continue;
    result.push_back({root.group,file.getRelativePathFrom(root.folder),file});
    if(result.size()>=50000)break;
   }
   if(result.size()>=50000)break;
  }
  if(threadShouldExit())return;
  std::sort(result.begin(),result.end(),[](const Entry& a,const Entry& b){
   return a.file.getFileName().compareIgnoreCase(b.file.getFileName())<0;
  });
  juce::Component::SafePointer<InversoSampleLibraries> safe(this);
  juce::MessageManager::callAsync([safe,found=std::move(result)]()mutable {
   if(safe==nullptr)return;
   safe->catalog=std::move(found);
   safe->refreshFilter();
  });
 }
 juce::Label title,explanation,status;
 juce::ComboBox source,pad,rootChoice;
 juce::TextEditor search;
 juce::ListBox sampleList;
 juce::TextButton addButton{"ADD FOLDER"},removeButton{"REMOVE"},rescanButton{"RESCAN"},
                  loadButton{"LOAD TO PAD"},closeButton{"CLOSE"};
 std::vector<Root> roots;
 std::vector<Entry> catalog;
 std::vector<size_t> filtered;
 std::unique_ptr<juce::FileChooser> chooser;
 FileHandler loadHandler;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InversoSampleLibraries)
};
