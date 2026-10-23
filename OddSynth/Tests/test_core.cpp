#include "../Source/OddCore.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
static int tests=0;
static void check(bool b,const char* m){++tests;if(!b){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
static double energy(odd::Engine& e,const odd::Settings& s,int frames){double out=0;for(int i=0;i<frames;++i){const auto a=e.tick(s);check(std::isfinite(a.l)&&std::isfinite(a.r),"finite audio");out+=a.l*a.l+a.r*a.r;}return out;}
int main(){
    odd::Settings s;for(auto& r:s.route)r={.35f,.2f,.65f,false};
    odd::Motion a,b;a.prepare(8000);b.prepare(8000);
    for(int i=0;i<500;++i){auto x=a.tick(s);for(auto v:x)check(v==.35f,"manual never moves base");}
    for(int mode:{1,2}){
        s.mode=mode;s.amount=1;s.rate=.9f;a.reset();b.reset();
        for(int i=0;i<40000;++i){auto x=a.tick(s),y=b.tick(s);check(x==y,"same seed and time reproduce trajectory");for(float v:x)check(v>=.2f&&v<=.65f,"autonomy obeys limits");}
        const auto frozen=a.current();s.frozen=true;for(int i=0;i<300;++i)check(a.tick(s)==frozen,"freeze holds motion exactly");s.frozen=false;
    }
    s.route[0].locked=true;s.route[0].base=.83f;for(int i=0;i<500;++i)check(a.tick(s)[0]==.83f,"lock preserves manual value even outside limits");
    s.amount=0;for(int i=0;i<500;++i)check(a.tick(s)[2]==s.route[2].base,"zero variation is manual");
    s.amount=1;s.route[1].low=.8f;s.route[1].high=.1f;for(int i=0;i<500;++i){const auto x=a.tick(s);check(x[1]>=.1f&&x[1]<=.8f,"crossed bounds remain safe");}
    odd::Engine engine;engine.prepare(48000);s=odd::Settings{};for(auto& r:s.route)r.base=.55f;
    for(size_t source=0;source<4;++source){engine.reset();s.level.fill(0);s.level[source]=1;engine.noteOn(48,.8f,1,s);check(energy(engine,s,12000)>.001,"each source independently produces audio");engine.noteOff(48,1);energy(engine,s,50000);check(engine.activeVoices()==0,"note off releases voice");}
    engine.reset();engine.noteOn(60,.5f,2,s);engine.pedal(2,true);engine.noteOff(60,2);energy(engine,s,48000);check(engine.activeVoices()==1,"sustain holds only the note channel");engine.pedal(2,false);energy(engine,s,48000);check(engine.activeVoices()==0,"pedal up releases held voice");
    odd::Engine e1,e2;e1.prepare(48000);e2.prepare(48000);s.mode=2;s.seed=823;s.amount=.7f;e1.noteOn(50,.8f,1,s);e2.noteOn(50,.8f,1,s);for(int i=0;i<24000;++i){auto x=e1.tick(s),y=e2.tick(s);check(x.l==y.l&&x.r==y.r,"repeated seeded performance reproduces audio");}
    odd::Sample sample;sample.sampleRate=48000;sample.left.resize(1000);sample.right.resize(1000);for(size_t i=0;i<1000;++i){sample.left[i]=static_cast<float>(i)/1000;sample.right[i]=-sample.left[i];}
    auto copy=sample;odd::edit::reverse(copy,.25f,.75f);check(copy.left[250]==sample.left[749]&&copy.left[0]==sample.left[0],"reverse affects selection only");odd::edit::reverse(copy,.25f,.75f);check(copy.left==sample.left,"reverse twice restores audio");
    copy=sample;odd::edit::crop(copy,.2f,.7f);check(copy.left.size()==500&&copy.right.size()==500&&copy.left.front()==sample.left[200],"crop keeps exact selected frames");
    copy=sample;odd::edit::erase(copy,.2f,.7f);check(copy.left.size()==500&&copy.left[200]==sample.left[700],"erase closes the selected gap");
    copy=sample;odd::edit::fade(copy,0,1,true);check(copy.left.front()==0&&copy.left.back()==sample.left.back(),"fade in endpoints");odd::edit::fade(copy,0,1,false);check(copy.left.back()==0,"fade out endpoint");
    copy=sample;odd::edit::speed(copy,0,1,.5);check(copy.left.size()==2000&&copy.right.size()==2000,"half speed doubles duration");odd::edit::speed(copy,0,1,2);check(copy.left.size()==1000,"double speed halves duration");
    s=odd::Settings{};s.level.fill(0);s.sampleMix=1;engine.prepare(48000);engine.setSample(&sample);engine.noteOn(60,.8f,1,s);check(energy(engine,s,6000)>1,"external sample is audible");s.granular=true;check(energy(engine,s,12000)>.1,"granular sample is audible");
    for(double sr:{8000.0,44100.0,96000.0,192000.0}){engine.prepare(sr);engine.setSample(&sample);s.mode=2;s.rate=1;s.amount=1;s.sampleStart=.99f;s.sampleEnd=.1f;s.reverse=true;for(int n=0;n<30;++n)engine.noteOn(n*4,.9f,1,s);energy(engine,s,10000);check(engine.activeVoices()<=odd::voiceCount,"polyphony remains bounded under stealing");}
    engine.stop();check(engine.activeVoices()==0,"panic stops all voices");
    std::printf("OddSynth: %d DSP/control checks passed\n",tests);
}
