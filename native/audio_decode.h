#pragma once
#include "workshop.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"ole32.lib")
namespace ayfx {
inline void audioCheck(HRESULT hr,const char* operation){
 if(FAILED(hr)){char code[24];sprintf_s(code," (0x%08lX)",static_cast<unsigned long>(hr));throw std::runtime_error(std::string(operation)+code);}
}
// Keep COM and Media Foundation alive until all reader/sample objects are released.
struct AudioPlatform {
 HRESULT com;bool started=false;
 AudioPlatform():com(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE)){
  if(FAILED(com)&&com!=RPC_E_CHANGED_MODE)audioCheck(com,"Cannot initialize Windows audio");
  HRESULT hr=MFStartup(MF_VERSION);
  if(FAILED(hr)){if(SUCCEEDED(com))CoUninitialize();audioCheck(hr,"Windows Media Foundation is unavailable (Windows N requires the Media Feature Pack)");}
  started=true;
 }
 ~AudioPlatform(){if(started)MFShutdown();if(SUCCEEDED(com))CoUninitialize();}
};
inline SourceWave decodeMP3(const std::filesystem::path& path){
 using Microsoft::WRL::ComPtr;
 require(std::filesystem::is_regular_file(path),"Choose a local MP3 file");
 require(std::filesystem::file_size(path)<=64*1024*1024,"Audio input exceeds 64 MB");
 AudioPlatform platform;ComPtr<IMFSourceReader> reader;
 auto absolute=std::filesystem::absolute(path);
 audioCheck(MFCreateSourceReaderFromURL(absolute.c_str(),nullptr,&reader),"Cannot open MP3 audio");
 audioCheck(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE),"Cannot select audio stream");
 audioCheck(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM,TRUE),"No audio stream in file");
 ComPtr<IMFMediaType> type;
 audioCheck(MFCreateMediaType(&type),"Cannot create audio format");
 audioCheck(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio),"Cannot set audio type");
 audioCheck(type->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM),"Cannot set PCM type");
 audioCheck(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16),"Cannot set PCM depth");
 audioCheck(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,type.Get()),"Cannot decode MP3 to PCM");
 auto getFormat=[&](){
  ComPtr<IMFMediaType> actual;audioCheck(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&actual),"Cannot read decoded format");
  UINT32 rate=0,channels=0,bits=0,align=0;GUID subtype{};
  audioCheck(actual->GetGUID(MF_MT_SUBTYPE,&subtype),"Missing decoded format");
  audioCheck(actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate),"Missing audio rate");
  audioCheck(actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&channels),"Missing audio channels");
  audioCheck(actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,&bits),"Missing audio depth");
  audioCheck(actual->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,&align),"Missing audio alignment");
  require(subtype==MFAudioFormat_PCM&&bits==16&&(channels==1||channels==2)&&rate>=4000&&rate<=192000&&align==channels*2,"Unsupported decoded MP3 format");
  SourceWave w;w.rate=int(rate);w.channels=int(channels);w.bits=16;return w;
 };
 SourceWave result=getFormat();int emptyReads=0;
 for(;;){
  DWORD flags=0;ComPtr<IMFSample> sample;
  audioCheck(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,nullptr,&sample),"MP3 decoding failed");
  require(!(flags&MF_SOURCE_READERF_ERROR),"MP3 decoder reported an error");
  if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED){auto next=getFormat();require(next.rate==result.rate&&next.channels==result.channels,"MP3 audio format changes mid-file");}
  if(sample){
   ComPtr<IMFMediaBuffer> buffer;audioCheck(sample->ConvertToContiguousBuffer(&buffer),"Cannot read MP3 samples");
   BYTE* data=nullptr;DWORD count=0;audioCheck(buffer->Lock(&data,nullptr,&count),"Cannot lock decoded samples");
   struct Unlock {IMFMediaBuffer* buffer;~Unlock(){buffer->Unlock();}} unlock{buffer.Get()};
   size_t align=result.channels*2,frames=count/align;
   require(count%align==0,"Incomplete decoded MP3 sample frame");
   require(result.mono.size()+frames<=size_t(result.rate)*300,"Choose audio of five minutes or less, then trim the effect in the workshop");
   size_t offset=result.mono.size();result.mono.resize(offset+frames);
   for(size_t i=0;i<frames;i++){double value=0;for(int ch=0;ch<result.channels;ch++){size_t p=i*align+ch*2;value+=int16_t(unsigned(data[p])|(unsigned(data[p+1])<<8))/32768.;}result.mono[offset+i]=float(value/result.channels);}
   emptyReads=frames?0:emptyReads+1;
  }else ++emptyReads;
  if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
  require(emptyReads<1000,"MP3 decoder produced no audio");
 }
 require(!result.mono.empty(),"MP3 contains no audio samples");return result;
}
inline SourceWave decodeAudioFile(const std::filesystem::path& path){
 auto extension=path.extension().wstring();for(auto& c:extension)c=wchar_t(towlower(c));
 if(extension==L".wav")return decodeSourceWave(read(path));
 require(extension==L".mp3","Choose WAV or MP3 audio");return decodeMP3(path);
}
}
