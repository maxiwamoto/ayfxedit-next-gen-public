// Native port of AYFXEdit's effect/bank model and renderer.
// Original source: Shiru; improved fork: Threetwosevensixseven.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include <cwctype>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace ayfx {
using Bytes=std::vector<uint8_t>;
constexpr int MaxFrames=4096, MaxEffects=256, SampleRate=44100;
struct Cell { int tone=0, noise=0, volume=0; bool t=false,n=false; bool envelope=false; int envelopePeriod=1,shape=-1; };
struct Effect { std::string name="Untitled"; std::array<Cell,MaxFrames> cells{}; std::string tags,sourcePath,recipe; bool favorite=false; Bytes sourceAudio; };
using Bank=std::vector<Effect>;
inline int length(const Effect& e) { for(int i=MaxFrames-1;i>=0;--i) if(e.cells[i].volume||e.cells[i].envelope) return i+1; return 0; }
inline void require(bool b,const char* msg) { if(!b) throw std::runtime_error(msg); }
inline unsigned u16(const Bytes& b,size_t p) { require(p+2<=b.size(),"Truncated 16-bit field");return b[p]|(unsigned(b[p+1])<<8); }
inline uint32_t u32(const Bytes& b,size_t p) { require(p+4<=b.size(),"Truncated 32-bit field");return u16(b,p)|(uint32_t(u16(b,p+2))<<16); }
inline void word(Bytes& b,unsigned n) { b.push_back(uint8_t(n));b.push_back(uint8_t(n>>8)); }
inline void dword(Bytes& b,uint32_t n) { word(b,n);word(b,n>>16); }
inline void text(Bytes& b,const char* s) { while(*s)b.push_back(*s++); }
inline Bytes read(const std::filesystem::path& p,size_t limit=64*1024*1024) {
 std::ifstream f(p,std::ios::binary);require(bool(f),"Cannot open file");
 auto n=std::filesystem::file_size(p);require(n<=limit,"File exceeds import size limit");
 Bytes b(static_cast<size_t>(n));f.read(reinterpret_cast<char*>(b.data()),b.size());require(bool(f),"Cannot read file");return b;
}
inline void write(const std::filesystem::path& p,const Bytes& b) {
 std::ofstream f(p,std::ios::binary);require(bool(f),"Cannot create file");
 f.write(reinterpret_cast<const char*>(b.data()),b.size());f.close();require(bool(f),"Cannot finish writing file");
}
// Same flag bits, delta fields and terminator as UnitMain.cpp/EffectEncode.
inline Bytes encode(const Effect& e) {
 Bytes b;int tone=-1,noise=-1;
 for(int i=0;i<length(e);++i) { auto c=e.cells[i];
  require(c.tone>=0&&c.tone<=4095&&c.noise>=0&&c.noise<=31&&c.volume>=0&&c.volume<=15,"Invalid AY register value");
  int flags=c.volume|(c.t?0:16)|(c.n?0:128)|(c.tone!=tone?32:0)|(c.noise!=noise?64:0);
  b.push_back(uint8_t(flags));if(flags&32) word(b,c.tone);if(flags&64)b.push_back(uint8_t(c.noise));tone=c.tone;noise=c.noise;
 }b.push_back(0xd0);b.push_back(0x20);return b;
}
inline Effect decode(const Bytes& b,size_t start,size_t end,size_t* consumed=nullptr) {
 require(start<=end&&end<=b.size(),"Invalid effect range");Effect e;int tone=0,noise=0,row=0;size_t p=start;
 while(p<end) { int flags=b[p++];
  if(flags&32) {require(p+2<=end,"Truncated tone");tone=u16(b,p)&4095;p+=2;}
  if(flags&64) {require(p<end,"Truncated noise");noise=b[p++];if(flags==0xd0&&noise>=32){if(consumed)*consumed=p;return e;}noise&=31;}
  require(row<MaxFrames,"Effect exceeds 4096 frames");e.cells[row++]={tone,noise,flags&15,!(flags&16),!(flags&128)};
 }throw std::runtime_error("Missing AFX end marker");
}
inline Bank decodeBank(const Bytes& b) {
 require(!b.empty(),"Empty bank");int count=b[0]?b[0]:256;size_t head=1+count*2;
 require(b.size()>=head,"Truncated bank offset table");Bank bank;bank.reserve(count);
 for(int i=0;i<count;++i) { size_t start=u16(b,1+i*2)+2+i*2;
  size_t end=i+1<count?u16(b,3+i*2)+4+i*2:b.size();require(start>=head&&start<end&&end<=b.size(),"Invalid bank offset");
  size_t used;auto e=decode(b,start,end,&used);e.name="Effect "+std::to_string(i+1);
  if(used<end) { auto zero=std::find(b.begin()+used,b.begin()+end,0);require(zero!=b.begin()+end,"Unterminated effect name");
   e.name=std::string(b.begin()+used,zero);require(e.name.size()<=255,"Effect name exceeds 255 bytes"); }
  bank.push_back(std::move(e));
 }return bank;
}
inline Bytes encodeBank(const Bank& bank,bool names) {
 require(!bank.empty()&&bank.size()<=256,"Banks require 1-256 effects");Bytes b(1+bank.size()*2);b[0]=uint8_t(bank.size());
 for(size_t i=0;i<bank.size();++i) { size_t off=b.size()-(2+i*2);require(off<=65535,"Bank offset exceeds the 16-bit AFB limit; split this bank");
  b[1+i*2]=uint8_t(off);b[2+i*2]=uint8_t(off>>8);auto fx=encode(bank[i]);b.insert(b.end(),fx.begin(),fx.end());
  if(names) { require(bank[i].name.size()<=255,"Effect name exceeds 255 bytes");b.insert(b.end(),bank[i].name.begin(),bank[i].name.end());b.push_back(0); }
 }return b;
}
// AY model below is adapted from src/wav_render.h. Unsigned LFSR avoids
// undefined signed shifts. Fractional clock accumulation avoids integer clock loss.
struct LegacyChip {
 unsigned reg=0xffff;int qcc=0,ns=0,ncount=0,tcount=0,ts=0,divider=0;
 int tick(const Cell& c) {
  static const int volume[16]={0,836/3,1212/3,1773/3,2619/3,3875/3,5397/3,8823/3,10392/3,16706/3,23339/3,29292/3,36969/3,46421/3,55195/3,65535/3};
  divider^=1;if(tcount>=c.tone){tcount=0;ts^=1;}++tcount;
  if(divider) {if(ncount==0){int di=(qcc^((reg>>13)&1))^1;qcc=(reg>>15)&1;ns=qcc;reg=(reg<<1)|di;}
   ncount=(ncount+1)&31;if(ncount>=c.noise)ncount=0;}
  return ((ts|!c.t)&(ns|!c.n))?volume[c.volume]:0;
 }
};
inline Bytes renderLegacy(const Effect& e,int clock=1773400,int rate=50,int from=0) {
 require(clock>=1000000&&clock<=4000000&&(rate==50||rate==60),"Invalid clock or frame rate");
 const int len=length(e);
 int frames=std::max(0,len-std::clamp(from,0,MaxFrames))+3;
 int samples=int(int64_t(frames)*SampleRate/rate);Bytes b;text(b,"RIFF");dword(b,samples*2+36);text(b,"WAVEfmt ");dword(b,16);
 word(b,1);word(b,1);dword(b,SampleRate);dword(b,SampleRate*2);word(b,2);word(b,16);text(b,"data");dword(b,samples*2);
 LegacyChip chip;int64_t phase=0;Cell silence;
 for(int s=0;s<samples;++s) { int row=from+int(int64_t(s)*rate/SampleRate);const auto& c=row<len?e.cells[row]:silence;
  phase+=clock;int ticks=int(phase/(8*SampleRate));phase%=8*SampleRate;int value=0;for(int t=0;t<ticks;++t)value+=chip.tick(c);
  word(b,ticks?value/ticks:0);
 }return b;
}

// Reference preview: logarithmic volume, 17-bit noise, and band-limited output.
// AY clock/noise division follows the GI datasheet. Volume steps follow the
// ideal logarithmic model also used by openMSX (not a calibrated analog board).
enum class PreviewModel { Reference, Legacy };
inline double amplitude(int level, PreviewModel model=PreviewModel::Reference) {
 static const int old[]={0,836,1212,1773,2619,3875,5397,8823,10392,16706,23339,29292,36969,46421,55195,65535};
 return level==0?0:model==PreviewModel::Legacy?double(old[level])/65535:std::exp2((level-15)*.5);
}
struct Chip {
 uint32_t random=1;int toneCount=0,noiseCount=0,tonePeriod=1,noisePeriod=2;bool tone=false;
 void periods(const Cell& c) {
  int t=std::max(1,c.tone),n=2*std::max(1,c.noise);
  if(t!=tonePeriod){tonePeriod=t;toneCount=std::min(toneCount,t-1);}
  if(n!=noisePeriod){noisePeriod=n;noiseCount=std::min(noiseCount,n-1);}
 }
 bool tick(const Cell& c) {
  periods(c);
  if(++toneCount>=tonePeriod){toneCount=0;tone=!tone;}
  if(++noiseCount>=noisePeriod){noiseCount=0;uint32_t feedback=random&1;random>>=1;random^=(feedback<<16)|(feedback<<13);}
  return (tone||!c.t)&&((random&1)||!c.n);
 }
};
inline Bytes renderExtended(const std::array<const Effect*,3>&,int,int,int,double,unsigned,int,int);
inline Bytes renderChannels(const std::array<const Effect*,3>& channels,int clock=1773400,int rate=50,int from=0,double gain=10000,unsigned audibleMask=7,int to=-1,int tailFrames=3) {
 for(auto e:channels)if(e)for(auto& c:e->cells)if(c.envelope||c.shape>=0)return renderExtended(channels,clock,rate,from,gain,audibleMask,to,tailFrames);
 require(clock>=1000000&&clock<=4000000&&(rate==50||rate==60),"Invalid clock or frame rate");
 from=std::clamp(from,0,MaxFrames);
 int len=0;for(auto e:channels)if(e)len=std::max(len,length(*e));
 // AY has one noise register shared by A/B/C. Reject conflicting authored tracks.
 std::array<int,MaxFrames> noise{};int held=0;
 for(int f=0;f<len;f++){int requested=-1;for(auto e:channels)if(e){auto c=e->cells[f];if(c.n&&c.volume){
  require(requested<0||requested==c.noise,"Channels request different shared AY noise periods at the same frame");requested=c.noise;}}
  if(requested>=0)held=requested;else for(auto e:channels)if(e){held=e->cells[f].noise;break;}noise[f]=held;}
 const int frames=std::max(0,(to<0?len:std::clamp(to,from,MaxFrames))-from)+tailFrames;
 const int samples=int(int64_t(frames)*SampleRate/rate);
 const double inputRate=clock/8.0,ratio=inputRate/SampleRate;
 // Fixed 64-tap Blackman-windowed sinc, 256 fractional phases. No per-effect
 // normalization: the authored volume dynamics remain intact.
 constexpr int taps=64,phases=256,half=31;
 std::array<std::array<float,taps>,phases> filter{};
 double cutoff=.45/ratio;
 for(int phase=0;phase<phases;++phase){double sum=0;for(int j=0;j<taps;++j){double x=j-half-double(phase)/phases;
  double sinc=std::abs(x)<1e-9?2*cutoff:std::sin(2*3.141592653589793*cutoff*x)/(3.141592653589793*x);
  double w=.42-.5*std::cos(2*3.141592653589793*j/(taps-1))+.08*std::cos(4*3.141592653589793*j/(taps-1));
  filter[phase][j]=float(sinc*w);sum+=filter[phase][j];}for(auto& v:filter[phase])v=float(v/sum);}
 std::vector<float> raw(size_t(std::ceil(samples*ratio))+taps,0);std::array<Chip,3> chips;Cell silence;
 for(size_t i=0;i<raw.size();++i){int row=from+int(uint64_t(i)*8*rate/clock);for(int ch=0;ch<3;ch++){auto c=row<len&&channels[ch]?channels[ch]->cells[row]:silence;
   c.noise=row<len?noise[row]:0; // All generators receive the same noise clock, even muted channels.
   if(chips[ch].tick(c)&&(audibleMask&(1u<<ch)))raw[i]+=float(gain*amplitude(c.volume));}}
 Bytes b;text(b,"RIFF");dword(b,samples*2+36);text(b,"WAVEfmt ");dword(b,16);word(b,1);word(b,1);dword(b,SampleRate);dword(b,SampleRate*2);word(b,2);word(b,16);text(b,"data");dword(b,samples*2);
 for(int i=0;i<samples;++i){double at=i*ratio;int center=int(at),phase=std::min(phases-1,int((at-center)*phases));double sum=0;
  for(int j=0;j<taps;++j){int index=center-half+j;if(index>=0&&size_t(index)<raw.size())sum+=raw[index]*filter[phase][j];}
  word(b,unsigned(int16_t(std::clamp(int(std::lround(sum)),-32768,32767))));}
 return b;
}
inline Bytes render(const Effect& e,int clock=1773400,int rate=50,int from=0,PreviewModel model=PreviewModel::Reference) {
 if(model==PreviewModel::Legacy)return renderLegacy(e,clock,rate,std::clamp(from,0,MaxFrames));
 return renderChannels({&e,nullptr,nullptr},clock,rate,from,21845);
}

// Exported channel names retain grouping even after an AFB Save As or reordering.
struct EffectGroup {std::string name;std::array<int,3> tracks{{-1,-1,-1}};int loopStart=-1,loopEnd=-1;int priority=128;};
inline std::vector<EffectGroup> groupBank(const Bank& bank){
 std::vector<EffectGroup> groups;
 for(size_t i=0;i<bank.size();i++){const auto& name=bank[i].name;
  auto hex=[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');};
  bool named=name.size()>5&&hex(name[0])&&hex(name[1])&&name[2]=='_'&&name[3]>='A'&&name[3]<='C'&&name[4]=='_';
  int ch=named?name[3]-'A':0;std::string title=named?name.substr(0,2)+"  "+name.substr(5):name;
  int found=-1;if(named)for(size_t g=0;g<groups.size();g++)if(groups[g].name==title&&groups[g].tracks[ch]<0){found=int(g);break;}
  if(found<0){groups.push_back({title});found=int(groups.size())-1;}groups[found].tracks[ch]=int(i);
 }return groups;
}
inline Bytes csv(const Effect& e) {std::string s;char line[80];for(int i=0;i<length(e);++i){auto c=e.cells[i];snprintf(line,sizeof(line),"%d,%d,0x%03X,0x%02X,0x%X\r\n",c.t,c.n,c.tone,c.noise,c.volume);s+=line;}return Bytes(s.begin(),s.end());}
inline Bytes vt2(const Effect& e,int clock,int base=48) {
 std::string s="[Sample]\r\n";int offset=int(clock/(16.0*440*std::pow(2.0,(base+12-69)/12.0)));char line[100];
 for(int i=0;i<std::min(63,length(e));++i){auto c=e.cells[i];int t=std::clamp(c.tone-offset,-1023,1023);
 snprintf(line,sizeof(line),"%c%ce %c%03X_ +%02X_ %X_\r\n",c.t?'T':'t',c.n?'N':'n',t<0?'-':'+',abs(t),c.noise,c.volume);s+=line;}
 s+="tne +000_ +00_ 0_ L\r\n";return Bytes(s.begin(),s.end());
}
inline Effect importPSG(const Bytes& b,int channel) {
 require(b.size()>=16&&std::equal(b.begin(),b.begin()+3,"PSG"),"Not a PSG register dump");
 Effect e;std::array<int,16> regs{};regs[7]=63;size_t p=16;int row=0;bool started=false;
 while(p<b.size()&&row<MaxFrames) {int cmd=b[p++],frames=0;if(cmd==253)break;
  if(cmd==255)frames=1;else if(cmd==254){require(p<b.size(),"Truncated PSG delay");frames=b[p++]*4;}
  else {require(cmd<16&&p<b.size(),"Invalid PSG register command");regs[cmd]=b[p++];if(channel<0&&cmd>=8&&cmd<=10&&(regs[cmd]&15))channel=cmd-8;}
  if(channel<0)continue;Cell c{(regs[channel*2]|(regs[channel*2+1]<<8))&4095,regs[6]&31,regs[8+channel]&15,!(regs[7]&(1<<channel)),!(regs[7]&(8<<channel))};
  while(frames--&&row<MaxFrames){if(!started&&!c.volume)continue;started=true;e.cells[row++]=c;}
 }return e;
}
// WAV pitch detection follows the difference-minimum method in import_wav.h
// (John Montgomery's 5KTuner, CC-BY 2008). RIFF parsing/resampling is replaced.
inline Effect importWAV(const Bytes& b,int clock,int rate) {
 require(b.size()>=12&&std::equal(b.begin(),b.begin()+4,"RIFF")&&std::equal(b.begin()+8,b.begin()+12,"WAVE"),"Not a RIFF WAVE file");
 unsigned channels=0,hz=0,bits=0,align=0;size_t data=0,count=0;
 for(size_t p=12;p+8<=b.size();) {uint32_t n=u32(b,p+4);require(n<=b.size()-p-8,"Truncated WAV chunk");
  if(std::equal(b.begin()+p,b.begin()+p+4,"fmt ")){require(n>=16&&u16(b,p+8)==1,"WAV import supports uncompressed PCM");channels=u16(b,p+10);hz=u32(b,p+12);align=u16(b,p+20);bits=u16(b,p+22);}
  if(std::equal(b.begin()+p,b.begin()+p+4,"data")){data=p+8;count=n;}p+=8+size_t(n)+(n&1);
 }
 require(data&&channels>=1&&channels<=2&&(bits==8||bits==16)&&hz>=4000&&hz<=192000&&align==channels*(bits/8),"WAV must be 8/16-bit PCM, mono or stereo, 4-192 kHz");
 size_t total=count/align;require(total>0,"Empty WAV");int step=SampleRate/rate;
 size_t outCount=std::min(size_t(MaxFrames)*step,size_t(std::ceil(double(total)*SampleRate/hz)));
 std::vector<double> wave(outCount+step*2,0);double peak=1;
 auto sample=[&](size_t i){double v=0;if(i>=total)return v;for(unsigned ch=0;ch<channels;++ch){size_t p=data+i*align+ch*(bits/8);v+=bits==8?(int(b[p])-128)*256:int16_t(u16(b,p));}return v/channels;};
 for(size_t i=0;i<outCount;++i){double pos=double(i)*hz/SampleRate;size_t j=size_t(pos);wave[i]=sample(j)*(1-(pos-j))+sample(j+1)*(pos-j);peak=std::max(peak,std::abs(wave[i]));}
 Effect e;int frames=int((outCount+step-1)/step);std::vector<double> amps(frames);double maxAmp=1;
 for(int f=0;f<frames;++f){for(int j=0;j<step;++j)amps[f]+=std::abs(wave[f*step+j]);amps[f]/=step;maxAmp=std::max(maxAmp,amps[f]);}
 static const double levels[]={0,836,1212,1773,2619,3875,5397,8823,10392,16706,23339,29292,36969,46421,55195,65535};
 for(int f=0;f<frames;++f){int n=step;std::vector<double> a(n);for(int j=0;j<n;++j)a[j]=wave[f*step+j*2]+wave[f*step+j*2+1];
  double prev=0,prevDx=0,maxDiff=0;int lag=0;
  for(int i=0;i<n;++i){double diff=0;for(int j=0;j<n;++j)diff+=std::abs(a[j]-(i+j<n?a[i+j]:0));double dx=prev-diff;
   if(dx<0&&prevDx>0&&diff<maxDiff*.5&&i>2){lag=i-1;break;}prevDx=dx;prev=diff;maxDiff=std::max(maxDiff,diff);}
  double target=amps[f]/maxAmp*65535;int vol=0;for(int v=1;v<16;++v)if(std::abs(levels[v]-target)<std::abs(levels[vol]-target))vol=v;
  int tone=lag?std::clamp(int(double(clock)*lag/(8*SampleRate)),1,4095):0;
  e.cells[f]={tone,std::clamp(tone>>7,0,31),vol,lag>0,lag==0};
 }return e;
}
} // namespace ayfx

#include "ay_stream.h"
