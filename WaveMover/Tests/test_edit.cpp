#include "../Source/AudioEdit.h"
#include <cstdio>
#include <cstdlib>
using namespace wavemover;
static int checks=0;
static void check(bool v,const char*m){++checks;if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
static double energy(const Clip&a,std::size_t first=0){double e=0;for(const auto&c:a.data)for(auto j=first;j<c.size();++j)e+=c[j]*c[j];return e;}
static double frequency(const Clip&a){const auto first=static_cast<std::size_t>(a.rate*.15),last=a.frames()-first;int crosses=0;for(auto i=first+1;i<last;++i)if(a.data[0][i-1]<=0&&a.data[0][i]>0)++crosses;return crosses*a.rate/static_cast<double>(last-first);}
int main(){
 for(double sr:{44100.,48000.,96000.})for(int channels:{1,2}){
  Clip a(channels,static_cast<std::size_t>(sr),sr);for(int c=0;c<channels;++c)for(std::size_t j=0;j<a.frames();++j)a.data[c][j]=static_cast<float>(.4*std::sin(2*pi*440*static_cast<double>(j)/sr))*(c?-.5f:1.f);
  const auto original=a.data;
  auto reversed=render(a,Op::Reverse,{}).audio;auto twice=render(reversed,Op::Reverse,{}).audio;check(twice.data==a.data,"reverse twice must be bit-identical");
  auto selection=section(a,{100,1000});check(selection.frames()==900,"selection length");auto sp=splice(a,{100,1000},selection);check(sp.data==a.data,"unchanged splice bit-identical");
  auto smaller=splice(a,{100,1000},Clip(channels,0,sr));check(smaller.frames()==a.frames()-900,"cut length");check(smaller.data[0][100]==a.data[0][1000],"cut does not duplicate boundary");
  auto fade=render(a,Op::FadeBoth,{20,1,1}).audio;check(fade.data[0].front()==0&&fade.data[0].back()==0,"fade endpoints");check(fade.data[0][a.frames()/2]==a.data[0][a.frames()/2],"fade leaves middle intact");
  auto stretched=render(a,Op::Stretch,{150,0,1}).audio;check(stretched.frames()==static_cast<std::size_t>(std::llround(sr*1.5)),"stretch exact output length");check(std::abs(frequency(stretched)-440)<4,"stretch preserves pitch");
  auto pitched=render(a,Op::Pitch,{12,0,1}).audio;check(pitched.frames()==a.frames(),"pitch preserves duration");check(std::abs(frequency(pitched)-880)<8,"pitch raises one octave");
  auto bpmEdit=render(a,Op::Tempo,{120,90,1}).audio;check(bpmEdit.frames()==static_cast<std::size_t>(std::llround(sr*120/90)),"fixed BPM exact duration");check(std::abs(frequency(bpmEdit)-440)<4,"fixed BPM preserves pitch");
  auto ramp=render(a,Op::TempoRamp,{120,60,1}).audio;check(std::abs(static_cast<double>(ramp.frames())-sr*2*std::log(2.))<10,"tempo ramp integral duration");check(std::abs(frequency(ramp)-440)<6,"tempo ramp preserves pitch");
  auto stop=render(a,Op::TapeStop,{}).audio;check(stop.frames()==a.frames()*2,"tape stop finite exact length");check(stop.data[0].back()==0,"tape stop ends silent");
  check(render(a,Op::Repeat,{3,0,1}).audio.frames()==a.frames()*3,"repeat length");check(render(a,Op::Ratchet,{8,0,1}).audio.frames()==a.frames(),"ratchet preserves length");
  const auto markers=grid(a,120,4);check(markers.size()==7,"grid subdivision count at 120 BPM");
  auto sh=render(a,Op::Shuffle,{0,0,16},markers);check(sh.audio.data==render(a,Op::Shuffle,{0,0,16},markers).audio.data,"seeded shuffle reproducible");check(sh.audio.frames()==a.frames(),"shuffle duration");
  auto cross=crossfadeAt(a,a.frames()/2,10);check(cross.frames()==a.frames()-static_cast<std::size_t>(sr*.01),"crossfade overlap duration");
  auto frozen=render(selection,Op::Freeze,{.1,40,1984}).audio;check(frozen.frames()==static_cast<std::size_t>(sr*.1)&&energy(frozen)>.01,"granular freeze audible");
  check(a.data==original,"all edits preserve source");
 }
 Clip impulse(2,4800,48000);impulse.data[0][0]=1;impulse.data[1][0]=1;
 auto tail=render(impulse,Op::Tail,{1,100,0}).audio;check(tail.frames()>impulse.frames(),"reverb tail extends output");check(energy(tail,impulse.frames())>.001,"tail remains audible after source");
 auto mixed=render(impulse,Op::Reverb,{1,35,0}).audio;check(mixed.data[0][0]==1,"reverb retains dry attack");
 auto echo=render(impulse,Op::Delay,{100,50,0}).audio;check(std::abs(echo.data[0][4800]-.5f)<1e-6,"delay first repeat");check(std::abs(echo.data[0][9600]-.25f)<1e-6,"delay feedback");
 for(auto op:{Op::Reverse,Op::FadeBoth,Op::Stretch,Op::TapeStop,Op::Freeze}){Clip tiny(1,1,48000);tiny.data[0][0]=.5;auto out=render(tiny,op,op==Op::Stretch?Parameters{150,1,1}:Parameters{.05,10,1});for(auto v:out.audio.data[0])check(std::isfinite(v),"one-sample edit stays finite");}
 Clip attacks(1,48000,48000);for(auto i:{5000,15000,30000})for(int j=0;j<300;++j)attacks.data[0][static_cast<std::size_t>(i+j)]=.8f;check(transients(attacks,.5).size()==3,"three isolated attacks detected");
 bool rejected=false;try{Clip huge(2,maxBytes,48000);}catch(...){rejected=true;}check(rejected,"allocation limit enforced");
 std::printf("WaveMover DSP: %d checks passed\n",checks);
}
