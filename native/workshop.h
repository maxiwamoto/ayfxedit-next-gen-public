#pragma once
#include "project.h"
#include <complex>
#include <functional>
#include <memory>
namespace ayfx {
constexpr double Pi=3.14159265358979323846;
struct SourceWave {int rate=0,channels=0,bits=0;std::vector<float> mono;double duration()const{return double(mono.size())/rate;}};
inline SourceWave decodeSourceWave(const Bytes& b){
 require(b.size()>=12&&std::equal(b.begin(),b.begin()+4,"RIFF")&&std::equal(b.begin()+8,b.begin()+12,"WAVE"),"Not a RIFF WAV file");
 SourceWave w;int format=0,align=0;size_t data=0,count=0;
 for(size_t p=12;p+8<=b.size();){size_t n=u32(b,p+4);require(n<=b.size()-p-8,"Truncated WAV chunk");
  if(std::equal(b.begin()+p,b.begin()+p+4,"fmt ")){require(n>=16,"Truncated WAV format");format=u16(b,p+8);w.channels=u16(b,p+10);w.rate=int(u32(b,p+12));align=u16(b,p+20);w.bits=u16(b,p+22);}
  if(std::equal(b.begin()+p,b.begin()+p+4,"data")){data=p+8;count=n;}p+=8+n+(n&1);
 }
 require(data&&format==1&&(w.channels==1||w.channels==2)&&(w.bits==8||w.bits==16)&&w.rate>=4000&&w.rate<=192000&&align==w.channels*(w.bits/8),"Use uncompressed 8/16-bit PCM WAV, mono or stereo, 4-192 kHz");
 require(count&&count%align==0,"Empty or incomplete WAV sample frame");size_t frames=count/align;require(double(frames)/w.rate<=300,"Choose a WAV of five minutes or less, then trim the effect in the workshop");w.mono.resize(frames);
 for(size_t i=0;i<frames;i++){double v=0;for(int ch=0;ch<w.channels;ch++){size_t p=data+i*align+ch*(w.bits/8);v+=w.bits==8?(int(b[p])-128)/128.:int16_t(u16(b,p))/32768.;}w.mono[i]=float(v/w.channels);}return w;
}
struct ConvertOptions {double start=0,end=0;int channels=2,clock=1789773,rate=60;double attackMs=0,decayMs=0,brightness=0,pitch=0;bool normalize=true;double toneSensitivity=50,noiseSensitivity=50;};
struct ConvertedWave {Bank tracks;int frames=0;double duration=0;};
inline int validateConversion(const SourceWave& w,const ConvertOptions& o){
 require(std::isfinite(o.start)&&std::isfinite(o.end)&&o.start>=0&&o.end>o.start&&o.end<=w.duration()+1e-6,"Select a non-empty range inside the audio");
 require(o.channels>=1&&o.channels<=3&&(o.rate==50||o.rate==60)&&o.clock>=1000000&&o.clock<=4000000,"Invalid conversion settings");
 int frames=int(std::ceil((o.end-o.start)*o.rate-1e-8));require(frames<=MaxFrames,"Selection exceeds 4096 AY frames; shorten the selected range");
 require(o.attackMs>=0&&o.attackMs<=2000&&o.decayMs>=0&&o.decayMs<=2000&&o.brightness>=-100&&o.brightness<=100&&o.pitch>=-24&&o.pitch<=24,"Conversion control out of range");require(o.toneSensitivity>=0&&o.toneSensitivity<=100&&o.noiseSensitivity>=0&&o.noiseSensitivity<=100,"Sensitivity out of range");return frames;
}
// Windowed-sinc resampling, including low-pass filtering for high-rate source WAVs.
inline std::vector<float> selectedSamples(const SourceWave& w,double start,double end,int rate,const std::function<bool()>& cancel={}){
 size_t count=size_t(std::ceil((end-start)*rate-1e-8));std::vector<float> out(count);double cutoff=.47*std::min(1.,double(rate)/w.rate);int radius=std::min(160,std::max(16,int(std::ceil(16.*w.rate/rate))));
 for(size_t i=0;i<count;i++){if((i&4095)==0&&cancel&&cancel())throw std::runtime_error("Conversion cancelled");double at=(start+double(i)/rate)*w.rate;int center=int(at);double total=0,weight=0;
  for(int j=-radius;j<=radius;j++){int pos=center+j;if(pos<0||pos>=int(w.mono.size()))continue;double x=pos-at;double sinc=std::abs(x)<1e-9?2*cutoff:std::sin(2*Pi*cutoff*x)/(Pi*x);double win=.5+.5*std::cos(Pi*x/(radius+1));double c=sinc*win;total+=w.mono[pos]*c;weight+=c;}out[i]=float(weight?total/weight:0);
 }return out;
}
inline Bytes pcmWave(const std::vector<float>& samples){Bytes b;text(b,"RIFF");dword(b,uint32_t(samples.size()*2+36));text(b,"WAVEfmt ");dword(b,16);word(b,1);word(b,1);dword(b,SampleRate);dword(b,SampleRate*2);word(b,2);word(b,16);text(b,"data");dword(b,uint32_t(samples.size()*2));for(float v:samples)word(b,unsigned(int16_t(std::clamp(int(std::lround(v*32767)),-32768,32767))));return b;}
inline Bytes seekWave(const Bytes& wav,double seconds){size_t offset=std::min(wav.size()-44,size_t(std::max(0.,seconds)*SampleRate)*2);Bytes out(wav.begin(),wav.begin()+44);out.insert(out.end(),wav.begin()+44+offset,wav.end());uint32_t size=uint32_t(out.size()-8),data=uint32_t(out.size()-44);for(int i=0;i<4;i++){out[4+i]=uint8_t(size>>(8*i));out[40+i]=uint8_t(data>>(8*i));}return out;}
inline void fft(std::vector<std::complex<double>>& a){size_t n=a.size();for(size_t i=1,j=0;i<n;i++){size_t bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
 for(size_t len=2;len<=n;len<<=1){std::complex<double> step=std::polar(1.,-2*Pi/len);for(size_t i=0;i<n;i+=len){std::complex<double> w=1;for(size_t j=0;j<len/2;j++){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=step;}}}}
constexpr int AnalysisRate=22050,FFTSize=2048,Bands=12;
inline std::vector<double> spectrum(const std::vector<float>& samples,int center){std::vector<std::complex<double>> data(FFTSize);double mean=0;int count=0;
 for(int j=0;j<FFTSize;j++){int p=center+j-FFTSize/2;if(p>=0&&p<int(samples.size())){mean+=samples[p];count++;}}if(count)mean/=count;
 for(int j=0;j<FFTSize;j++){int p=center+j-FFTSize/2;double v=p>=0&&p<int(samples.size())?samples[p]-mean:0;data[j]=v*(.5-.5*std::cos(2*Pi*j/(FFTSize-1)));}fft(data);
 std::vector<double> power(FFTSize/2+1);for(size_t k=1;k<power.size();k++)power[k]=std::norm(data[k]);return power;
}
inline std::array<double,Bands> bandPower(const std::vector<double>& power){std::array<double,Bands> result{};double total=0;
 for(size_t k=2;k<power.size();k++){int band=std::clamp(int(std::log2(k*double(AnalysisRate)/FFTSize/60.)*Bands/std::log2(11025./60.)),0,Bands-1);result[band]+=power[k];total+=power[k];}
 for(auto& p:result)p/=std::max(1e-20,total);return result;
}
inline std::array<std::array<double,Bands>,32> noiseProfiles(int clock,const std::function<bool()>& cancel){std::array<std::array<double,Bands>,32> profiles{};
 for(int n=1;n<=31;n++){if(cancel&&cancel())throw std::runtime_error("Conversion cancelled");std::vector<float> samples(FFTSize*4);Chip chip;Cell cell{1,n,15,false,true};int64_t phase=0;
  for(auto& sample:samples){phase+=clock;int ticks=int(phase/(8*AnalysisRate));phase%=8*AnalysisRate;int on=0;for(int t=0;t<ticks;t++)on+=chip.tick(cell);sample=ticks?float(double(on)/ticks-.5):0;}
  std::vector<double> power(FFTSize/2+1);for(int block=0;block<3;block++){auto p=spectrum(samples,FFTSize/2+block*FFTSize);for(size_t k=0;k<p.size();k++)power[k]+=p[k];}profiles[n]=bandPower(power);
 }profiles[0]=profiles[1];return profiles;
}
inline int ayVolume(double level){int result=0;double error=std::abs(level);for(int v=1;v<=15;v++){double d=std::abs(level-amplitude(v));if(d<error){result=v;error=d;}}return result;}
inline ConvertedWave convertWave(const SourceWave& source,const ConvertOptions& o,const std::function<bool()>& cancel={},const std::function<void(int)>& progress={}){
 ConvertedWave result;result.frames=validateConversion(source,o);result.duration=o.end-o.start;result.tracks.resize(o.channels);
 auto samples=selectedSamples(source,o.start,o.end,AnalysisRate,cancel);auto profiles=noiseProfiles(o.clock,cancel);
 std::vector<double> envelope(result.frames);double peak=0;for(int f=0;f<result.frames;f++){size_t begin=size_t(int64_t(f)*AnalysisRate/o.rate),end=std::min(samples.size(),size_t(int64_t(f+1)*AnalysisRate/o.rate));double mean=0,power=0;for(size_t i=begin;i<end;i++){mean+=samples[i];power+=samples[i]*samples[i];}double n=double(end-begin);envelope[f]=n?std::sqrt(std::max(0.,power/n-(mean/n)*(mean/n))):0;peak=std::max(peak,envelope[f]);}
 std::array<double,3> previous{};int previousNoise=1;
 for(int f=0;f<result.frames;f++){if(cancel&&cancel())throw std::runtime_error("Conversion cancelled");if(progress&&(f%8==0))progress(f*100/result.frames);
  auto power=spectrum(samples,int((int64_t(f)*2+1)*AnalysisRate/(2*o.rate)));double total=0;for(double v:power)total+=v;auto residual=power;
  struct Peak{int bin;double hz,power,score;};std::vector<Peak> peaks;
  for(int k=3;k<FFTSize/2-2;k++)if(power[k]>power[k-1]&&power[k]>=power[k+1]){double cluster=0;for(int j=-2;j<=2;j++)cluster+=power[k+j];
   if(cluster<total*.025*std::exp2((50-o.toneSensitivity)/25.))continue;double left=std::log(power[k-1]+1e-20),mid=std::log(power[k]+1e-20),right=std::log(power[k+1]+1e-20);double delta=std::clamp(.5*(left-right)/(left-2*mid+right),-.5,.5);
   double hz=(k+delta)*AnalysisRate/FFTSize;peaks.push_back({k,hz,cluster,cluster*std::pow(hz/1000.,o.brightness/200.)});}
  std::sort(peaks.begin(),peaks.end(),[](const Peak& a,const Peak& b){return a.score>b.score;});std::vector<Peak> tones;
  for(auto p:peaks){bool harmonic=false;for(auto t:tones){double ratio=p.hz/t.hz;int multiple=int(std::round(ratio));if(multiple>=1&&multiple<=9&&multiple%2==1&&std::abs(ratio-multiple)<.04)harmonic=true;}
   if(harmonic)continue;if(tones.size()==3)break;tones.push_back(p);
   for(int harmonic=1;harmonic<=9;harmonic+=2){int bin=int(std::round(p.bin*harmonic));for(int j=-2;j<=2;j++)if(bin+j>=0&&bin+j<int(residual.size()))residual[bin+j]=0;}
  }
  double noisePower=0;for(double v:residual)noisePower+=v;bool useNoise=noisePower>total*.22*std::exp2((50-o.noiseSensitivity)/25.);
  int toneBudget=o.channels-(useNoise?1:0);if(o.channels==1){bool strong=!tones.empty()&&tones[0].power>total*std::clamp(.42*std::exp2((o.noiseSensitivity-o.toneSensitivity)/50.),.05,.95);useNoise=!strong;toneBudget=strong?1:0;}
  if(int(tones.size())>toneBudget)tones.resize(toneBudget);
  // Keep a continuing tone on the same lane when peak strengths cross.
  for(size_t ch=0;ch<tones.size();ch++)if(previous[ch]>0){size_t best=ch;double distance=1e9;for(size_t j=ch;j<tones.size();j++){double d=std::abs(std::log2(tones[j].hz/previous[ch]));if(d<distance){distance=d;best=j;}}std::swap(tones[ch],tones[best]);}
  auto bands=bandPower(residual);int noise=previousNoise;double best=1e100;for(int n=1;n<=31;n++){double distance=0;for(int b=0;b<Bands;b++){double a=std::sqrt(bands[b]),v=std::sqrt(profiles[n][b]);distance+=(a-v)*(a-v);}distance+=.0003*std::abs(n-previousNoise);if(distance<best){best=distance;noise=n;}}
  previousNoise=noise;noise=std::clamp(int(std::round(noise/std::exp2(o.brightness/50.))),1,31);
  double level=o.normalize?(peak>1e-9?envelope[f]/peak:0):std::min(1.,envelope[f]*1.41421356237);double time=(f+.5)/o.rate;
  if(o.attackMs>0)level*=std::min(1.,time/(o.attackMs/1000.));if(o.decayMs>0)level*=std::clamp((result.duration-time)/(o.decayMs/1000.),0.,1.);
  if(peak<1e-7||envelope[f]<1e-7||total<1e-12)level=0;
  double sum=useNoise?std::sqrt(noisePower):0;for(auto t:tones)sum+=std::sqrt(t.power);sum=std::max(sum,1e-20);
  for(int ch=0;ch<o.channels;ch++){auto& c=result.tracks[ch].cells[f];c.noise=noise;
   if(ch<int(tones.size())){auto t=tones[ch];previous[ch]=t.hz;c.tone=std::clamp(int(std::round(o.clock/(16*t.hz*std::exp2(o.pitch/12.)))),1,4095);c.t=true;c.volume=ayVolume(level*std::sqrt(t.power)/sum);}
   else if(useNoise&&ch==int(tones.size())){c.n=true;c.volume=ayVolume(level*std::sqrt(noisePower)/sum);previous[ch]=0;}else previous[ch]=0;
  }
 }if(progress)progress(100);return result;
}
inline Bytes convertedPreview(const ConvertedWave& result,const ConvertOptions& o){std::array<const Effect*,3> channels{};for(size_t ch=0;ch<result.tracks.size();ch++)channels[ch]=&result.tracks[ch];return renderChannels(channels,o.clock,o.rate,0,result.tracks.size()>1?10000:21845,7,result.frames,0);}
// Apply is a staged transaction: validation precedes the caller's undo snapshot.
inline int applyConverted(Bank& bank,std::vector<EffectGroup>& groups,int group,const ConvertedWave& result,std::string name,bool replace){
 require(!result.tracks.empty()&&result.tracks.size()<=3,"No converted sound");if(name.empty())name="Converted audio";if(name.size()>240){size_t end=240;while(end&&(static_cast<unsigned char>(name[end])&0xc0)==0x80)--end;name.resize(end);}
 Bank next=bank;auto nextGroups=groups;int target=int(nextGroups.size());
 if(!replace){EffectGroup g;g.name=name;for(size_t ch=0;ch<result.tracks.size();ch++){require(next.size()<MaxEffects,"Bank is full; remove tracks or use a new bank");g.tracks[ch]=int(next.size());auto e=result.tracks[ch];e.name=name+" "+char('A'+ch);next.push_back(e);}nextGroups.push_back(g);}
 else{require(group>=0&&group<int(groups.size()),"No effect selected");target=group;auto& g=nextGroups[group];std::vector<int> destinations;for(int t:g.tracks)if(t>=0)destinations.push_back(t);
  for(int ch=0;destinations.size()<result.tracks.size()&&ch<3;ch++)if(g.tracks[ch]<0){require(next.size()<MaxEffects,"Bank is full; cannot add another channel");g.tracks[ch]=int(next.size());destinations.push_back(int(next.size()));Effect e;e.name=g.name+" "+char('A'+ch);next.push_back(e);}
  for(size_t ch=0;ch<destinations.size();ch++){auto& e=next[destinations[ch]];e.cells=ch<result.tracks.size()?result.tracks[ch].cells:std::array<Cell,MaxFrames>{};e.sourceAudio=ch<result.tracks.size()?result.tracks[ch].sourceAudio:Bytes{};e.sourcePath=ch<result.tracks.size()?result.tracks[ch].sourcePath:"";e.recipe=ch<result.tracks.size()?result.tracks[ch].recipe:"";}g.loopStart=g.loopEnd=-1;
 }
 validateGroups(next,nextGroups);encodeBank(next,true);bank=std::move(next);groups=std::move(nextGroups);return target;
}
}
