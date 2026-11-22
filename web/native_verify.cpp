// Independent native execution of files saved by Web Studio.
#include "game_export.h"
#include "register_export.h"
#include <iostream>
#include <cstring>
using namespace ayfx; namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 require(argc==3,"usage: native_verify bank.afb output-folder");
 fs::path input=argv[1],out=argv[2];auto raw=read(input);auto bank=decodeBank(raw);auto side=input;side+=".ayfx-project";auto p=decodeProject(read(side),bank,raw);fs::create_directories(out);
 auto saved=encodeBank(bank,true);write(out/"roundtrip.afb",saved);write(out/"roundtrip.afb.ayfx-project",encodeProject(bank,saved,p));
 for(size_t i=0;i<p.groups.size();i++){
  auto& g=p.groups[i];auto prefix=std::to_string(i);auto regs=encodeRegisters(bank,g,p.clock,p.rate,false);write(out/(prefix+".ayregs"),regs.data);write(out/(prefix+".json"),Bytes(regs.metadata.begin(),regs.metadata.end()));
  std::array<const Effect*,3> channels{};int len=1,count=0;for(int ch=0;ch<3;ch++)if(g.tracks[ch]>=0){channels[ch]=&bank[g.tracks[ch]];count++;len=std::max(len,length(*channels[ch]));}if(g.loopStart>=0)len=g.loopEnd;
  for(unsigned mask=0;mask<8;mask++)write(out/(prefix+"-"+std::to_string(mask)+".wav"),renderChannels(channels,p.clock,p.rate,0,count==1?21845:10000,mask,len,3));
  AYStream stream(registerFrames(channels,len),p.clock,p.rate,7,count==1?21845:10000,0,g.loopStart,g.loopEnd);std::vector<float> samples(33075);for(auto& v:samples)v=stream.next()/32768.f;Bytes pcm(samples.size()*sizeof(float));std::memcpy(pcm.data(),samples.data(),pcm.size());write(out/(prefix+".f32"),pcm);
  for(int ch=0;ch<3;ch++)if(channels[ch]){bool extended=false;for(auto& c:channels[ch]->cells)if(c.envelope||c.shape>=0)extended=true;if(!extended)write(out/(prefix+"-"+std::to_string(ch)+".afx"),encode(*channels[ch]));}
 }
 exportBankData(out/"sdk",bank,p);std::cout<<"Native verified "<<p.groups.size()<<" groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
