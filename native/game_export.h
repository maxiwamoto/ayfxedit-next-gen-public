#pragma once
#include "project.h"
#include "build/runtime_embedded.h"
namespace ayfx {
struct PackedEffect {Bytes data;unsigned mask=0;int frames=0;};
inline PackedEffect packEffect(const Bank& bank,const EffectGroup& g){
 std::array<const Effect*,3> channels{};PackedEffect out;int len=0;
 for(int ch=0;ch<3;ch++)if(g.tracks[ch]>=0){channels[ch]=&bank[g.tracks[ch]];out.mask|=1<<ch;len=std::max(len,length(*channels[ch]));}
 if(g.loopStart>=0)len=g.loopEnd;len=std::max(1,len);auto frames=registerFrames(channels,len);out.frames=len;
 for(auto& r:frames){if((r[7]&56)!=56)out.mask|=8;if((r[8]|r[9]|r[10])&16)out.mask|=16;}
 out.data={uint8_t(out.mask),uint8_t(g.priority),255,255};
 Registers previous{};previous[7]=63;int pending=0;size_t loop=65535;
 auto flush=[&](){while(pending){int n=std::min(128,pending);out.data.push_back(uint8_t(128+n-1));pending-=n;}};
 for(int f=0;f<len;f++){auto r=frames[f];bool force=f==0||f==g.loopStart;
  if(f==g.loopStart){flush();loop=out.data.size();}
  unsigned flags=0;Bytes payload;
  for(int ch=0;ch<3;ch++){
   bool lo=force||r[ch*2]!=previous[ch*2],hi=force||r[ch*2+1]!=previous[ch*2+1];
   if(force||lo||hi||r[ch+8]!=previous[ch+8]){flags|=1<<ch;payload.push_back(uint8_t(r[ch+8]|(lo?32:0)|(hi?64:0)));if(lo)payload.push_back(r[ch*2]);if(hi)payload.push_back(r[ch*2+1]);}
  }
  if(force||r[6]!=previous[6]){flags|=8;payload.push_back(r[6]);}
  if(force||r[11]!=previous[11]||r[12]!=previous[12]){flags|=16;payload.push_back(r[11]);payload.push_back(r[12]);}
  if(r[13]!=255){flags|=32;payload.push_back(r[13]);}
  if(force||r[7]!=previous[7]){flags|=64;payload.push_back(r[7]);}
  if(!flags){pending++;continue;}flush();out.data.push_back(uint8_t(flags));out.data.insert(out.data.end(),payload.begin(),payload.end());previous=r;
 }
 flush();out.data.push_back(0);require(out.data.size()<=65535,"Effect stream exceeds 64 KB");
 out.data[2]=uint8_t(loop);out.data[3]=uint8_t(loop>>8);return out;
}
inline std::string effectSymbol(const std::string& name,size_t index){
 std::string result="AYFX_"+std::to_string(index)+"_";for(unsigned char c:name)result+=c>='a'&&c<='z'?char(c-32):((c>='A'&&c<='Z')||(c>='0'&&c<='9'))?char(c):'_';if(result.size()>64)result.resize(64);return result;
}
inline std::string asmHex(unsigned n,int width=2){char s[24];sprintf_s(s,"%0*Xh",width,n);return s;}
inline void writeText(const std::filesystem::path& p,const std::string& s){write(p,Bytes(s.begin(),s.end()));}
struct ExportSummary {size_t bytes=0,frames=0;int effects=0;};
inline ExportSummary exportBankData(const std::filesystem::path& folder,const Bank& bank,const Project& project){
 validateGroups(bank,project.groups);require(!std::filesystem::exists(folder),"Choose a new export folder; existing exports are not overwritten");
 Bytes all;std::string assembly="; AYFXEDIT NEXT GEN - tniASM bank. Keep mapped while playing.\n\nAYFX_BANK:\n",c="#ifndef AYFX_BANK_H\n#define AYFX_BANK_H\n\n",ids="; Named effect IDs\n",report="AYFX compact channel bank v2\n";
 std::vector<PackedEffect> packed;for(auto& g:project.groups)packed.push_back(packEffect(bank,g));
 size_t offset=project.groups.size()*2;require(offset<=65535,"Too many effects");
 std::vector<size_t> owners,offsets;size_t saved=0;
 for(size_t i=0;i<packed.size();i++){size_t owner=i;for(size_t j=0;j<i;j++)if(packed[j].data==packed[i].data){owner=j;break;}owners.push_back(owner);
  if(owner==i){require(offset+packed[i].data.size()<=65535,"Export bank exceeds one 64 KB address space; split the bank");offsets.push_back(offset);offset+=packed[i].data.size();}
  else {offsets.push_back(offsets[owner]);saved+=packed[i].data.size();}word(all,unsigned(offsets.back()));
 }
 for(size_t i=0;i<packed.size();i++){if(i%8==0)assembly+="\tDW\t";else assembly+=",";assembly+="AYFX_EFFECT_"+std::to_string(i);if(i%8==7||i+1==packed.size())assembly+="\n";}
 ExportSummary summary;
 size_t aliasWidth=0;for(size_t i=0;i<owners.size();i++)if(owners[i]!=i)aliasWidth=std::max(aliasWidth,std::string("AYFX_EFFECT_").size()+std::to_string(i).size()+3);
 size_t symbolWidth=0;for(size_t i=0;i<packed.size();i++)symbolWidth=std::max(symbolWidth,effectSymbol(project.groups[i].name,i).size()+3);
 for(size_t i=0;i<packed.size();i++){
  auto symbol=effectSymbol(project.groups[i].name,i);ids+=symbol+":"+std::string(symbolWidth-symbol.size()-1,' ')+"EQU\t"+asmHex(unsigned(i))+"\n";c+="#define "+symbol+" "+std::to_string(i)+"\n";
  if(owners[i]!=i)assembly+="\nAYFX_EFFECT_"+std::to_string(i)+":"+std::string(aliasWidth-std::string("AYFX_EFFECT_").size()-std::to_string(i).size()-1,' ')+"EQU\tAYFX_EFFECT_"+std::to_string(owners[i])+"\n";
  else assembly+="\nAYFX_EFFECT_"+std::to_string(i)+":\n";
  auto& effect=packed[i];if(owners[i]==i)for(size_t p=0;p<effect.data.size();p+=16){assembly+="\tDB\t";for(size_t j=p;j<std::min(p+16,effect.data.size());j++){if(j>p)assembly+=",";assembly+=asmHex(effect.data[j]);}assembly+="\n";}
  if(owners[i]==i)all.insert(all.end(),effect.data.begin(),effect.data.end());summary.frames+=effect.frames;
  report+=(owners[i]==i?"":"[shared with "+std::to_string(owners[i])+"] ")+std::to_string(i)+" "+project.groups[i].name+": "+std::to_string(effect.data.size())+" bytes, "+std::to_string(effect.frames)+" frames, channels "+std::to_string(effect.mask&7)+"\n";
 }
 c+="\nstatic const unsigned char ayfx_bank[] = {\n";
 for(size_t p=0;p<all.size();p++){if(p%16==0)c+=" ";char value[10];sprintf_s(value,"0x%02X,",all[p]);c+=value;if(p%16==15)c+="\n";}
 c+="\n};\n#define AYFX_EFFECT_COUNT "+std::to_string(packed.size())+"\n#endif\n";
 summary.bytes=all.size();summary.effects=int(packed.size());report+="Identical-effect sharing saved "+std::to_string(saved)+" bytes.\n";report+="Total: "+std::to_string(all.size())+" bytes; "+std::to_string(summary.frames)+" authored frames.\nClock: "+std::to_string(project.clock)+" Hz; tick rate: "+std::to_string(project.rate)+" Hz.\n";
 std::filesystem::create_directories(folder);
 try { for(auto& file:runtimeFiles)writeText(folder/file.name,file.text);
 write(folder/"effects.bin",all);writeText(folder/"effects.asm",assembly);writeText(folder/"effect_ids.asm",ids);writeText(folder/"effects.h",c);writeText(folder/"bank-report.txt",report); }catch(...){std::filesystem::remove_all(folder);throw;}return summary;
}
}
