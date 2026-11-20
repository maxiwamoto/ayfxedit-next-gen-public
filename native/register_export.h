#pragma once
#include "project.h"
namespace ayfx {
struct RegisterExport {Bytes data;std::string metadata;};
inline std::string jsonString(const std::string& value){
 std::string out="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){char escape[7];sprintf_s(escape,"\\u%04x",unsigned(c));out+=escape;}else out+=char(c);}return out+'"';
}
inline RegisterExport encodeRegisters(const Bank& bank,const EffectGroup& group,int clock,int rate,bool psg11){
 require(std::any_of(group.tracks.begin(),group.tracks.end(),[](int t){return t>=0;}),"Choose at least one channel");
 std::vector<bool> seen(bank.size());for(int t:group.tracks){require(t>=-1&&t<int(bank.size()),"Invalid channel assignment");if(t>=0){require(!seen[t],"Duplicate channel assignment");seen[t]=true;}}
 require((group.loopStart==-1&&group.loopEnd==-1)||(group.loopStart>=0&&group.loopEnd>group.loopStart&&group.loopEnd<=MaxFrames),"Invalid export loop");
 require((rate==50||rate==60)&&clock>=1000000&&clock<=4000000,"Invalid register export timing");
 std::array<const Effect*,3> channels{};unsigned mask=0;int count=1;
 for(int ch=0;ch<3;ch++)if(group.tracks[ch]>=0){channels[ch]=&bank[group.tracks[ch]];mask|=1<<ch;count=std::max(count,length(*channels[ch]));}
 if(group.loopStart>=0)count=group.loopEnd;
 auto frames=registerFrames(channels,count);RegisterExport out;
 for(auto r:frames){
  if(psg11){require(!((r[8]|r[9]|r[10])&16)&&r[13]==255,"PSG11 cannot preserve hardware envelopes. Export .ayregs or use the compact game SDK.");r[7]=(r[7]&63)|128;}
  out.data.insert(out.data.end(),r.begin(),r.begin()+(psg11?11:14));
 }
 out.metadata="{\n  \"format\": \""+std::string(psg11?"PSG11":"AY14")+"\",\n  \"version\": 1,\n  \"name\": "+jsonString(group.name)+",\n  \"registers_per_frame\": "+std::to_string(psg11?11:14)+",\n  \"frames\": "+std::to_string(count)+",\n  \"ay_clock_hz\": "+std::to_string(clock)+",\n  \"tick_hz\": "+std::to_string(rate)+",\n  \"channel_mask\": "+std::to_string(mask)+",\n  \"priority\": "+std::to_string(group.priority)+",\n  \"loop_start\": "+(group.loopStart<0?"null":std::to_string(group.loopStart))+",\n  \"loop_end_exclusive\": "+(group.loopStart<0?"null":std::to_string(group.loopEnd))+",\n  \"r13_no_write\": "+std::string(psg11?"null":"255")+"\n}\n";
 return out;
}
inline void exportRegisters(const std::filesystem::path& path,const Bank& bank,const EffectGroup& group,int clock,int rate){
 auto extension=path.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),towlower);
 require(extension==L".psg11"||extension==L".ayregs","Choose .psg11 or .ayregs for register export");
 auto result=encodeRegisters(bank,group,clock,rate,extension==L".psg11");auto side=path;side+=L".json";
 write(path,result.data);write(side,Bytes(result.metadata.begin(),result.metadata.end()));
}
}
