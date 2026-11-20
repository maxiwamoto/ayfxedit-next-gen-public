#include "game_export.h"
#include "register_export.h"
#include "sound_tools.h"
#include "project.h"
#include "audio_decode.h"
#include <iostream>
using namespace ayfx;namespace fs=std::filesystem;
int wmain(int argc,wchar_t** argv){try{
 int fxCount=0,bankCount=0,total=0;
 for(auto& entry:fs::recursive_directory_iterator("native/build/fixtures")){auto p=entry.path();if(p.extension()==".afx"){
  auto b=read(p);size_t used=0;auto e=decode(b,0,b.size(),&used);require(used==b.size(),"Unexpected AFX trailer");auto encoded=encode(e);require(encoded==b,"AFX bytes changed");++fxCount;
 }if(p.extension()==".afb"){
  auto b=decodeBank(read(p));for(bool names:{false,true}){auto encoded=encodeBank(b,names);auto again=decodeBank(encoded);require(b.size()==again.size(),"Bank count changed");for(size_t i=0;i<b.size();i++){require(encode(b[i])==encode(again[i]),"Bank effect changed");if(names)require(b[i].name==again[i].name,"Bank name changed");}}
  total+=int(b.size());++bankCount;
 }}
 Bank many(256);auto packed=encodeBank(many,true);require(packed[0]==0&&decodeBank(packed).size()==256,"256-effect bank failed");
 Effect limit;for(int i=0;i<MaxFrames;i++)limit.cells[i]={i,i&31,15,true,false};auto packedFx=encode(limit);require(encode(decode(packedFx,0,packedFx.size()))==packedFx,"4096-frame effect failed");
 for(auto bad:std::vector<Bytes>{{},{0x20},{0x40},{0},{0xd0}}){bool threw=false;try{decode(bad,0,bad.size());}catch(...){threw=true;}require(threw,"Truncated AFX accepted");}
 bool overflow=false;try{Bank large(12,limit);encodeBank(large,false);}catch(...){overflow=true;}require(overflow,"Bank offset overflow accepted");
 Effect note;for(int i=0;i<10;i++)note.cells[i]={254,0,15,true,false};
 auto wav=render(note,1789773,50);require(u32(wav,40)==13*882*2,"WAV length wrong");require(u32(wav,24)==44100&&u16(wav,34)==16,"WAV header wrong");
 auto nt=render(note,1789773,60);require(u32(nt,40)==13*735*2,"60 Hz duration wrong");
 auto tail=render(note,1789773,50,4000);require(u32(tail,40)==3*882*2,"Playback past end failed");
 int crossings=0;for(size_t p=46;p+2<wav.size()-3*882*2;p+=2)if(int16_t(u16(wav,p))>10000&&int16_t(u16(wav,p-2))<=10000)crossings++;
 require(crossings>=85&&crossings<=90,"AY tone frequency incorrect");
 Bytes psg(16);psg[0]='P';psg[1]='S';psg[2]='G';for(int b:{0,254,1,0,7,62,8,15,255,254,2,253})psg.push_back(uint8_t(b));
 auto imported=importPSG(psg,-1);require(length(imported)==9&&imported.cells[0].tone==254&&imported.cells[0].t&&!imported.cells[0].n,"PSG import failed");
 // Verify WAV parser with an odd-length ancillary chunk and stereo samples.
 Bytes stereo;text(stereo,"RIFF");dword(stereo,0);text(stereo,"WAVEJUNK");dword(stereo,1);stereo.push_back(42);stereo.push_back(0);text(stereo,"fmt ");dword(stereo,16);word(stereo,1);word(stereo,2);dword(stereo,22050);dword(stereo,88200);word(stereo,4);word(stereo,16);text(stereo,"data");dword(stereo,4410*4);
 for(int i=0;i<4410;i++){int value=int(16000*std::sin(i*2*3.141592653589793*440/22050));word(stereo,value);word(stereo,value);}
 auto converted=importWAV(stereo,1789773,50);require(length(converted)==10,"Stereo WAV duration incorrect");require(converted.cells[2].tone>240&&converted.cells[2].tone<270&&converted.cells[2].t,"WAV pitch incorrect");

 // Independent expected counter schedule for all 32 noise register values.
 for(int n=0;n<32;n++){Chip chip;Cell c{1,n,15,false,true};unsigned expected=1;int period=2*std::max(1,n);
  for(int tick=1;tick<=period*200;tick++){chip.tick(c);if(tick%period==0){if(expected&1)expected^=0x24000;expected>>=1;}require(chip.random==expected,"Noise divider/LFSR mismatch");}}
 unsigned lfsr=1;std::vector<bool> seen(131072,false);for(int i=0;i<131071;i++){require(lfsr&&!seen[lfsr],"Noise sequence repeats too soon");seen[lfsr]=true;if(lfsr&1)lfsr^=0x24000;lfsr>>=1;}require(lfsr==1,"Noise LFSR period incorrect");
 for(int v=2;v<16;v++)require(std::abs(20*std::log10(amplitude(v)/amplitude(v-1))-3.0102999566)<1e-6,"Volume step is not 3 dB");
 require(amplitude(0)==0,"Level zero is not silent");
 auto rms=[](const Bytes& data){double mean=0,power=0;int n=0;for(size_t p=44+882*2;p<44+882*9*2;p+=2){double v=int16_t(u16(data,p));mean+=v;power+=v*v;n++;}return std::sqrt(power/n-(mean/n)*(mean/n));};
 double full=rms(wav);for(int v:{1,4,8,12}){Effect quiet=note;for(int i=0;i<10;i++)quiet.cells[i].volume=v;auto q=render(quiet,1789773,50);require(std::abs(rms(q)/full-amplitude(v))<.002,"Rendered volume ratio differs");}
 Effect ultrasonic=note;for(int i=0;i<10;i++)ultrasonic.cells[i].tone=1;
 auto filtered=render(ultrasonic,1789773,50),legacy=render(ultrasonic,1789773,50,0,PreviewModel::Legacy);
 require(rms(filtered)<rms(legacy)*.01,"Ultrasonic alias suppression failed");
 std::cout<<"PASS: all noise periods, 131071-state LFSR cycle, logarithmic volume/RMS ratios, and >40 dB ultrasonic alias reduction.\n";
 // Grouping must preserve bank bytes and resolve channels independently of track order.
 Bank grouped(4);grouped[0].name="0F_C_fire_whip";grouped[1].name="0F_A_fire_whip";grouped[2].name="30_B_effect";grouped[3].name="Untitled";
 auto groupBytes=encodeBank(grouped,true);auto groups=groupBank(grouped);
 require(groups.size()==3&&groups[0].tracks[0]==1&&groups[0].tracks[1]==-1&&groups[0].tracks[2]==0,"Channel grouping failed");
 require(encodeBank(grouped,true)==groupBytes,"Grouping changed the bank");
 grouped.push_back(grouped[0]);require(groupBank(grouped).size()==4,"Duplicate channel was silently lost");
 // Identical noise channels must share one sequence, not double-clock it.
 Effect noiseFx;for(int i=0;i<10;i++)noiseFx.cells[i]={0,7,15,false,true};
 std::array<const Effect*,3> channels{{&noiseFx,nullptr,&noiseFx}};
 auto together=renderChannels(channels,1789773,60),solo=renderChannels(channels,1789773,60,0,10000,1);
 for(size_t p=44;p<together.size();p+=2)require(std::abs(int(int16_t(u16(together,p)))-2*int(int16_t(u16(solo,p))))<=1,"Channels do not share the same noise phase/gain");
 Effect conflict=noiseFx;conflict.cells[5].noise=8;bool rejected=false;try{renderChannels({&noiseFx,&conflict,nullptr});}catch(...){rejected=true;}require(rejected,"Conflicting AY noise periods accepted");
 Effect longTone=note;longTone.cells[19]={127,0,12,true,false};auto both=renderChannels({&note,&longTone,nullptr},1789773,60);
 require(u32(both,40)==23*735*2,"Group duration ignored longer channel");
 auto toneA=renderChannels({&note,&longTone,nullptr},1789773,60,0,10000,1),toneB=renderChannels({&note,&longTone,nullptr},1789773,60,0,10000,2);
 for(size_t p=44;p<both.size();p+=2)require(std::abs(int(int16_t(u16(both,p)))-int(int16_t(u16(toneA,p)))-int(int16_t(u16(toneB,p))))<=1,"Independent tones did not mix correctly");
 std::cout<<"PASS: grouped channels, unchanged bank bytes, duplicate handling, synchronized noise, solo gain, independent tones, and group duration.\n";
 // Explicit grouping works with arbitrary names and survives renaming/save/reload.
 Bank generic(4);generic[0]=note;generic[0].name="Zap body";generic[1]=noiseFx;generic[1].name="Air burst";generic[2]=longTone;generic[2].name="Low ring";generic[3].name="Unused";
 auto genericGroups=groupBank(generic);int target=assignChannels(generic,genericGroups,0,"Laser \"shot\"",{{0,2,1}},2,10);
 require(genericGroups.size()==2&&genericGroups[target].tracks[1]==2,"Generic assignment failed");
 generic[0].name="Renamed freely";Project project{genericGroups,1789773,60};auto bankData=encodeBank(generic,true),projectData=encodeProject(generic,bankData,project);
 auto restored=decodeProject(projectData,generic,bankData);require(restored.groups[0].name=="Laser \"shot\""&&restored.groups[0].tracks==genericGroups[0].tracks&&restored.groups[0].loopStart==2&&restored.groups[0].loopEnd==10,"Project roundtrip failed");
 auto modified=bankData;modified.back()^=1;bool stale=false;try{decodeProject(projectData,generic,modified);}catch(...){stale=true;}require(stale,"Stale project accepted");
 auto beforeGroups=encodeProject(generic,bankData,project);bool duplicate=false;try{assignChannels(generic,genericGroups,0,"Bad",{{0,0,-1}});}catch(...){duplicate=true;}require(duplicate&&encodeBank(generic,true)==bankData,"Invalid assignment mutated data");
 target=assignChannels(generic,genericGroups,0,"Split",{{0,-2,-1}});require(generic.size()==5&&genericGroups[target].tracks[1]==4&&genericGroups.size()==4,"Displaced or blank tracks lost");
 removeTrack(generic,genericGroups,1);validateGroups(generic,genericGroups);require(generic.size()==4,"Track removal failed");
 auto path=fs::path("native/build/test-generic.afb");Project saved{genericGroups,2000000,50};saveProjectBank(path,generic,saved);
 auto savedBytes=read(path);auto loadedBank=decodeBank(savedBytes);auto loadedProject=decodeProject(read(projectPath(path)),loadedBank,savedBytes);require(loadedProject.clock==2000000&&loadedProject.groups.size()==genericGroups.size(),"Saved project failed");
 // If the companion cannot be replaced, the old AFB must be restored.
 auto rollback=fs::path("native/build/test-rollback.afb");write(rollback,bankData);fs::create_directory(projectPath(rollback));bool saveFailed=false;
 try{saveProjectBank(rollback,generic,saved);}catch(...){saveFailed=true;}require(saveFailed&&read(rollback)==bankData,"Failed project save did not restore AFB");fs::remove(projectPath(rollback));
 auto loop=renderChannels({&note,nullptr,nullptr},1789773,60,2,10000,7,7,0);require(u32(loop,40)==5*735*2,"Loop segment includes unwanted tail");
 auto muted=renderChannels({&note,&longTone,nullptr},1789773,60,0,10000,0);for(size_t p=44;p<muted.size();p+=2)require(u16(muted,p)==0,"Muted channels are audible");
 Bank conflicts{noiseFx,conflict};EffectGroup conflictGroup{"Test",{{0,-1,1}}};require(!noiseConflict(conflicts,conflictGroup,4)&&noiseConflict(conflicts,conflictGroup,5),"Conflict frame highlighting incorrect");
 std::cout<<"PASS: generic groups, project persistence, stale-file rejection, save rollback, blank/displaced tracks, exact loops, mutes, and conflict frames.\n";
 // Workshop conversion is tested with known signals rather than subjective claims.
 SourceWave source;source.rate=44100;source.channels=1;source.bits=16;source.mono.resize(44100/2);
 for(size_t i=0;i<source.mono.size();i++)source.mono[i]=float(.5*std::sin(2*Pi*440*i/44100));
 auto sourceBytes=pcmWave(source.mono);write("native/build/test-workshop-source.wav",sourceBytes);auto decodedSource=decodeSourceWave(sourceBytes);require(decodedSource.mono.size()==source.mono.size(),"Workshop WAV decode changed length");
 auto decodedStereo=decodeSourceWave(stereo);require(decodedStereo.channels==2&&decodedStereo.mono.size()==4410,"Workshop stereo WAV decode failed");
 ConvertOptions options;options.start=.1;options.end=.4;options.channels=1;auto toneResult=convertWave(source,options);
 require(toneResult.frames==18&&toneResult.tracks[0].cells[8].t&&std::abs(toneResult.tracks[0].cells[8].tone-254)<=2,"Workshop tone/trim detection failed");
 auto resultWav=convertedPreview(toneResult,options);require(u32(resultWav,40)==18*735*2,"Workshop duration drifted");
 options.pitch=12;auto up=convertWave(source,options);require(std::abs(up.tracks[0].cells[8].tone-127)<=1,"Workshop pitch control failed");options.pitch=0;
 options.attackMs=100;options.decayMs=100;auto faded=convertWave(source,options);require(faded.tracks[0].cells[0].volume<faded.tracks[0].cells[8].volume&&faded.tracks[0].cells[17].volume<faded.tracks[0].cells[8].volume,"Workshop fades failed");options.attackMs=options.decayMs=0;
 auto quietSource=source;for(auto& sample:quietSource.mono)sample*=.01f;options.normalize=false;auto quietConversion=convertWave(quietSource,options);require(quietConversion.tracks[0].cells[8].volume<toneResult.tracks[0].cells[8].volume,"Normalization off lost original dynamics");options.normalize=true;
 SourceWave mixture=source;for(size_t i=0;i<mixture.mono.size();i++)mixture.mono[i]=float(.3*std::sin(2*Pi*440*i/44100)+.3*std::sin(2*Pi*660*i/44100));options.channels=2;
 auto dual=convertWave(mixture,options);auto c0=dual.tracks[0].cells[8],c1=dual.tracks[1].cells[8];require(c0.t&&c1.t&&std::abs(c0.tone-c1.tone)>60,"Two-tone conversion collapsed channels");
 SourceWave noiseSource=source;uint32_t rng=1234567;for(auto& sample:noiseSource.mono){rng=rng*1664525u+1013904223u;sample=float((int(rng>>16)-32768)/65536.);}
 options.channels=1;auto white=convertWave(noiseSource,options);auto lowSource=noiseSource;float filteredNoise=0;for(auto& sample:lowSource.mono){filteredNoise+=.03f*(sample-filteredNoise);sample=filteredNoise;}
 auto low=convertWave(lowSource,options);int whiteN=0,lowN=0;for(int f=4;f<14;f++){whiteN+=white.tracks[0].cells[f].noise;lowN+=low.tracks[0].cells[f].noise;}require(white.tracks[0].cells[8].n&&lowN>whiteN,"Noise bandwidth fitting failed");
 options.brightness=75;auto bright=convertWave(lowSource,options);require(bright.tracks[0].cells[8].noise<low.tracks[0].cells[8].noise,"Brightness control failed");options.brightness=0;
 options.channels=3;auto threeSource=mixture;for(size_t i=0;i<threeSource.mono.size();i++)threeSource.mono[i]=threeSource.mono[i]*.5f+noiseSource.mono[i];auto three=convertWave(threeSource,options);
 bool hasTone=false,hasNoise=false;for(auto& e:three.tracks){hasTone|=e.cells[8].t;hasNoise|=e.cells[8].n;}require(hasTone&&hasNoise,"Three-channel tone/noise decomposition failed");
 auto silentSource=source;std::fill(silentSource.mono.begin(),silentSource.mono.end(),0.f);auto silent=convertWave(silentSource,options);for(auto& e:silent.tracks)require(length(e)==0,"Silence generated sound");
 bool cancelled=false;try{convertWave(source,options,[](){return true;});}catch(...){cancelled=true;}require(cancelled,"Conversion cancellation ignored");
 auto badOptions=options;badOptions.end=badOptions.start;bool badTrim=false;try{convertWave(source,badOptions);}catch(...){badTrim=true;}require(badTrim,"Empty trim accepted");
 Bank destination(1);destination[0].name="Keep my name";auto destGroups=groupBank(destination);auto savedDestination=encodeBank(destination,true);int added=applyConverted(destination,destGroups,0,dual,"Custom sound",false);require(destination.size()==3&&destGroups.size()==2&&destination[0].name=="Keep my name"&&added==1,"New conversion replaced existing data");
 applyConverted(destination,destGroups,added,toneResult,"Replacement",true);require(destGroups[added].tracks[1]>=0&&length(destination[destGroups[added].tracks[1]])==0&&destination[destGroups[added].tracks[0]].name=="Custom sound A","Replace did not preserve names/silence spare channels");
 require(u32(seekWave(sourceBytes,.1),40)==uint32_t((source.mono.size()-4410)*2),"A/B seeking changed position");
 std::cout<<"PASS: WAV workshop pitch, trim, timing, fades, normalization, multiple tones, noise bandwidth, brightness, silence, cancellation, application, and A/B seeking.\n";
 // Exercise Windows MP3 decoding with an optional real local fixture.
 write("native/build/test-invalid.mp3",Bytes{'b','a','d'});
 bool rejectedMP3=false;try{decodeAudioFile(L"native/build/test-invalid.mp3");}catch(...){rejectedMP3=true;}require(rejectedMP3,"Malformed MP3 accepted");
 if(argc>1){
  auto mp3=decodeAudioFile(argv[1]);require(mp3.duration()>0&&mp3.rate>=4000&&mp3.bits==16,"MP3 decoded format invalid");
  double energy=0;for(float sample:mp3.mono){require(std::isfinite(sample)&&std::abs(sample)<=1,"MP3 sample invalid");energy+=sample*sample;}require(energy>1e-5,"MP3 decoded to silence");
  ConvertOptions mp3Options;mp3Options.end=std::min(.5,mp3.duration());auto importedMP3=convertWave(mp3,mp3Options);require(importedMP3.frames==int(std::ceil(mp3Options.end*60-1e-8)),"MP3 conversion timing wrong");
  auto unicodePath=fs::path(L"native/build/test-audio-\u97f3\u58f0.MP3");fs::copy_file(argv[1],unicodePath,fs::copy_options::overwrite_existing);
  auto unicodeAudio=decodeAudioFile(unicodePath);require(unicodeAudio.mono==mp3.mono,"Unicode/uppercase MP3 import changed audio");fs::remove(unicodePath);
  std::cout<<"PASS: MP3 decode, non-silent samples, conversion, Unicode paths and uppercase extension ("<<mp3.rate<<" Hz, "<<mp3.channels<<" channels, "<<mp3.duration()<<" s).\n";
 }
 {
 Bank fixture(6);std::vector<EffectGroup> gs={ {"One",{{0,-1,-1}}},{"Two",{{1,2,-1}}},{"Three",{{3,4,5}}} };
 for(int t=0;t<6;t++)for(int f=0;f<12;f++)fixture[t].cells[f]={100+t*40+f,3,15-f/3,true,t==5};
 gs[2].loopStart=3;gs[2].loopEnd=12;fixture[3].cells[0].envelope=true;fixture[3].cells[0].shape=10;fixture[3].cells[0].envelopePeriod=100;
 // VGM -> editable 3-channel group -> compact SDK -> compiled Z80 tests.
 // Consecutive identical shape writes must still retrigger the envelope.
 Bytes importedVGM(0x100);importedVGM[0]='V';importedVGM[1]='g';importedVGM[2]='m';importedVGM[3]=' ';importedVGM[8]=0x71;importedVGM[9]=1;importedVGM[0x34]=0xcc;
 for(int i=0;i<4;i++)importedVGM[0x74+i]=uint8_t(1789773>>(8*i));
 for(int f=0;f<12;f++){
  auto reg=[&](int r,int v){importedVGM.insert(importedVGM.end(),{0xa0,uint8_t(r),uint8_t(v)});};
  for(int ch=0;ch<3;ch++){reg(ch*2,100+ch*30+f);reg(ch*2+1,ch);reg(ch+8,ch==1?16:15-ch);}
  reg(6,f+1);reg(7,0);reg(11,f<6?40:80);reg(12,0);if(f==0||f==5||f==6)reg(13,10);importedVGM.push_back(0x63);
 }
 importedVGM.push_back(0x66);auto vgmCapture=captureVGM(importedVGM,50);auto vgmEffect=extractCapture(vgmCapture,0,12,7);
 require(vgmEffect.tracks.size()==3&&vgmEffect.tracks[1].cells[5].shape==10&&vgmEffect.tracks[1].cells[6].shape==10,"VGM envelope retrigger lost before compact export");
 fixture.insert(fixture.end(),vgmEffect.tracks.begin(),vgmEffect.tracks.end());gs.push_back({"Imported VGM",{{6,7,8}}});fixture.insert(fixture.end(),vgmEffect.tracks.begin(),vgmEffect.tracks.end());gs.push_back({"Duplicate VGM",{{9,10,11}}});
 auto dir=std::filesystem::path("native/build/sdk-test");std::filesystem::remove_all(dir);
 auto result=exportBankData(dir,fixture,{gs,1789773,50});require(result.effects==5,"SDK fixture failed");auto compactBytes=read(dir/"effects.bin");require(u16(compactBytes,6)==u16(compactBytes,8),"Identical compact effects were not shared");
 for(size_t g=0;g<gs.size();g++){std::array<const Effect*,3> channels{};for(int ch=0;ch<3;ch++)if(gs[g].tracks[ch]>=0)channels[ch]=&fixture[gs[g].tracks[ch]];Bytes expected;for(auto r:registerFrames(channels,12))expected.insert(expected.end(),r.begin(),r.end());write(dir/("expected"+std::to_string(g)+".bin"),expected);}
 auto rawAY=encodeRegisters(fixture,gs[3],1789773,50,false);require(rawAY.data.size()==12*14,"AY14 frame count wrong");
 require(rawAY.data[13]==10&&rawAY.data[14+13]==255&&rawAY.data[5*14+13]==10&&rawAY.data[6*14+13]==10,"AY14 envelope retrigger lost");
 bool rejectedPSG11=false;try{encodeRegisters(fixture,gs[3],1789773,50,true);}catch(...){rejectedPSG11=true;}require(rejectedPSG11,"PSG11 silently discarded hardware envelopes");
 auto rawPSG=encodeRegisters(fixture,gs[1],1789773,60,true);auto expectedPSG=registerFrames({&fixture[1],&fixture[2],nullptr},12);require(rawPSG.data.size()==12*11,"PSG11 frame count wrong");
 for(int f=0;f<12;f++)for(int r=0;r<11;r++)require(rawPSG.data[f*11+r]==(r==7?(expectedPSG[f][r]|128):expectedPSG[f][r]),"PSG11 registers changed");
 auto exportedLoop=encodeRegisters(fixture,gs[2],1789773,50,false);require(exportedLoop.metadata.find("\"loop_start\": 3")!=std::string::npos&&exportedLoop.data.size()==12*14,"Register loop metadata wrong");
 auto named=gs[1];named.name="Quote \" and newline\n";auto escaped=encodeRegisters(fixture,named,1789773,60,true);require(escaped.metadata.find("\\u000a")!=std::string::npos,"JSON name escaping wrong");
 exportRegisters(dir/"imported-vgm.ayregs",fixture,gs[3],1789773,50);exportRegisters(dir/"two-channel.psg11",fixture,gs[1],1789773,60);
 std::cout<<"PASS: AY14/PSG11 export, exact channel frames, envelope events, envelope rejection, loops and metadata escaping.\n";
 fixture[0].tags="test metal";fixture[0].favorite=true;fixture[0].sourceAudio=Bytes{1,2,3};fixture[0].recipe="settings";
 auto raw=encodeBank(fixture,true);auto extra=encodeProject(fixture,raw,{gs,1789773,50});auto restored=decodeBank(raw);auto restoredProject=decodeProject(extra,restored,raw);
 require(restored[0].tags==fixture[0].tags&&restored[0].sourceAudio==fixture[0].sourceAudio&&restored[3].cells[0].shape==10,"Extended metadata lost");
 auto generated=generateSound({1,30,3});require(generated.tracks.size()==3&&generated.frames==30,"Generator channels failed");
 applyCurve(generated.tracks[0],0,29,0,{{0,1},{1,0}});require(generated.tracks[0].cells[0].volume==15&&generated.tracks[0].cells[29].volume==0,"Curve endpoints failed");

 auto input=registerFrames({&fixture[0],nullptr,nullptr},12);std::vector<Registers> repeated;for(int i=0;i<20;i++)repeated.insert(repeated.end(),input.begin(),input.end());
 AYStream loop(input,1789773,50,7,10000,0,0,12),linearStream(repeated,1789773,50);for(int i=0;i<30000;i++)require(loop.next()==linearStream.next(),"Loop reset chip/filter phase");
 for(int shape=0;shape<16;shape++){EnvelopeGenerator env;env.setPeriod(2);env.trigger(shape);require(env.level()==((shape&4)?0:15),"Envelope initial attack wrong");for(int i=0;i<400;i++){env.tick();require(env.level()>=0&&env.level()<=15,"Envelope range invalid");}if(!(shape&8))require(env.level()==0,"One-shot envelope did not decay to zero");}
 Bytes psg(16);psg[0]='P';psg[1]='S';psg[2]='G';psg.insert(psg.end(),{0,100,7,62,8,16,11,20,13,10,255,255,253});auto capture=capturePSG(psg,50);require(capture.frames.size()==2&&capture.frames[0][13]==10&&capture.frames[1][13]==255,"PSG retrigger events lost");auto extracted=extractCapture(capture,1,2,1);require(extracted.tracks[0].cells[0].shape==10&&extracted.tracks[0].cells[0].envelope,"Capture selection lost envelope state");
 Bytes vgm(0x100);vgm[0]='V';vgm[1]='g';vgm[2]='m';vgm[3]=' ';vgm[8]=0x71;vgm[9]=1;vgm[0x34]=0xcc;int hz=1789773;for(int i=0;i<4;i++)vgm[0x74+i]=(hz>>(8*i))&255;vgm.insert(vgm.end(),{0xa0,0,100,0xa0,7,62,0xa0,8,15,0x63,0x66});auto vc=captureVGM(vgm,50);require(vc.frames.size()==1&&vc.frames[0][0]==100,"VGM extraction failed");bool rejected=false;vgm.pop_back();try{captureVGM(vgm,50);}catch(...){rejected=true;}require(rejected,"Truncated VGM accepted");
 bool classicRejected=false;try{requireClassic(fixture[3]);}catch(...){classicRejected=true;}require(classicRejected,"Envelope silently exported as classic AFX");
 std::cout<<"PASS: extended metadata, all envelope shapes, continuous loops, PSG/VGM extraction, generators, curves and SDK fixtures.\n";

 }
 write("native/build/test-tone.wav",wav);
 std::cout<<"PASS: "<<fxCount<<" byte-exact AFX files; "<<bankCount<<" banks / "<<total<<" effects round-trip; 256 effects, 4096 frames, malformed input, bank overflow, AY frequency, 50/60 Hz, stereo WAV and PSG imports.\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}return 0;}
