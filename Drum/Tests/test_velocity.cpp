#include "../Source/DrumEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace inverso_drum;
static void check(bool okay,const char* msg){
 if(!okay){std::fprintf(stderr,"VELOCITY TEST FAILED: %s\n",msg);std::exit(1);}
}
static double renderedEnergy(Engine& e,int pad,float velocity,int sampleRate){
 e.prepare(sampleRate);
 e.trigger(pad,velocity);
 double sum=0.;
 for(int i=0;i<2048;++i){
  float left=0,right=0;
  e.renderFrame(left,right);
  check(std::isfinite(left)&&std::isfinite(right),"audio must remain finite");
  sum+=left*left+right*right;
 }
 return sum;
}
int main(){
 for(int sr:{44100,48000,96000}){
  Engine engine;engine.prepare(sr);
  // Every pattern, track and step has its own independent integer MIDI velocity.
  for(int pat=0;pat<patterns;++pat)
   for(int lane=0;lane<pads;++lane)
    for(int step=0;step<steps;++step)
     engine.set(pat,lane,step,(pat*17+lane*11+step*7)%128);
  for(int pat=0;pat<patterns;++pat)
   for(int lane=0;lane<pads;++lane)
    for(int step=0;step<steps;++step)
     check(engine.get(pat,lane,step)==(pat*17+lane*11+step*7)%128,
       "each unique step must retain its velocity");
  engine.set(7,15,127,0);check(engine.get(7,15,127)==0,"velocity 0 disables step");
  engine.set(7,15,127,127);check(engine.get(7,15,127)==127,"velocity 127 retained");
  // Velocity scales sample amplitude in the audio path, not just in MIDI export.
  engine.gain[0].store(.8f);
  const double soft=renderedEnergy(engine,0,.25f,sr);
  const double strong=renderedEnergy(engine,0,1.f,sr);
  check(soft>0.001&&strong>soft*12.0&&strong<soft*20.0,
        "velocity must affect rendered amplitude (near 4:1 amplitude)");
 }
 std::puts("INVERSO DRUM per-step velocity memory and audio response passed");
}
