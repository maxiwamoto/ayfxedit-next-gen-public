// Browser adapter only. AY rendering and file codecs stay in the shared native engine.
#include <emscripten/bind.h>
#include "sound_tools.h"
#include "game_export.h"
#include "register_export.h"
using namespace ayfx;
using emscripten::val;
namespace fs=std::filesystem;
struct Document {Bank bank;Project project;};
static std::unique_ptr<AYStream> stream;
static int remaining=0;
static int number(val v,const char* key,int low,int high){double d=v[key].as<double>();require(std::isfinite(d)&&d==std::floor(d)&&d>=low&&d<=high,"Invalid numeric setting");return int(d);}
static Bytes bytes(val v,size_t max=64*1024*1024){require(v["length"].as<size_t>()<=max,"File is too large");return emscripten::convertJSArrayToNumberVector<uint8_t>(v);}
static val arrayBytes(const Bytes& b){return val::global("Uint8Array").new_(val(emscripten::typed_memory_view(b.size(),b.data())));}
static Document readDocument(val v){
 Document d;auto tracks=v["tracks"];int n=tracks["length"].as<int>();require(n>0&&n<=MaxEffects,"Use 1-256 tracks");d.bank.resize(n);
 for(int t=0;t<n;t++){auto x=tracks[t];auto& e=d.bank[t];e.name=x["name"].as<std::string>();require(e.name.size()<=255&&e.name.find('\0')==std::string::npos,"Track name is too long or invalid");e.tags=x["tags"].as<std::string>();e.favorite=x["favorite"].as<bool>();e.sourcePath=x["sourcePath"].as<std::string>();e.recipe=x["recipe"].as<std::string>();e.sourceAudio=bytes(x["sourceAudio"]);
  auto cells=x["cells"];int len=cells["length"].as<int>();require(len>=0&&len<=MaxFrames,"A track supports up to 4096 frames");
  for(int f=0;f<len;f++){auto a=cells[f];require(a["length"].as<int>()==8,"Invalid frame record");auto integer=[&](int i,int lo,int hi){double q=a[i].as<double>();require(std::isfinite(q)&&q==std::floor(q)&&q>=lo&&q<=hi,"Frame value is out of range");return int(q);};auto& c=e.cells[f];c={integer(0,0,4095),integer(1,0,31),integer(2,0,15),bool(integer(3,0,1)),bool(integer(4,0,1)),bool(integer(5,0,1)),integer(6,0,65535),integer(7,-1,15)};}
 }
 d.project.clock=number(v,"clock",1000000,4000000);require(d.project.clock==1789773||d.project.clock==1773400||d.project.clock==2000000,"Unsupported project clock");d.project.rate=number(v,"rate",50,60);require(d.project.rate==50||d.project.rate==60,"Choose 50 or 60 Hz");auto groups=v["groups"];n=groups["length"].as<int>();require(n>0&&n<=int(d.bank.size()),"Invalid groups");
 for(int i=0;i<n;i++){auto x=groups[i];EffectGroup g;g.name=x["name"].as<std::string>();for(int c=0;c<3;c++)g.tracks[c]=x["tracks"][c].as<int>();g.loopStart=number(x,"loopStart",-1,4095);g.loopEnd=number(x,"loopEnd",-1,4096);g.priority=number(x,"priority",0,255);d.project.groups.push_back(g);}validateGroups(d.bank,d.project.groups);return d;
}
static val document(const Bank& bank,const Project& project){
 val out=val::object(),tracks=val::array(),groups=val::array();
 for(size_t t=0;t<bank.size();t++){auto& e=bank[t];auto x=val::object(),cells=val::array();int len=0;for(int f=0;f<MaxFrames;f++){auto& c=e.cells[f];if(c.tone||c.noise||c.volume||c.t||c.n||c.envelope||c.envelopePeriod!=1||c.shape!=-1)len=f+1;}
  for(int f=0;f<len;f++){auto& c=e.cells[f];auto a=val::array();int values[]={c.tone,c.noise,c.volume,int(c.t),int(c.n),int(c.envelope),c.envelopePeriod,c.shape};for(int i=0;i<8;i++)a.set(i,values[i]);cells.set(f,a);}
  x.set("name",e.name);x.set("cells",cells);x.set("tags",e.tags);x.set("favorite",e.favorite);x.set("sourcePath",e.sourcePath);x.set("recipe",e.recipe);x.set("sourceAudio",arrayBytes(e.sourceAudio));tracks.set(t,x);
 }
 for(size_t i=0;i<project.groups.size();i++){auto& g=project.groups[i];auto x=val::object(),a=val::array();for(int c=0;c<3;c++)a.set(c,g.tracks[c]);x.set("tracks",a);x.set("name",g.name);x.set("loopStart",g.loopStart);x.set("loopEnd",g.loopEnd);x.set("priority",g.priority);groups.set(i,x);}
 out.set("tracks",tracks);out.set("groups",groups);out.set("clock",project.clock);out.set("rate",project.rate);return out;
}
static val run(std::string op,val a){
 try{val result=val::object(),out=val::object();
 if(op=="decode"){
  auto raw=bytes(a["bytes"]);auto ext=a["extension"].as<std::string>();Bank bank;if(ext=="afx"){bank={decode(raw,0,raw.size())};bank[0].name="Imported effect";}else bank=decodeBank(raw);Project p{groupBank(bank),1789773,50};if(!a["side"].isNull()&&!a["side"].isUndefined())p=decodeProject(bytes(a["side"],256*1024*1024),bank,raw);out=document(bank,p);
 }else if(op=="generate"){
  GeneratorOptions o;o.preset=number(a,"preset",0,5);o.frames=number(a,"frames",1,4096);o.channels=number(a,"channels",1,3);o.clock=number(a,"clock",1000000,4000000);o.startHz=a["startHz"].as<double>();o.endHz=a["endHz"].as<double>();o.noise=a["noise"].as<double>();auto r=generateSound(o);EffectGroup g;g.name=a["name"].as<std::string>();for(int c=0;c<o.channels;c++){g.tracks[c]=c;r.tracks[c].name=g.name+" "+char('A'+c);}out=document(r.tracks,Project{{g},o.clock,number(a,"rate",50,60)});
 }else if(op=="convert"){
  SourceWave w;w.rate=number(a,"sampleRate",4000,192000);w.channels=1;w.bits=32;require(a["samples"]["length"].as<size_t>()<=size_t(w.rate)*300,"Audio exceeds five minutes");w.mono=emscripten::convertJSArrayToNumberVector<float>(a["samples"]);for(float v:w.mono)require(std::isfinite(v)&&std::abs(v)<=2,"Invalid audio samples");ConvertOptions o;o.start=a["start"].as<double>();o.end=a["end"].as<double>();o.channels=number(a,"channels",1,3);o.clock=number(a,"clock",1000000,4000000);o.rate=number(a,"rate",50,60);o.attackMs=a["attack"].as<double>();o.decayMs=a["decay"].as<double>();o.pitch=a["pitch"].as<double>();o.brightness=a["brightness"].as<double>();o.normalize=a["normalize"].as<bool>();o.toneSensitivity=a["tone"].as<double>();o.noiseSensitivity=a["noise"].as<double>();auto r=convertWave(w,o);EffectGroup g;g.name=a["name"].as<std::string>();for(int c=0;c<o.channels;c++){g.tracks[c]=c;r.tracks[c].name=g.name+" "+char('A'+c);}r.tracks[0].sourceAudio=pcmWave(selectedSamples(w,o.start,o.end,SampleRate));r.tracks[0].sourcePath=g.name;std::ostringstream recipe;recipe<<o.channels<<' '<<o.attackMs<<' '<<o.decayMs<<' '<<o.brightness<<' '<<o.pitch<<' '<<o.normalize<<' '<<o.toneSensitivity<<' '<<o.noiseSensitivity;r.tracks[0].recipe=recipe.str();out=document(r.tracks,Project{{g},o.clock,o.rate});
 }else if(op=="capture"){
  auto raw=bytes(a["bytes"]);int rate=number(a,"rate",50,60);auto cap=a["extension"].as<std::string>()=="vgm"?captureVGM(raw,rate):capturePSG(raw,rate);if(a["extension"].as<std::string>()!="vgm")cap.clock=number(a,"clock",1000000,4000000);require(cap.frames.size()<=4096,"Capture exceeds 4096 frames; trim it in the desktop editor first");auto r=extractCapture(cap,0,int(cap.frames.size()),7);out=document(r.tracks,Project{{EffectGroup{"Capture",{{0,1,2}},-1,-1}},cap.clock,rate});
 }else if(op=="streamNext"){
  require(bool(stream),"No active playback");int n=number(a,"count",1,44100);if(remaining>=0)n=std::min(n,remaining);std::vector<float> samples(n);for(auto& v:samples)v=stream->next()/32768.f;if(remaining>=0)remaining-=n;out.set("samples",val::global("Float32Array").new_(val(emscripten::typed_memory_view(samples.size(),samples.data()))));out.set("ended",remaining==0);
 }else if(op=="stop"){stream.reset();}
 else{
  auto d=readDocument(a["doc"]);int index=number(a,"group",0,int(d.project.groups.size())-1);auto& g=d.project.groups[index];std::array<const Effect*,3> channels{};int len=1,count=0;for(int c=0;c<3;c++)if(g.tracks[c]>=0){channels[c]=&d.bank[g.tracks[c]];count++;len=std::max(len,length(*channels[c]));}if(g.loopStart>=0)len=g.loopEnd;
  if(op=="save"){auto bank=encodeBank(d.bank,true);out.set("bank",arrayBytes(bank));out.set("project",arrayBytes(encodeProject(d.bank,bank,d.project)));}
  else if(op=="render"){out=arrayBytes(renderChannels(channels,d.project.clock,d.project.rate,0,count==1?21845:10000,number(a,"mask",0,7),len,3));}
  else if(op=="streamStart"){int from=number(a,"from",0,4095);if(from>=len)from=0;stream=std::make_unique<AYStream>(registerFrames(channels,len),d.project.clock,d.project.rate,number(a,"mask",0,7),count==1?21845:10000,from,g.loopStart,g.loopEnd);remaining=g.loopStart>=0?-1:(len-from+3)*SampleRate/d.project.rate;out.set("frames",len);out.set("from",from);}
  else if(op=="registers"){auto r=encodeRegisters(d.bank,g,d.project.clock,d.project.rate,false);out.set("data",arrayBytes(r.data));out.set("metadata",r.metadata);}
  else if(op=="classic"){int ch=number(a,"channel",0,2);require(g.tracks[ch]>=0,"Channel is empty");auto& e=d.bank[g.tracks[ch]];requireClassic(e);out=arrayBytes(encode(e));}
  else if(op=="sdk"){static unsigned id=0;auto folder=fs::path("/export-"+std::to_string(++id));exportBankData(folder,d.bank,d.project);for(auto& f:fs::directory_iterator(folder))if(f.is_regular_file())out.set(f.path().filename().string(),arrayBytes(read(f.path())));fs::remove_all(folder);}
  else if(op!="validate")throw std::runtime_error("Unknown engine operation");
 }
 result.set("ok",true);result.set("data",out);return result;
 }catch(const std::exception& e){auto r=val::object();r.set("ok",false);r.set("error",std::string(e.what()));return r;}
}
EMSCRIPTEN_BINDINGS(ayfx_web){emscripten::function("run",&run);}
