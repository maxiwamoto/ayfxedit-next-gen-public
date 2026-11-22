// Independent native execution of files saved by Web Studio.
#include "game_export.h"
#include "sound_tools.h"
#include "register_export.h"
#include <iostream>
#include <cstring>
using namespace ayfx; namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 require(argc==3,"usage: native_verify bank.afb output-folder");
 fs::path input=argv[1],out=argv[2];auto raw=read(input);auto bank=decodeBank(raw);auto side=input;side+=".ayfx-project";auto p=decodeProject(read(side),bank,raw);fs::create_directories(out);
 auto saved=encodeBank(bank,true);write(out/"roundtrip.afb",saved);write(out/"roundtrip.afb.ayfx-project",encodeProject(bank,saved,p));
 auto saveCase=[&](const std::string& name,const Bank& b,const Project& project){auto data=encodeBank(b,true);write(out/(name+".afb"),data);write(out/(name+".project"),encodeProject(b,data,project));};
 {auto next=bank;auto track=p.groups[0].tracks[0];if(track<0)track=*std::find_if(p.groups[0].tracks.begin(),p.groups[0].tracks.end(),[](int t){return t>=0;});applyCurve(next[track],2,18,1,{{0,.15},{.3,.8},{.65,.1},{1,.5}},true);saveCase("curve",next,p);
 next=bank;for(int f=2;f<=18;f++)next[track].cells[f].tone=std::clamp(int(std::lround(next[track].cells[f].tone/std::exp2(3.5/12))),0,4095);modulate(next[track],2,18,1.25,3);saveCase("pitch",next,p);}
 {GeneratorOptions o{1,35,3,p.clock,500,70,.6,782};auto result=generateSound(o);EffectGroup g;g.name="Seed check";for(int ch=0;ch<3;ch++){g.tracks[ch]=ch;result.tracks[ch].name=g.name+" "+char('A'+ch);}saveCase("seed",result.tracks,Project{{g},p.clock,p.rate});}
 for(size_t i=0;i<p.groups.size();i++){
  auto& g=p.groups[i];auto prefix=std::to_string(i);auto regs=encodeRegisters(bank,g,p.clock,p.rate,false);write(out/(prefix+".ayregs"),regs.data);write(out/(prefix+".json"),Bytes(regs.metadata.begin(),regs.metadata.end()));
  std::array<const Effect*,3> channels{};int len=1,count=0;for(int ch=0;ch<3;ch++)if(g.tracks[ch]>=0){channels[ch]=&bank[g.tracks[ch]];count++;len=std::max(len,length(*channels[ch]));}if(g.loopStart>=0)len=g.loopEnd;
  for(unsigned mask=0;mask<8;mask++)write(out/(prefix+"-"+std::to_string(mask)+".wav"),renderChannels(channels,p.clock,p.rate,0,count==1?21845:10000,mask,len,3));
  write(out/(prefix+".batch.wav"),renderChannels(channels,p.clock,p.rate));
  bool classic=true;for(auto c:channels)if(c&&hasEnvelope(*c))classic=false;if(classic){auto r=encodeRegisters(bank,g,p.clock,p.rate,true);write(out/(prefix+".psg11"),r.data);write(out/(prefix+".psg11.json"),Bytes(r.metadata.begin(),r.metadata.end()));}
  AYStream stream(registerFrames(channels,len),p.clock,p.rate,7,count==1?21845:10000,0,g.loopStart,g.loopEnd);std::vector<float> samples(33075);for(auto& v:samples)v=stream.next()/32768.f;Bytes pcm(samples.size()*sizeof(float));std::memcpy(pcm.data(),samples.data(),pcm.size());write(out/(prefix+".f32"),pcm);
  for(int ch=0;ch<3;ch++)if(channels[ch]){bool extended=false;for(auto& c:channels[ch]->cells)if(c.envelope||c.shape>=0)extended=true;if(!extended){auto base=prefix+"-"+std::to_string(ch);write(out/(base+".afx"),encode(*channels[ch]));write(out/(base+".csv"),csv(*channels[ch]));write(out/(base+".vt2"),vt2(*channels[ch],p.clock));if(count==1&&g.loopStart<0)write(out/(prefix+".legacy.wav"),render(*channels[ch],p.clock,p.rate,0,PreviewModel::Legacy));}}
 }
 exportBankData(out/"sdk",bank,p);std::cout<<"Native verified "<<p.groups.size()<<" groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
