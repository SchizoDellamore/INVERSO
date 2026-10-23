#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<juce::String,odd::dimensions> OddProcessor::ids{{"rough","unstable","density","bright","vowel","pulseRate"}};
const std::array<juce::String,odd::dimensions> OddProcessor::names{{"Atrito","Instabilidade","Densidade","Brilho","Garganta","Pulso"}};
using Layout=juce::AudioProcessorValueTreeState::ParameterLayout;
Layout OddProcessor::layout(){
    Layout l;
    auto f=[&](juce::String id,juce::String name,float lo,float hi,float def,float skew=1.0f){
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(lo,hi,0.0f,skew),def));
    };
    auto b=[&](juce::String id,juce::String name,bool def){l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},name,def));};
    const std::array<float,6> defaults{{.25f,.1f,.35f,.4f,.3f,.25f}};
    for(size_t i=0;i<ids.size();++i){f(ids[i],names[i],0,1,defaults[i]);f(ids[i]+"Low",names[i]+" minimo",0,1,0);f(ids[i]+"High",names[i]+" maximo",0,1,1);b(ids[i]+"Lock",names[i]+" travado",false);}
    for(const auto& id: {"material","throat","swarm","pulse"})f(id,id,0,1,juce::String(id)=="material"?.65f:0.0f);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Autonomia",juce::StringArray{"Manual","Dentro dos limites","Livre"},0));
    f("amount","Quantidade de variacao",0,1,.3f);f("rate","Velocidade da variacao",0,1,.15f);
    b("freeze","Congelar transformacao",false);b("repeat","Reiniciar ao tocar DAW",true);
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"seed",1},"Semente",1,999999,1975));
    f("tension","Tensao",0,1,0);f("attack","Ataque",.002f,10,.08f,.35f);f("release","Release",.01f,15,.8f,.35f);
    f("gain","Saida dB",-60,0,-18);
    f("sampleMix","Sample",0,1,0);f("sampleStart","Inicio do trecho",0,1,0);f("sampleEnd","Fim do trecho",0,1,1);
    f("sampleSpeed","Velocidade do sample",.125f,4,1,.5f);f("grain","Tamanho do grao s",.015f,.5f,.14f,.5f);
    b("granular","Granular",false);b("sampleLoop","Loop",true);b("sampleReverse","Reproducao reversa",false);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"sampleRoute",1},"Destino do sample",juce::StringArray{"Camada","Materia","Garganta","Enxame","Pulso"},0));
    f("reverb","Reverb",0,1,0);f("delay","Delay",0,1,0);f("drive","Distorcao",0,1,0);
    f("delayTime","Tempo do delay s",.05f,1.5f,.37f,.5f);
    return l;
}
OddProcessor::OddProcessor():AudioProcessor(BusesProperties().withOutput("Saida",juce::AudioChannelSet::stereo(),true)),state(*this,&undoManager,"OddSynth",layout()){}
bool OddProcessor::isBusesLayoutSupported(const BusesLayout& b)const{return b.getMainInputChannelSet().isDisabled()&&(b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()||b.getMainOutputChannelSet()==juce::AudioChannelSet::mono());}
float OddProcessor::value(const juce::String& id)const{if(auto* p=state.getRawParameterValue(id))return p->load();return 0;}
void OddProcessor::setValue(const juce::String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();state.getParameterAsValue(id).setValue(p->convertFrom0to1(p->convertTo0to1(v)));p->endChangeGesture();}}
odd::Settings OddProcessor::settings()const{
    odd::Settings s;
    for(size_t i=0;i<ids.size();++i)s.route[i]={value(ids[i]),value(ids[i]+"Low"),value(ids[i]+"High"),value(ids[i]+"Lock")>.5f};
    s.level={{value("material"),value("throat"),value("swarm"),value("pulse")}};
    s.mode=static_cast<int>(value("mode"));s.seed=static_cast<uint32_t>(value("seed"));s.amount=value("amount");s.rate=value("rate");
    s.frozen=value("freeze")>.5f;s.tension=std::max(value("tension"),wheel);s.attack=value("attack");s.release=value("release");
    s.gain=juce::Decibels::decibelsToGain(value("gain"))*expression;
    s.sampleMix=value("sampleMix");s.sampleStart=value("sampleStart");s.sampleEnd=value("sampleEnd");s.sampleSpeed=value("sampleSpeed");
    s.grainSize=value("grain");s.granular=value("granular")>.5f;s.sampleLoop=value("sampleLoop")>.5f;s.reverse=value("sampleReverse")>.5f;
    s.sampleRoute=static_cast<int>(value("sampleRoute"));
    return s;
}
void OddProcessor::prepareToPlay(double sampleRate,int block){
    sr=sampleRate;engine.prepare(sr);engine.setSample(currentSample.get());reverb.setSampleRate(sr);reverb.reset();
    delayL.assign(static_cast<size_t>(sr*2)+1,0);delayR=delayL;delayWrite=0;
    effectBuffer.setSize(2,std::max(1,block));
    capture=std::make_shared<CaptureBuffer>(sr);
    wasPlaying=false;wheel=0;expression=1;dcIn.fill(0);dcOut.fill(0);smoothLevels.fill(0);
}
void OddProcessor::midi(const juce::MidiMessage& m,const odd::Settings& s){
    if(m.isNoteOn())engine.noteOn(m.getNoteNumber(),m.getFloatVelocity(),m.getChannel(),s);
    else if(m.isNoteOff())engine.noteOff(m.getNoteNumber(),m.getChannel());
    else if(m.isPitchWheel())engine.pitchBend(m.getChannel(),(m.getPitchWheelValue()-8192)/8192.0f*2.0f);
    else if(m.isAllNotesOff()||m.isAllSoundOff())engine.stop();
    else if(m.isController()){
        if(m.getControllerNumber()==1)wheel=m.getControllerValue()/127.0f;
        if(m.getControllerNumber()==11)expression=m.getControllerValue()/127.0f;
        if(m.getControllerNumber()==64)engine.pedal(m.getChannel(),m.getControllerValue()>=64);
    }
}
void OddProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& events){
    juce::ScopedNoDenormals noDenormals;buffer.clear();
    keyboard.processNextMidiBuffer(events,0,buffer.getNumSamples(),true);
    bool playing=false;if(auto* p=getPlayHead())if(auto pos=p->getPosition())playing=pos->getIsPlaying();
    if((playing&&!wasPlaying&&value("repeat")>.5f)||restartRequest.exchange(false))engine.restartMotion();
    if(motionRestoreRequest.exchange(false))engine.restoreMotion(pendingMotion);
    wasPlaying=playing;
    if(panicRequest.exchange(false)){engine.stop();keyboard.allNotesOff(0);reverb.reset();std::fill(delayL.begin(),delayL.end(),0);std::fill(delayR.begin(),delayR.end(),0);}
    auto s=settings();const float smooth=static_cast<float>(1-std::exp(-1.0/(sr*.015)));
    auto it=events.cbegin();
    const int channels=buffer.getNumChannels();
    for(int n=0;n<buffer.getNumSamples();++n){
        while(it!=events.cend()&&(*it).samplePosition<=n){midi((*it).getMessage(),s);++it;s.tension=std::max(value("tension"),wheel);s.gain=juce::Decibels::decibelsToGain(value("gain"))*expression;}
        for(size_t k=0;k<4;++k){smoothLevels[k]+=smooth*(s.level[k]-smoothLevels[k]);}
        const auto target=s.level;s.level={{smoothLevels[0],smoothLevels[1],smoothLevels[2],smoothLevels[3]}};
        auto o=engine.tick(s);s.level=target;
        // Remove DC; transparent peak protection only at the output ceiling.
        for(int c=0;c<2;++c){const float input=c==0?o.l:o.r;const float x=input-dcIn[static_cast<size_t>(c)]+.9993f*dcOut[static_cast<size_t>(c)];dcIn[static_cast<size_t>(c)]=input;dcOut[static_cast<size_t>(c)]=x;if(c==0)o.l=x;else o.r=x;}
        if(channels==1)buffer.setSample(0,n,(o.l+o.r)*.5f);else{buffer.setSample(0,n,o.l);buffer.setSample(1,n,o.r);}
    }
    const float drive=value("drive"),delay=value("delay"),wet=value("reverb");
    if(drive>0)for(int c=0;c<channels;++c)for(int n=0;n<buffer.getNumSamples();++n){const float x=buffer.getSample(c,n);buffer.setSample(c,n,(1-drive)*x+drive*std::tanh(x*(1+drive*12))/(1+drive*2));}
    if(!delayL.empty()){
        const auto offset=static_cast<size_t>(std::clamp(value("delayTime")*static_cast<float>(sr),1.0f,static_cast<float>(delayL.size()-1)));
        for(int n=0;n<buffer.getNumSamples();++n){const auto at=(delayWrite+delayL.size()-offset)%delayL.size();const float l=buffer.getSample(0,n),r=buffer.getSample(channels>1?1:0,n);const float dl=delayL[at],dr=delayR[at];delayL[delayWrite]=l+dl*.42f;delayR[delayWrite]=r+dr*.42f;delayWrite=(delayWrite+1)%delayL.size();if(delay>0){buffer.setSample(0,n,l+dl*delay*.65f);if(channels>1)buffer.setSample(1,n,r+dr*delay*.65f);}}
    }
    if(wet>0){juce::Reverb::Parameters p;p.roomSize=.83f;p.damping=.35f;p.wetLevel=wet*.48f;p.dryLevel=1;p.width=1;reverb.setParameters(p);if(channels>1)reverb.processStereo(buffer.getWritePointer(0),buffer.getWritePointer(1),buffer.getNumSamples());else reverb.processMono(buffer.getWritePointer(0),buffer.getNumSamples());}
    else reverb.reset();
    float maximum=0;auto written=capture?capture->written.load(std::memory_order_relaxed):0;
    for(int n=0;n<buffer.getNumSamples();++n){float l=0,r=0;for(int c=0;c<channels;++c){const float raw=buffer.getSample(c,n);const float x=std::isfinite(raw)?std::clamp(raw,-.98f,.98f):0;buffer.setSample(c,n,x);maximum=std::max(maximum,std::abs(x));if(c==0)l=x;else if(c==1)r=x;}if(channels==1)r=l;
        if(capture&&capture->frames>0){const auto at=static_cast<size_t>(written%capture->frames);capture->audio[at*2].store(l,std::memory_order_relaxed);capture->audio[at*2+1].store(r,std::memory_order_relaxed);++written;}
    }
    if(capture)capture->written.store(written,std::memory_order_release);peak.store(maximum);voices.store(engine.activeVoices());
    const auto& actual=engine.values();for(size_t i=0;i<actual.size();++i)live[i].store(actual[i]);events.clear();
}
void OddProcessor::restart(){restartRequest=true;}
void OddProcessor::panic(){panicRequest=true;}
void OddProcessor::randomise(){
    state.copyState();
    undoManager.beginNewTransaction("Variar timbre");
    const uint32_t seed=static_cast<uint32_t>(value("seed"));
    for(size_t i=0;i<ids.size();++i)if(value(ids[i]+"Lock")<.5f){float lo=value(ids[i]+"Low"),hi=value(ids[i]+"High");if(lo>hi)std::swap(lo,hi);const float base=std::clamp(value(ids[i]),lo,hi);setValue(ids[i],base+value("amount")*(lo+(hi-lo)*odd::randomAt(seed,1,static_cast<int>(i))-base));}
    setValue("seed",static_cast<float>(1+odd::hash(seed)%999999u));
}
class OddProcessor::SampleAction final:public juce::UndoableAction{
public:
    SampleAction(OddProcessor& p,std::shared_ptr<const odd::Sample> next,juce::String name):owner(p),before(p.sampleCopy()),after(std::move(next)),beforeName(p.sampleName()),afterName(std::move(name)){}
    bool perform()override{owner.swapSample(after,afterName);return true;}
    bool undo()override{owner.swapSample(before,beforeName);return true;}
    int getSizeInUnits()override{return 4000;}
private:OddProcessor& owner;std::shared_ptr<const odd::Sample>before,after;juce::String beforeName,afterName;
};
void OddProcessor::swapSample(std::shared_ptr<const odd::Sample> s,const juce::String& name){
    const juce::ScopedLock lock(getCallbackLock());currentSample=std::move(s);currentSampleName=name;engine.setSample(currentSample.get());
}
std::shared_ptr<const odd::Sample> OddProcessor::sampleCopy()const{const juce::ScopedLock lock(getCallbackLock());return currentSample;}
juce::String OddProcessor::sampleName()const{const juce::ScopedLock lock(getCallbackLock());return currentSampleName;}
bool OddProcessor::loadSample(const juce::File& file,juce::String& error){
    juce::AudioFormatManager formats;formats.registerBasicFormats();auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if(!reader){error="Formato de audio nao reconhecido.";return false;}
    if(reader->lengthInSamples<2||reader->sampleRate<=0){error="Arquivo sem audio valido.";return false;}
    if(reader->lengthInSamples>static_cast<juce::int64>(reader->sampleRate*120)||reader->lengthInSamples>8000000){error="Use um trecho de ate 120 segundos (maximo de 8 milhoes de amostras por canal).";return false;}
    const int count=static_cast<int>(reader->lengthInSamples);juce::AudioBuffer<float> audio(2,count);
    if(!reader->read(&audio,0,count,0,true,true)){error="Nao foi possivel ler o audio.";return false;}
    auto s=std::make_shared<odd::Sample>();s->sampleRate=reader->sampleRate;s->left.assign(audio.getReadPointer(0),audio.getReadPointer(0)+count);s->right.assign(audio.getReadPointer(reader->numChannels>1?1:0),audio.getReadPointer(reader->numChannels>1?1:0)+count);
    for(auto* d:{&s->left,&s->right})for(auto& x:*d)if(!std::isfinite(x))x=0;
    undoManager.beginNewTransaction("Carregar sample");undoManager.perform(new SampleAction(*this,s,file.getFileName()));setValue("sampleStart",0);setValue("sampleEnd",1);setValue("sampleMix",.7f);return true;
}
bool OddProcessor::sampleEdit(int action){
    auto original=sampleCopy();if(!original)return false;auto next=std::make_shared<odd::Sample>(*original);
    const float a=value("sampleStart"),b=value("sampleEnd");const auto range=odd::edit::range(*next,a,b);if(range.second<=range.first)return false;
    if(action==0)odd::edit::reverse(*next,a,b);if(action==1)odd::edit::crop(*next,a,b);if(action==2)odd::edit::erase(*next,a,b);
    if(action==3)odd::edit::fade(*next,a,b,true);if(action==4)odd::edit::fade(*next,a,b,false);
    if(action==5){if(next->left.size()+range.second-range.first>8000000)return false;odd::edit::speed(*next,a,b,.5);}if(action==6)odd::edit::speed(*next,a,b,2);
    const auto name=sampleName();undoManager.beginNewTransaction("Editar sample");undoManager.perform(new SampleAction(*this,next->left.empty()?nullptr:next,next->left.empty()?juce::String{}:name));if(action==1||action==2||action==5||action==6){setValue("sampleStart",0);setValue("sampleEnd",1);}return true;
}
static bool writeWav(const juce::File& file,const juce::AudioBuffer<float>& audio,double rate){
    juce::TemporaryFile temp(file);auto stream=temp.getFile().createOutputStream();if(!stream)return false;
    juce::WavAudioFormat wav;auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),rate,static_cast<unsigned int>(audio.getNumChannels()),24,{},0));
    if(!writer)return false;stream.release();if(!writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()))return false;writer.reset();return temp.overwriteTargetFileWithTemporary();
}
bool OddProcessor::exportSample(const juce::File& file){auto s=sampleCopy();if(!s||s->left.empty())return false;juce::AudioBuffer<float> b(2,static_cast<int>(s->left.size()));b.copyFrom(0,0,s->left.data(),b.getNumSamples());b.copyFrom(1,0,s->right.empty()?s->left.data():s->right.data(),b.getNumSamples());return writeWav(file,b,s->sampleRate);}
bool OddProcessor::exportCapture(const juce::File& file){
    // Read atomics, leaving ten seconds of overwrite headroom for the copy.
    std::shared_ptr<CaptureBuffer> source;{const juce::ScopedLock lock(getCallbackLock());source=capture;}
    if(!source||!source->frames)return false;
    const auto end=source->written.load(std::memory_order_acquire);const auto count=static_cast<size_t>(std::min<uint64_t>(end,static_cast<uint64_t>(source->sampleRate*15)));if(!count)return false;
    juce::AudioBuffer<float> b(2,static_cast<int>(count));for(size_t i=0;i<count;++i){const auto at=static_cast<size_t>((end-count+i)%source->frames);b.setSample(0,static_cast<int>(i),source->audio[at*2].load(std::memory_order_relaxed));b.setSample(1,static_cast<int>(i),source->audio[at*2+1].load(std::memory_order_relaxed));}
    if(source->written.load(std::memory_order_acquire)-end>source->frames-count)return false;
    return writeWav(file,b,source->sampleRate);
}
juce::String OddProcessor::encodeSample(const odd::Sample& s){
    juce::MemoryBlock data;{juce::MemoryOutputStream out(data,false);out.writeDouble(s.sampleRate);out.writeInt(static_cast<int>(s.left.size()));for(float x:s.left)out.writeFloat(x);for(size_t i=0;i<s.left.size();++i)out.writeFloat(s.right.empty()?s.left[i]:s.right[i]);}
    juce::MemoryBlock compressed;{juce::MemoryOutputStream out(compressed,false);juce::GZIPCompressorOutputStream zip(out,6);zip.write(data.getData(),data.getSize());}return compressed.toBase64Encoding();
}
std::shared_ptr<const odd::Sample> OddProcessor::decodeSample(const juce::String& text){
    if(text.isEmpty())return {};if(text.length()>100000000)return {};juce::MemoryBlock compressed;if(!compressed.fromBase64Encoding(text))return {};
    juce::MemoryInputStream input(compressed,false);juce::GZIPDecompressorInputStream zip(input);const double rate=zip.readDouble();const int n=zip.readInt();
    if(rate<8000||rate>384000||!std::isfinite(rate)||n<2||n>8000000)return {};
    auto s=std::make_shared<odd::Sample>();s->sampleRate=rate;s->left.resize(static_cast<size_t>(n));s->right.resize(static_cast<size_t>(n));
    for(auto* d:{&s->left,&s->right})for(auto& x:*d){if(zip.isExhausted())return {};x=zip.readFloat();if(!std::isfinite(x))x=0;}return s;
}
juce::ValueTree OddProcessor::makeState(bool withSlots){
    auto tree=state.copyState();tree.setProperty("formatVersion",1,nullptr);tree.setProperty("sampleName",sampleName(),nullptr);if(auto s=sampleCopy())tree.setProperty("sampleData",encodeSample(*s),nullptr);
    odd::MotionState movement;{const juce::ScopedLock lock(getCallbackLock());movement=motionRestoreRequest.load()?pendingMotion:engine.motionSnapshot();}
    tree.setProperty("motionSeconds",movement.seconds,nullptr);tree.setProperty("motionValid",movement.valid,nullptr);for(size_t i=0;i<movement.last.size();++i)tree.setProperty("motion"+juce::String(static_cast<int>(i)),movement.last[i],nullptr);
    if(withSlots){if(slotA.isValid()){auto a=juce::ValueTree("SlotA");a.addChild(slotA.createCopy(),-1,nullptr);tree.addChild(a,-1,nullptr);}if(slotB.isValid()){auto b=juce::ValueTree("SlotB");b.addChild(slotB.createCopy(),-1,nullptr);tree.addChild(b,-1,nullptr);}}return tree;
}
bool OddProcessor::applyState(const juce::ValueTree& t,bool withSlots){
    if(!t.isValid()||!t.hasType("OddSynth"))return false;
    const auto data=t.getProperty("sampleData").toString();auto sample=decodeSample(data);if(data.isNotEmpty()&&!sample)return false;
    auto clean=t.createCopy();clean.removeProperty("sampleData",nullptr);clean.removeProperty("sampleName",nullptr);clean.removeChild(clean.getChildWithName("SlotA"),nullptr);clean.removeChild(clean.getChildWithName("SlotB"),nullptr);
    state.replaceState(clean);swapSample(sample,t.getProperty("sampleName").toString());
    {const juce::ScopedLock lock(getCallbackLock());pendingMotion.seconds=static_cast<double>(t.getProperty("motionSeconds",0));pendingMotion.valid=static_cast<bool>(t.getProperty("motionValid",false));for(size_t i=0;i<pendingMotion.last.size();++i)pendingMotion.last[i]=static_cast<float>(t.getProperty("motion"+juce::String(static_cast<int>(i)),0));motionRestoreRequest=true;}
    if(withSlots){slotA=t.getChildWithName("SlotA").getChild(0).createCopy();slotB=t.getChildWithName("SlotB").getChild(0).createCopy();undoManager.clearUndoHistory();}
    return true;
}
void OddProcessor::getStateInformation(juce::MemoryBlock& out){auto xml=makeState(true).createXml();copyXmlToBinary(*xml,out);}
void OddProcessor::setStateInformation(const void* bytes,int size){if(auto xml=getXmlFromBinary(bytes,size))applyState(juce::ValueTree::fromXml(*xml),true);}
void OddProcessor::captureSlot(int slot){if(slot==0)slotA=makeState(false);else slotB=makeState(false);}
bool OddProcessor::hasSlot(int slot)const{return (slot==0?slotA:slotB).isValid();}
bool OddProcessor::recallSlot(int slot){
    const auto t=(slot==0?slotA:slotB).createCopy();if(!t.isValid())return false;
    auto sample=decodeSample(t.getProperty("sampleData").toString());
    undoManager.beginNewTransaction(slot==0?"Recuperar A":"Recuperar B");
    undoManager.perform(new SampleAction(*this,sample,t.getProperty("sampleName").toString()));
    for(int i=0;i<t.getNumChildren();++i){auto c=t.getChild(i);const auto id=c.getProperty("id").toString();if(state.getParameter(id))setValue(id,static_cast<float>(c.getProperty("value")));}
    {const juce::ScopedLock lock(getCallbackLock());pendingMotion.seconds=static_cast<double>(t.getProperty("motionSeconds",0));pendingMotion.valid=static_cast<bool>(t.getProperty("motionValid",false));for(size_t i=0;i<pendingMotion.last.size();++i)pendingMotion.last[i]=static_cast<float>(t.getProperty("motion"+juce::String(static_cast<int>(i)),0));motionRestoreRequest=true;}
    restart();return true;
}
bool OddProcessor::morph(float mix){
    if(!slotA.isValid()||!slotB.isValid())return false;
    auto find=[](const juce::ValueTree& t,const juce::String& id){for(int i=0;i<t.getNumChildren();++i){auto c=t.getChild(i);if(c.getProperty("id").toString()==id)return static_cast<float>(c.getProperty("value"));}return 0.0f;};
    for(size_t i=0;i<ids.size();++i)if(value(ids[i]+"Lock")<.5f){const float a=find(slotA,ids[i]),b=find(slotB,ids[i]);setValue(ids[i],a+odd::unit(mix)*(b-a));}
    for(const auto* id:{"material","throat","swarm","pulse","sampleMix"}){const float a=find(slotA,id),b=find(slotB,id);setValue(id,a+odd::unit(mix)*(b-a));}return true;
}
bool OddProcessor::savePreset(const juce::File& file){juce::MemoryBlock bytes;getStateInformation(bytes);juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithData(bytes.getData(),bytes.getSize()))return false;return temp.overwriteTargetFileWithTemporary();}
bool OddProcessor::loadPreset(const juce::File& file){juce::MemoryBlock bytes;if(file.getSize()>150000000||!file.loadFileAsData(bytes))return false;if(auto xml=getXmlFromBinary(bytes.getData(),static_cast<int>(bytes.getSize())))return applyState(juce::ValueTree::fromXml(*xml),true);return false;}
void OddProcessor::factoryPreset(int index){
    undoManager.beginNewTransaction("Preset");
    const std::array<std::array<float,10>,6> preset{{
        {{.25f,.1f,.35f,.4f,.3f,.25f,.65f,0,0,0}},
        {{.78f,.31f,.3f,.65f,.2f,.18f,.8f,0,.2f,0}},
        {{.38f,.52f,.4f,.25f,.7f,.15f,.12f,.85f,.15f,0}},
        {{.3f,.72f,.93f,.33f,.4f,.2f,.08f,.18f,.85f,0}},
        {{.58f,.3f,.25f,.45f,.55f,.38f,.15f,.2f,.1f,.85f}},
        {{.73f,.8f,.75f,.32f,.65f,.61f,.45f,.4f,.45f,.3f}}
    }};
    const auto& p=preset[static_cast<size_t>(juce::jlimit(0,5,index))];for(size_t i=0;i<ids.size();++i){setValue(ids[i],p[i]);setValue(ids[i]+"Low",0);setValue(ids[i]+"High",1);setValue(ids[i]+"Lock",0);}
    const std::array<juce::String,4> sources{{"material","throat","swarm","pulse"}};for(size_t i=0;i<sources.size();++i)setValue(sources[i],p[i+6]);
    setValue("mode",0);setValue("freeze",0);setValue("tension",0);setValue("reverb",0);setValue("delay",0);setValue("drive",0);setValue("sampleMix",0);setValue("gain",-18);restart();
}
juce::AudioProcessorEditor* OddProcessor::createEditor(){return new OddEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new OddProcessor();}
