#pragma once
// Register frames and a stateful AY renderer shared by preview, capture and export.
namespace ayfx {
using Registers=std::array<uint8_t,14>;
inline bool hasEnvelope(const Effect& e){for(auto& c:e.cells)if(c.envelope||c.shape>=0)return true;return false;}
inline std::vector<Registers> registerFrames(const std::array<const Effect*,3>& channels,int requested=-1){
 int len=0;for(auto e:channels)if(e)len=std::max(len,length(*e));if(requested>=0)len=requested;
 require(len>=0&&len<=MaxFrames,"Invalid frame count");std::vector<Registers> result;Registers held{};held[7]=63;
 for(int f=0;f<len;f++){
  Registers r=held;r[7]=63;r[13]=255;int noise=-1,period=-1,shape=-1;
  for(int ch=0;ch<3;ch++){
   Cell c=channels[ch]?channels[ch]->cells[f]:Cell{};
   require(c.tone>=0&&c.tone<=4095&&c.noise>=0&&c.noise<=31&&c.volume>=0&&c.volume<=15&&c.envelopePeriod>=0&&c.envelopePeriod<=65535&&c.shape>=-1&&c.shape<=15,"Invalid AY register value");
   r[ch*2]=uint8_t(c.tone);r[ch*2+1]=uint8_t(c.tone>>8);r[ch+8]=uint8_t(c.envelope?16:c.volume);
   if(c.t)r[7]&=~(1<<ch);if(c.n)r[7]&=~(8<<ch);
   if(c.n&&(c.volume||c.envelope)){require(noise<0||noise==c.noise,"Conflicting shared noise periods");noise=c.noise;}
   if(c.envelope){require(period<0||period==c.envelopePeriod,"Conflicting shared envelope periods");period=c.envelopePeriod;}
   if(c.shape>=0){require(shape<0||shape==c.shape,"Conflicting envelope retriggers");shape=c.shape;}
  }
  if(noise>=0)r[6]=uint8_t(noise);if(period>=0){r[11]=uint8_t(period);r[12]=uint8_t(period>>8);}
  if(f==0&&period>=0&&shape<0)shape=0;
  if(shape>=0)r[13]=uint8_t(shape);result.push_back(r);held=r;
 }return result;
}
struct EnvelopeGenerator {
 int period=1,count=0,step=15,attack=0;bool alternate=false,hold=true,holding=true;
 void setPeriod(int value){period=2*std::max(1,value);count=std::min(count,period-1);}
 void trigger(int shape){attack=(shape&4)?15:0;hold=!(shape&8)||(shape&1);alternate=!(shape&8)?attack!=0:(shape&2)!=0;step=15;holding=false;}
 int level()const{return step^attack;}
 void tick(){if(++count>=period){count=0;if(!holding&&--step<0){if(hold){if(alternate)attack^=15;holding=true;step=0;}else{if(alternate)attack^=15;step=15;}}}}
};
class AYStream {
 std::vector<Registers> frames;std::array<Chip,3> chips{};EnvelopeGenerator envelope;Registers regs{};
 int clock,rate,frame=0,loopStart=-1,loopEnd=-1;unsigned mask;double gain;uint64_t framePhase=0,ticks=0,sample=0;
 std::array<float,128> ring{};std::array<std::array<float,64>,256> filter{};
 void apply(){
  if(frame<int(frames.size()))regs=frames[frame];else{regs={};regs[7]=63;regs[13]=255;}
  envelope.setPeriod(regs[11]+256*regs[12]);if(regs[13]!=255)envelope.trigger(regs[13]);
 }
 float tick(){
  float value=0;
  for(int ch=0;ch<3;ch++){Cell c{regs[ch*2]+256*regs[ch*2+1],regs[6],regs[ch+8]&15,!(regs[7]&(1<<ch)),!(regs[7]&(8<<ch))};
   if(chips[ch].tick(c)&&(mask&(1u<<ch)))value+=float(gain*amplitude((regs[ch+8]&16)?envelope.level():c.volume));}
  envelope.tick();framePhase+=uint64_t(8)*rate;
  if(framePhase>=unsigned(clock)){framePhase-=clock;++frame;if(loopStart>=0&&frame==loopEnd)frame=loopStart;apply();}
  return value;
 }
public:
 AYStream(std::vector<Registers> data,int hz,int fps,unsigned audible=7,double volume=10000,int from=0,int a=-1,int b=-1):frames(std::move(data)),clock(hz),rate(fps),frame(from),loopStart(a),loopEnd(b),mask(audible),gain(volume){
  require(hz>=1000000&&hz<=4000000&&(fps==50||fps==60),"Invalid playback clock/rate");
  require(a==-1||(a>=0&&b>a&&b<=int(frames.size())),"Invalid streaming loop");
  constexpr double pi=3.141592653589793;double ratio=clock/(8.*SampleRate),cutoff=.45/ratio;
  for(int phase=0;phase<256;phase++){double sum=0;for(int j=0;j<64;j++){double x=j-31-phase/256.;double sinc=std::abs(x)<1e-9?2*cutoff:std::sin(2*pi*cutoff*x)/(pi*x);double window=.42-.5*std::cos(2*pi*j/63)+.08*std::cos(4*pi*j/63);filter[phase][j]=float(sinc*window);sum+=filter[phase][j];}for(auto& v:filter[phase])v=float(v/sum);}
  apply();
 }
 int16_t next(){
  double at=double(sample++)*clock/(8.*SampleRate);int64_t center=int64_t(at);int phase=std::min(255,int((at-center)*256));
  while(ticks<=uint64_t(center+32)){ring[ticks%128]=tick();++ticks;}
  double value=0;for(int j=0;j<64;j++){int64_t i=center-31+j;if(i>=0)value+=ring[size_t(i)%128]*filter[phase][j];}
  return int16_t(std::clamp(int(std::lround(value)),-32768,32767));
 }
};
inline Bytes renderExtended(const std::array<const Effect*,3>& channels,int clock,int rate,int from,double gain,unsigned mask,int to,int tail){
 int len=0;for(auto e:channels)if(e)len=std::max(len,length(*e));int end=to<0?len:to;
 auto frames=registerFrames(channels,std::max(len,end));AYStream stream(std::move(frames),clock,rate,mask,gain,from);
 int count=std::max(0,end-from+tail)*SampleRate/rate;Bytes b;text(b,"RIFF");dword(b,count*2+36);text(b,"WAVEfmt ");dword(b,16);word(b,1);word(b,1);dword(b,SampleRate);dword(b,SampleRate*2);word(b,2);word(b,16);text(b,"data");dword(b,count*2);
 for(int i=0;i<count;i++)word(b,uint16_t(stream.next()));return b;
}
}
