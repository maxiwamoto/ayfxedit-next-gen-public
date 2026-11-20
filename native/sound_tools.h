#pragma once
#include "workshop.h"
namespace ayfx {
struct CurvePoint {double x,y;};
inline void applyCurve(Effect& e,int first,int last,int lane,std::vector<CurvePoint> points,bool smooth=false){
 require(first>=0&&last>=first&&last<MaxFrames&&lane>=0&&lane<3&&!points.empty(),"Invalid curve range");
 std::sort(points.begin(),points.end(),[](auto a,auto b){return a.x<b.x;});
 for(int f=first;f<=last;f++){double x=last==first?0:double(f-first)/(last-first),y=points.front().y;
  for(size_t j=1;j<points.size();j++){if(x>=points[j].x){y=points[j].y;continue;}double a=points[j-1].x,b=points[j].x,t=std::clamp((x-a)/std::max(1e-9,b-a),0.,1.);if(smooth)t=t*t*(3-2*t);y=points[j-1].y+(points[j].y-points[j-1].y)*t;break;}
  auto& c=e.cells[f];if(lane==0){c.volume=std::clamp(int(std::lround(y*15)),0,15);c.envelope=false;}
  if(lane==1){c.tone=std::clamp(int(std::lround(y*4095)),0,4095);c.t=true;}
  if(lane==2){c.noise=std::clamp(int(std::lround(y*31)),0,31);c.n=true;}
 }
}
inline void modulate(Effect& e,int a,int b,double semitones,double cycles){
 require(a>=0&&b>=a&&b<MaxFrames&&std::isfinite(semitones)&&std::abs(semitones)<=24&&cycles>=0&&cycles<=64,"Invalid modulation");
 for(int f=a;f<=b;f++){double x=double(f-a)/std::max(1,b-a);auto& c=e.cells[f];c.tone=std::clamp(int(std::lround(c.tone/std::exp2(semitones*std::sin(2*Pi*x*cycles)/12.))),1,4095);}
}
struct GeneratorOptions {int preset=0,frames=24,channels=1,clock=1789773;double startHz=1200,endHz=100,noise=.5;uint32_t seed=1;};
inline ConvertedWave generateSound(const GeneratorOptions& o){
 require(o.frames>0&&o.frames<=MaxFrames&&o.channels>=1&&o.channels<=3&&o.startHz>=20&&o.startHz<=20000&&o.endHz>=20&&o.endHz<=20000&&o.noise>=0&&o.noise<=1,"Generator settings out of range");
 ConvertedWave out;out.frames=o.frames;out.tracks.resize(o.channels);uint32_t seed=o.seed;
 for(int f=0;f<o.frames;f++){double x=double(f)/std::max(1,o.frames-1);seed=seed*1664525u+1013904223u;double jitter=((seed>>16)/65535.-.5);
  for(int ch=0;ch<o.channels;ch++){auto& c=out.tracks[ch].cells[f];double hz=o.startHz*std::pow(o.endHz/o.startHz,x),volume=std::pow(1-x,.6);
   bool noise=false,tone=true;int period=std::clamp(int(1+30*(.2+.8*x)),1,31);
   switch(o.preset){
   case 1:noise=true;tone=ch>0;volume=std::pow(1-x,1.5);break; // Explosion
   case 2:hz=o.startHz*std::pow(o.endHz/o.startHz,1-x);break; // Jump
   case 3:hz=o.startHz*std::exp2((int(x*4)*4+ch*7)/12.);volume=(1-x)*(.65+.35*std::sin(x*Pi));break; // Pickup
   case 4:noise=true;tone=false;volume=std::pow(std::max(0.,1-std::fmod(x*4,1.)),4)*(1-.5*x);period=18;break; // Footsteps
   case 5:hz=o.startHz*(1+.08*std::sin(x*Pi*12));noise=ch>0;volume=.65+.2*std::sin(x*Pi*8);break; // Engine
   default:noise=ch>0&&x<o.noise;tone=ch==0;
   }
   hz*=std::exp2(ch/12.)*(1+jitter*.08);c={std::clamp(int(o.clock/(16*hz)),1,4095),period,ayVolume(std::clamp(volume,0.,1.)),tone,noise};
   if(noise)c.volume=ayVolume(volume*o.noise);
  }
 }return out;
}
inline void requireClassic(const Effect& e){require(!hasEnvelope(e),"This effect uses hardware envelopes. Save the project or export the game SDK; classic AFX/CSV/VT2 cannot preserve envelopes.");}
struct Capture {std::vector<Registers> frames;int clock=1789773,rate=50;};
inline Capture capturePSG(const Bytes& b,int rate){
 require(b.size()>=16&&std::equal(b.begin(),b.begin()+3,"PSG"),"Not a PSG capture");Capture out;out.rate=rate;Registers regs{};regs[7]=63;regs[13]=255;
 for(size_t p=16;p<b.size();){int op=b[p++],wait=0;if(op==253)break;if(op==255)wait=1;else if(op==254){require(p<b.size(),"Truncated PSG delay");wait=b[p++]*4;}else{require(op<16&&p<b.size(),"Invalid PSG command");auto value=b[p++];if(op<14)regs[op]=value;}
  while(wait--){require(out.frames.size()<size_t(300*rate),"Capture exceeds five minutes");out.frames.push_back(regs);regs[13]=255;}}
 return out;
}
inline Capture captureVGM(const Bytes& b,int rate){
 require(b.size()>=0x80&&std::equal(b.begin(),b.begin()+4,"Vgm "),"Not an uncompressed VGM file");
 Capture out;out.rate=rate;out.clock=int(u32(b,0x74)&0x3fffffff);require(out.clock>=1000000&&out.clock<=4000000,"VGM has no supported AY clock");
 require(!(u32(b,0x74)&0x40000000),"Dual-AY VGM is not supported");size_t pos=u32(b,8)>=0x150?(u32(b,0x34)?0x34+u32(b,0x34):0x40):0x40;
 require(pos<=b.size(),"VGM data offset is outside the file");Registers regs{};regs[7]=63;regs[13]=255;uint64_t elapsed=0,next=0;int step=SampleRate/rate;
 auto wait=[&](int n){elapsed+=n;while(next<elapsed){require(out.frames.size()<size_t(300*rate),"Capture exceeds five minutes");out.frames.push_back(regs);regs[13]=255;next+=step;}};
 bool ended=false;
 while(pos<b.size()){int op=b[pos++];auto take=[&](size_t n){require(n<=b.size()-pos,"Truncated VGM command");size_t at=pos;pos+=n;return at;};
  if(op==0x66){ended=true;break;}if(op==0xa0){auto p=take(2);require(b[p]<16,"Second AY chip/register not supported");if(b[p]<14)regs[b[p]]=b[p+1];}
  else if(op==0x61){auto p=take(2);wait(int(u16(b,p)));}
  else if(op==0x62)wait(735);else if(op==0x63)wait(882);else if(op>=0x70&&op<=0x7f)wait((op&15)+1);
  else if(op>=0x80&&op<=0x8f)wait(op&15);
  else if(op==0x67){auto p=take(6);require(b[p]==0x66,"Invalid VGM block");take(u32(b,p+2)&0x7fffffff);}
  else if(op==0x4f||op==0x50)take(1);
  else if((op>=0x51&&op<=0x5f)||(op>=0xa1&&op<=0xbf))take(2);
  else if(op>=0xc0&&op<=0xdf)take(3);
  else if(op==0xe0||op==0xe1)take(4);
  else throw std::runtime_error("Unsupported VGM command; export a plain AY register log");
 }
 require(ended&&!out.frames.empty(),"VGM has no complete audio frames");return out;
}
inline ConvertedWave extractCapture(const Capture& cap,int start,int end,unsigned mask){
 require(start>=0&&end>start&&end<=int(cap.frames.size())&&end-start<=MaxFrames&&mask>0&&mask<8,"Invalid capture range/channels");
 ConvertedWave out;out.frames=end-start;Registers initial{};initial[7]=63;initial[13]=0;
 for(int i=0;i<=start;i++)for(int r=0;r<14;r++)if(r!=13||cap.frames[i][r]!=255)initial[r]=cap.frames[i][r];
 for(int ch=0;ch<3;ch++)if(mask&(1<<ch)){
  Effect e;e.name=std::string("Captured ")+char('A'+ch);
  for(int f=start;f<end;f++){auto r=cap.frames[f];auto& c=e.cells[f-start];c={r[ch*2]+256*(r[ch*2+1]&15),r[6]&31,r[ch+8]&15,!(r[7]&(1<<ch)),!(r[7]&(8<<ch))};c.envelope=(r[ch+8]&16)!=0;c.envelopePeriod=r[11]+256*r[12];c.shape=r[13]==255?-1:r[13]&15;if(f==start&&c.envelope)c.shape=initial[13]&15;}
  out.tracks.push_back(std::move(e));
 }return out;
}
}
