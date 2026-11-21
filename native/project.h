#pragma once
#include "core.h"
#include <sstream>
#include <iomanip>
#include "platform_files.h"
namespace ayfx {
inline void validateGroups(const Bank& bank,const std::vector<EffectGroup>& groups){
 require(!groups.empty()&&groups.size()<=bank.size(),"Invalid project group count");std::vector<bool> seen(bank.size());
 for(auto& g:groups){require(!g.name.empty()&&g.name.size()<=255&&g.name.find('\0')==std::string::npos,"Invalid effect group name");require(g.priority>=0&&g.priority<=255,"Invalid effect priority");int count=0;
  for(int t:g.tracks){require(t>=-1&&t<int(bank.size()),"Project track index out of range");if(t>=0){require(!seen[t],"A track cannot belong to two channels or effects");seen[t]=true;count++;}}
  require(count>0,"An effect must have at least one channel");
  require((g.loopStart==-1&&g.loopEnd==-1)||(g.loopStart>=0&&g.loopEnd>g.loopStart&&g.loopEnd<=MaxFrames),"Invalid loop frame range");
 }for(bool b:seen)require(b,"Project contains an unassigned track");
}
inline uint64_t bankFingerprint(const Bytes& b){uint64_t h=14695981039346656037ull;for(auto c:b){h^=c;h*=1099511628211ull;}return h;}
struct Project {std::vector<EffectGroup> groups;int clock=1789773,rate=50;};
inline Bytes encodeProject(const Bank& bank,const Bytes& bankBytes,const Project& project){
 validateGroups(bank,project.groups);std::ostringstream s;
 s<<"AYFXEDIT_PROJECT 2\n"<<bankFingerprint(bankBytes)<<' '<<project.clock<<' '<<project.rate<<' '<<project.groups.size()<<'\n';
 for(auto& g:project.groups)s<<std::quoted(g.name)<<' '<<g.tracks[0]<<' '<<g.tracks[1]<<' '<<g.tracks[2]<<' '<<g.loopStart<<' '<<g.loopEnd<<' '<<g.priority<<'\n';
 s<<bank.size()<<'\n';
 for(auto& e:bank){
  require(e.tags.size()<=4096&&e.sourcePath.size()<=32768&&e.recipe.size()<=65536&&e.sourceAudio.size()<=64*1024*1024,"Effect metadata is too large");
  s<<e.favorite<<' '<<std::quoted(e.tags)<<' '<<std::quoted(e.sourcePath)<<' '<<std::quoted(e.recipe)<<'\n';
  size_t count=0;for(auto& c:e.cells)if(c.envelope||c.shape>=0){require(c.envelopePeriod>=0&&c.envelopePeriod<=65535&&c.shape>=-1&&c.shape<=15,"Invalid envelope event");count++;}
  s<<count<<'\n';for(int f=0;f<MaxFrames;f++){auto& c=e.cells[f];if(c.envelope||c.shape>=0)s<<f<<' '<<c.envelope<<' '<<c.envelopePeriod<<' '<<c.shape<<'\n';}
  s<<e.sourceAudio.size()<<'\n';s.write((const char*)e.sourceAudio.data(),e.sourceAudio.size());s<<'\n';
 }
 auto text=s.str();require(text.size()<=256*1024*1024,"Project exceeds 256 MB");return Bytes(text.begin(),text.end());
}
inline Project decodeProject(const Bytes& data,Bank& bank,const Bytes& bankBytes){
 require(data.size()<=256*1024*1024,"Project file is too large");std::istringstream s(std::string(data.begin(),data.end()));std::string magic;int version;uint64_t hash;size_t count;Project p;
 require(bool(s>>magic>>version>>hash>>p.clock>>p.rate>>count)&&magic=="AYFXEDIT_PROJECT"&&(version==1||version==2),"Invalid AYFXEdit project file");
 require(hash==bankFingerprint(bankBytes),"AFB changed outside this project. Open the matching bank/project pair, or move the .ayfx-project file aside and reassign channels.");
 require(count>0&&count<=bank.size(),"Invalid project group count");
 require((p.clock==1789773||p.clock==1773400||p.clock==2000000)&&(p.rate==50||p.rate==60),"Invalid project playback settings");
 for(size_t i=0;i<count;i++){EffectGroup g;require(bool(s>>std::quoted(g.name)>>g.tracks[0]>>g.tracks[1]>>g.tracks[2]>>g.loopStart>>g.loopEnd),"Truncated project group");if(version==2)require(bool(s>>g.priority)&&g.priority>=0&&g.priority<=255,"Invalid priority");p.groups.push_back(g);}
 Bank next=bank;for(auto& e:next){e.tags.clear();e.recipe.clear();e.sourcePath.clear();e.favorite=false;e.sourceAudio.clear();for(auto& c:e.cells){c.envelope=false;c.shape=-1;c.envelopePeriod=1;}}
 if(version==2){size_t tracks;require(bool(s>>tracks)&&tracks==bank.size(),"Project track count mismatch");
  for(auto& e:next){require(bool(s>>e.favorite>>std::quoted(e.tags)>>std::quoted(e.sourcePath)>>std::quoted(e.recipe)),"Truncated metadata");
   require(e.tags.size()<=4096&&e.sourcePath.size()<=32768&&e.recipe.size()<=65536,"Metadata too large");
   size_t events;require(bool(s>>events)&&events<=MaxFrames,"Invalid envelope count");std::array<bool,MaxFrames> seen{};
   for(size_t j=0;j<events;j++){int f,enabled,period,shape;require(bool(s>>f>>enabled>>period>>shape)&&f>=0&&f<MaxFrames&&!seen[f]&&(enabled==0||enabled==1)&&period>=0&&period<=65535&&shape>=-1&&shape<=15,"Invalid envelope event");seen[f]=true;e.cells[f].envelope=enabled!=0;e.cells[f].envelopePeriod=period;e.cells[f].shape=shape;}
   size_t bytes;require(bool(s>>bytes)&&bytes<=64*1024*1024,"Invalid source audio size");require(s.get()=='\n',"Invalid source audio header");e.sourceAudio.resize(bytes);s.read((char*)e.sourceAudio.data(),bytes);require(size_t(s.gcount())==bytes&&s.get()=='\n',"Truncated source audio");
  }
 }
 s>>std::ws;require(s.eof(),"Unexpected project data");validateGroups(next,p.groups);bank=std::move(next);return p;
}
inline std::filesystem::path projectPath(const std::filesystem::path& bank){auto p=bank;p+=L".ayfx-project";return p;}
inline void saveProjectBank(const std::filesystem::path& path,const Bank& bank,const Project& project){
 auto bytes=encodeBank(bank,true),metadata=encodeProject(bank,bytes,project);auto side=projectPath(path);
 auto suffix=temporarySaveSuffix();auto tmp=path;tmp+=suffix;auto tmpSide=side;tmpSide+=suffix;
 require(!std::filesystem::exists(tmp)&&!std::filesystem::exists(tmpSide),"Temporary save file already exists");
 bool existed=std::filesystem::exists(path),replaced=false;Bytes previous;if(existed)previous=read(path);
 try{write(tmp,bytes);write(tmpSide,metadata);
  require(replaceFile(tmp,path),"Cannot replace AFB file; it may be read-only");replaced=true;
  require(replaceFile(tmpSide,side),"Cannot replace companion project file");
 }catch(...){std::error_code error;
  if(replaced){if(existed){write(tmp,previous);require(replaceFile(tmp,path),"Project save failed and AFB rollback failed; previous AFB is in the temporary file");}else std::filesystem::remove(path,error);}
  std::filesystem::remove(tmp,error);std::filesystem::remove(tmpSide,error);throw;
 }
}
// Reassign existing tracks, preserving every track displaced from the edited group.
// -2 requests a new blank channel; -1 leaves that channel unused.
inline int assignChannels(Bank& bank,std::vector<EffectGroup>& groups,int group,const std::string& name,std::array<int,3> tracks,int loopStart=-1,int loopEnd=-1){
 require(group>=0&&group<int(groups.size()),"No effect selected");auto nextBank=bank;auto nextGroups=groups;
 for(int ch=0;ch<3;ch++){require(tracks[ch]>=-2&&tracks[ch]<int(bank.size()),"Invalid channel selection");if(tracks[ch]==-2){require(nextBank.size()<MaxEffects,"AFB supports at most 256 tracks");Effect e;e.name=name+" "+char('A'+ch);require(e.name.size()<=255,"Track name too long");tracks[ch]=int(nextBank.size());nextBank.push_back(e);}}
 std::vector<bool> chosen(nextBank.size());for(int t:tracks)if(t>=0){require(!chosen[t],"Choose a different track for each channel");chosen[t]=true;}
 auto displaced=nextGroups[group].tracks;
 for(auto& g:nextGroups)for(auto& t:g.tracks)if(t>=0&&chosen[t])t=-1;
 int priority=nextGroups[group].priority;nextGroups[group]={name,tracks,loopStart,loopEnd};nextGroups[group].priority=priority;
 for(int t:displaced)if(t>=0&&!chosen[t])nextGroups.push_back({nextBank[t].name,{{t,-1,-1}}});
 int result=0;std::vector<EffectGroup> compact;
 for(size_t i=0;i<nextGroups.size();i++){auto& g=nextGroups[i];if(std::any_of(g.tracks.begin(),g.tracks.end(),[](int t){return t>=0;})){if(int(i)==group)result=int(compact.size());compact.push_back(g);}}
 // Validate the edited group explicitly: an all-empty edit must not disappear.
 require(std::any_of(tracks.begin(),tracks.end(),[](int t){return t>=0;}),"Choose at least one channel");
 validateGroups(nextBank,compact);bank=std::move(nextBank);groups=std::move(compact);return result;
}
inline void removeTrack(Bank& bank,std::vector<EffectGroup>& groups,int track){
 require(bank.size()>1,"Keep at least one track");bank.erase(bank.begin()+track);
 for(auto& g:groups)for(auto& t:g.tracks){if(t==track)t=-1;else if(t>track)--t;}
 groups.erase(std::remove_if(groups.begin(),groups.end(),[](const EffectGroup& g){return std::all_of(g.tracks.begin(),g.tracks.end(),[](int t){return t<0;});}),groups.end());validateGroups(bank,groups);
}
inline bool noiseConflict(const Bank& bank,const EffectGroup& g,int frame){int noise=-1;for(int t:g.tracks)if(t>=0){auto c=bank[t].cells[frame];if(c.n&&(c.volume||c.envelope)){if(noise>=0&&noise!=c.noise)return true;noise=c.noise;}}return false;}
}
