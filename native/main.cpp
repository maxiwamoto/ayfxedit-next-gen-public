#define WIN32_LEAN_AND_MEAN
#include "project.h"
#include "sound_tools.h"
#include "game_export.h"
#include "register_export.h"
#include <windowsx.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <sstream>
#include <deque>
#include <regex>
#include <commctrl.h>
#include <uxtheme.h>
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"uxtheme.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"shell32.lib")
using namespace ayfx;
namespace fs=std::filesystem;
HINSTANCE instance;HWND mainWnd,grid,effects,nameEdit,clockBox,rateBox,octaveBox,statusLabel,modelBox;
HFONT uiFont,monoFont,titleFont,smallFont,boldFont;float scale=1;int current=0,row=0,first=0,column=0,anchor=0,selectionEnd=0,clockHz=1789773,fps=50,octave=4;
bool dirty=false,refreshing=false,dragging=false,strokeSaved=false;Bank bank(1);fs::path bankPath;Bytes audio;std::vector<Cell> clipboard;
struct Snapshot {Bank bank;std::vector<EffectGroup> groups;int current,row,anchor,end;};std::deque<Snapshot> undoStack,redoStack;
enum {New=100,Open,Save,SaveAs,SaveBare,LoadFx,SaveFx,ImportWav,ImportPsg,ExportWav,ExportCsv,ExportVT,Quit,ExportRegisters,
 Undo=130,Redo,Cut,Copy,Paste,DeleteRows,InsertRow,CloneRow,SelectAll,AddFx,RemoveFx,DuplicateFx,ClearFx,
 Play=160,PlayHere,Stop,Help,ChooseA,ChooseB,ChooseC,ChooseAuto,Linear,Solo,Channels,Linked,MuteA,MuteB,MuteC,PreviousEffect,NextEffect,About,
 Curves=500,Generate,Envelope,FindLibrary,Metadata,CaptureImport,ExportSDK,BatchImport,BatchExport,Recovery,ReopenSource,
 BankList=200,NameEdit,ClockBox,RateBox,OctaveBox,ModelBox,
 RemoveChannelA=600,RemoveChannelB,RemoveChannelC};
int importChannel=-1;bool linear=true,linked=true;unsigned audibleMask=7;
std::array<std::vector<Cell>,3> clipboardChannels;bool clipboardGrouped=false;
std::vector<EffectGroup> groups;int activeGroup=0;
void syncGroups(){if(groups.empty())groups=groupBank(bank);for(size_t g=0;g<groups.size();g++)for(int t:groups[g].tracks)if(t==current)activeGroup=int(g);}
bool referenceRequired(){if(hasEnvelope(bank[current]))return true;return !groups.empty()&&(groups[activeGroup].loopStart>=0||std::count_if(groups[activeGroup].tracks.begin(),groups[activeGroup].tracks.end(),[](int t){return t>=0;})>1);}
int channelCount(){int n=0;if(!groups.empty())for(int t:groups[activeGroup].tracks)if(t>=0)n++;return n;}
std::array<const Effect*,3> groupChannels(){std::array<const Effect*,3> out{};for(int ch=0;ch<3;ch++){int t=groups[activeGroup].tracks[ch];if(t>=0)out[ch]=&bank[t];}return out;}

PreviewModel previewModel=PreviewModel::Reference;
const COLORREF bg=RGB(240,244,248),ink=RGB(28,41,56),muted=RGB(100,116,134),accent=RGB(0,121,112),side=RGB(25,38,54);
HBRUSH bgBrush,whiteBrush,sideBrush;
void paintChrome(HDC dc);
void updateChannelControls();
void projectTiming(const fs::path& path,bool importLoops=true);
void captureGrid(const fs::path& path,bool full=false,HWND window=nullptr);
bool smokeChannels=false,dialogSmokePassed=false;
void stop();

std::wstring widen(const std::string& s){if(s.empty())return L"";int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),out.data(),n);return out;}
std::string narrow(const std::wstring& s){if(s.empty())return "";int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string out(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;}
int px(int x){return int(x*scale+.5f);}int height(){RECT r;GetClientRect(grid,&r);return int(r.bottom/scale);}
Effect& effect(){return bank[current];}
void fail(const std::exception& e){MessageBoxW(mainWnd,widen(e.what()).c_str(),L"AYFXEdit Native",MB_ICONERROR);}
void status(){std::wstringstream s;s<<(hasEnvelope(effect())?L"ENV | ":L"")<<L"Frame "<<row<<L" / "<<length(effect())<<L"  |  "<<encode(effect()).size()<<L" bytes  |  "<<fps<<L" Hz  |  "<<(linked?L"Linked frame edits":L"Single-track frame edits");if(!groups.empty()){auto& g=groups[activeGroup];if(g.loopStart>=0)s<<L"  |  Loop "<<g.loopStart<<L".."<<g.loopEnd<<L" (end excluded)";
 for(int f=0;f<MaxFrames;f++)if(noiseConflict(bank,g,f)){s<<L"  |  NOISE CONFLICT: frame "<<f;break;}}SetWindowTextW(statusLabel,s.str().c_str());}
void redraw(){InvalidateRect(grid,nullptr,FALSE);InvalidateRect(mainWnd,nullptr,FALSE);status();std::wstring title=L"AYFXEdit Studio - "+(bankPath.empty()?L"Untitled bank":bankPath.filename().wstring())+(dirty?L" *":L"");SetWindowTextW(mainWnd,title.c_str());}
void library(){syncGroups();SendMessage(effects,WM_SETREDRAW,FALSE,0);SendMessageW(effects,LB_RESETCONTENT,0,0);
 for(auto& g:groups){bool fav=false;for(int t:g.tracks)if(t>=0)fav|=bank[t].favorite;auto label=(fav?L"* ":L"")+widen(g.name);SendMessageW(effects,LB_ADDSTRING,0,(LPARAM)label.c_str());}
 SendMessageW(effects,LB_SETCURSEL,activeGroup,0);SendMessage(effects,WM_SETREDRAW,TRUE,0);InvalidateRect(effects,nullptr,FALSE);
 EnableWindow(modelBox,!referenceRequired());SendMessage(modelBox,CB_SETCURSEL,referenceRequired()?0:(previewModel==PreviewModel::Reference?0:1),0);SetWindowTextW(GetDlgItem(mainWnd,Play),channelCount()>1?L"Play all":L"Play");EnableWindow(GetDlgItem(mainWnd,PreviousEffect),activeGroup>0);EnableWindow(GetDlgItem(mainWnd,NextEffect),activeGroup+1<int(groups.size()));updateChannelControls();}
void refresh(){refreshing=true;library();SetWindowTextW(nameEdit,widen(effect().name).c_str());refreshing=false;redraw();}
void remember(){undoStack.push_back({bank,groups,current,row,anchor,selectionEnd});if(undoStack.size()>24)undoStack.pop_front();redoStack.clear();dirty=true;}
void history(bool redo){auto& src=redo?redoStack:undoStack;auto& dst=redo?undoStack:redoStack;if(src.empty())return;dst.push_back({bank,groups,current,row,anchor,selectionEnd});auto s=std::move(src.back());src.pop_back();bank=std::move(s.bank);groups=std::move(s.groups);current=s.current;row=s.row;anchor=s.anchor;selectionEnd=s.end;dirty=true;refresh();}
void visible(){int rows=std::max(1,(height()-28)/22);if(row<first)first=row;if(row>=first+rows)first=row-rows+1;first=std::clamp(first,0,MaxFrames-rows);SCROLLINFO si{sizeof(si),SIF_ALL};si.nMin=0;si.nMax=MaxFrames-1;si.nPage=rows;si.nPos=first;SetScrollInfo(grid,SB_VERT,&si,TRUE);redraw();}
UINT_PTR CALLBACK registerExportFileHook(HWND child,UINT message,WPARAM,LPARAM lp){
 if(message==WM_NOTIFY){auto* notice=(OFNOTIFYW*)lp;if(notice->hdr.code==CDN_TYPECHANGE)SendMessageW(GetParent(child),CDM_SETDEFEXT,0,(LPARAM)(notice->lpOFN->nFilterIndex==2?L"psg11":L"ayregs"));}
 return 0;
}
fs::path dialog(bool save,const wchar_t* filter,const wchar_t* ext,const std::wstring& initial=L"",bool registerTypes=false) {
 wchar_t path[32768]={};wcsncpy_s(path,initial.c_str(),_TRUNCATE);OPENFILENAMEW o{sizeof(o)};o.hwndOwner=mainWnd;o.lpstrFilter=filter;o.lpstrFile=path;o.nMaxFile=32768;o.lpstrDefExt=ext;
 o.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);if(registerTypes){o.Flags|=OFN_ENABLEHOOK;o.lpfnHook=registerExportFileHook;}if(save?GetSaveFileNameW(&o):GetOpenFileNameW(&o))return path;return {};
}
const wchar_t* bankFilter=L"AY effect bank (*.afb)\0*.afb\0\0";const wchar_t* fxFilter=L"AY effect (*.afx)\0*.afx\0\0";
bool saveBank(bool choose,bool names=true){fs::path p=bankPath;if(p.empty()||choose)p=dialog(true,bankFilter,L"afb",p.wstring());if(p.empty())return false;saveProjectBank(p,bank,{groups,clockHz,fps});bankPath=p;dirty=false;redraw();return true;}
bool discard(){if(!dirty)return true;int r=MessageBoxW(mainWnd,L"Save changes to this bank?",L"Unsaved changes",MB_YESNOCANCEL|MB_ICONQUESTION);return r==IDNO||(r==IDYES&&saveBank(false));}
void load(const fs::path& p){auto bytes=read(p);auto ext=p.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),towlower);
 Bank next;bool single=ext==L".afx";Project project;bool hasProject=false;
 if(single){auto e=decode(bytes,0,bytes.size());e.name=p.stem().u8string();next={e};}else{next=decodeBank(bytes);auto side=projectPath(p);if(fs::exists(side)){project=decodeProject(read(side,256*1024*1024),next,bytes);hasProject=true;}}
 auto nextGroups=hasProject?project.groups:groupBank(next);validateGroups(next,nextGroups);
 stop();bank=std::move(next);groups=std::move(nextGroups);bankPath=single?fs::path{}:p;dirty=single;
 current=row=first=anchor=selectionEnd=0;undoStack.clear();redoStack.clear();projectTiming(p,!hasProject);
 if(hasProject){clockHz=project.clock;fps=project.rate;SendMessage(clockBox,CB_SETCURSEL,clockHz==1789773?0:clockHz==1773400?1:2,0);SendMessage(rateBox,CB_SETCURSEL,fps==60?1:0,0);}
 refresh();visible();
}
// Only read clock/rate metadata for the exact companion bank name.
void projectTiming(const fs::path& path,bool importLoops){
 auto file=path.parent_path()/L"project.json";if(!fs::exists(file))return;
 auto bytes=read(file);std::string json(bytes.begin(),bytes.end());std::smatch match;
 if(!std::regex_search(json,match,std::regex(R"re("bank"\s*:\s*"([^"]+)")re"))||match[1].str()!=path.filename().u8string())return;
 if(std::regex_search(json,match,std::regex(R"re("tick_hz"\s*:\s*(50|60)\b)re"))){fps=std::stoi(match[1]);SendMessage(rateBox,CB_SETCURSEL,fps==60?1:0,0);}
 if(std::regex_search(json,match,std::regex(R"re("ay_clock_hz"\s*:\s*(1789773|1773400|2000000)\b)re"))){clockHz=std::stoi(match[1]);SendMessage(clockBox,CB_SETCURSEL,clockHz==1789773?0:clockHz==1773400?1:2,0);}
 if(importLoops){std::regex object(R"re(\{[^{}]*\})re");for(std::sregex_iterator it(json.begin(),json.end(),object),end;it!=end;++it){auto record=it->str();std::smatch name,start,frames;
  if(!std::regex_search(record,name,std::regex(R"re("name"\s*:\s*"([^"]+)")re"))||!std::regex_search(record,start,std::regex(R"re("loop"\s*:\s*(\d+))re"))||!std::regex_search(record,frames,std::regex(R"re("frames"\s*:\s*(\d+))re")))continue;
  int a=std::stoi(start[1]),b=std::stoi(frames[1]);if(a<0||a>=b||b>MaxFrames)continue;
  for(auto& g:groups)for(int t:g.tracks)if(t>=0&&bank[t].name==name[1].str()){g.loopStart=a;g.loopEnd=b;}
 }}
}
HWAVEOUT loopDevice=nullptr;std::array<WAVEHDR,3> loopHeaders{};std::array<std::array<int16_t,2048>,3> loopSamples{};std::unique_ptr<AYStream> liveStream;
void stop(){PlaySoundW(nullptr,nullptr,0);audio.clear();liveStream.reset();if(loopDevice){auto device=loopDevice;loopDevice=nullptr;waveOutReset(device);for(auto& h:loopHeaders)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device,&h,sizeof(h));waveOutClose(device);}loopHeaders={};}
void refillLoop(WAVEHDR* header){if(!loopDevice||!liveStream||!(header->dwFlags&WHDR_DONE))return;int index=int(header-loopHeaders.data());if(index<0||index>=3)return;for(auto& sample:loopSamples[index])sample=liveStream->next();require(waveOutWrite(loopDevice,header,sizeof(*header))==MMSYSERR_NOERROR,"Cannot queue streaming audio");}
unsigned playMask(bool solo){unsigned mask=audibleMask;if(solo)for(int ch=0;ch<3;ch++)if(groups[activeGroup].tracks[ch]==current)mask=1u<<ch;return mask;}
Bytes preview(int from,bool solo=false){auto channels=groupChannels();unsigned mask=playMask(solo);
 if(!referenceRequired()&&previewModel==PreviewModel::Legacy){int ch=0;while(!channels[ch])ch++;if(mask&(1u<<ch))return render(effect(),clockHz,fps,from,previewModel);}
 return renderChannels(channels,clockHz,fps,from,channelCount()>1?10000:21845,mask);
}
void play(int from,bool solo=false){stop();SetCursor(LoadCursor(nullptr,IDC_WAIT));try{auto g=groups[activeGroup];
 if(g.loopStart>=0&&from<g.loopEnd){
  auto frames=registerFrames(groupChannels(),g.loopEnd);liveStream=std::make_unique<AYStream>(std::move(frames),clockHz,fps,playMask(solo),channelCount()>1?10000:21845,from,g.loopStart,g.loopEnd);
  WAVEFORMATEX format{WAVE_FORMAT_PCM,1,SampleRate,SampleRate*2,2,16,0};require(waveOutOpen(&loopDevice,WAVE_MAPPER,&format,(DWORD_PTR)mainWnd,0,CALLBACK_WINDOW)==MMSYSERR_NOERROR,"Cannot open streaming device");
  waveOutPause(loopDevice);
  for(int i=0;i<3;i++){for(auto& sample:loopSamples[i])sample=liveStream->next();auto& h=loopHeaders[i];h.lpData=(LPSTR)loopSamples[i].data();h.dwBufferLength=sizeof(loopSamples[i]);require(waveOutPrepareHeader(loopDevice,&h,sizeof(h))==MMSYSERR_NOERROR,"Cannot prepare stream");require(waveOutWrite(loopDevice,&h,sizeof(h))==MMSYSERR_NOERROR,"Cannot queue stream");}
  waveOutRestart(loopDevice);
 }else{audio=preview(from,solo);require(PlaySoundW(reinterpret_cast<LPCWSTR>(audio.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT)!=FALSE,"Windows audio playback failed");}
 }catch(...){stop();SetCursor(LoadCursor(nullptr,IDC_ARROW));throw;}SetCursor(LoadCursor(nullptr,IDC_ARROW));}
std::vector<int> timelineTracks(){std::vector<int> out;if(linked&&channelCount()>1){for(int t:groups[activeGroup].tracks)if(t>=0)out.push_back(t);}else out.push_back(current);return out;}
void copy(){int a=std::min(anchor,selectionEnd),b=std::max(anchor,selectionEnd);clipboard.assign(effect().cells.begin()+a,effect().cells.begin()+b+1);clipboardGrouped=linked&&channelCount()>1;for(auto& c:clipboardChannels)c.clear();
 if(clipboardGrouped)for(int ch=0;ch<3;ch++){int t=groups[activeGroup].tracks[ch];if(t>=0)clipboardChannels[ch].assign(bank[t].cells.begin()+a,bank[t].cells.begin()+b+1);}}
void shiftLoop(int at,int count,bool deleting){auto& g=groups[activeGroup];if(g.loopStart<0||(!linked&&channelCount()>1))return;
 auto shift=[&](int frame){return deleting?(frame<=at?frame:std::max(at,frame-count)):(frame<at?frame:std::min(MaxFrames,frame+count));};
 g.loopStart=shift(g.loopStart);g.loopEnd=shift(g.loopEnd);if(g.loopStart>=g.loopEnd)g.loopStart=g.loopEnd=-1;
}
void erase(){remember();int a=std::min(anchor,selectionEnd),b=std::max(anchor,selectionEnd),n=b-a+1;for(int t:timelineTracks()){auto& c=bank[t].cells;std::move(c.begin()+b+1,c.end(),c.begin()+a);std::fill(c.end()-n,c.end(),Cell{});}shiftLoop(a,n,true);row=anchor=selectionEnd=a;visible();}
void insert(bool clone){remember();for(int t:timelineTracks()){auto& c=bank[t].cells;Cell previous=c[row];std::move_backward(c.begin()+row,c.end()-1,c.end());c[row]=clone?previous:Cell{};}shiftLoop(row,1,false);anchor=selectionEnd=row;redraw();}
void paste(){if(clipboard.empty())return;remember();int n=std::min(int(clipboard.size()),MaxFrames-row);
 for(int t:timelineTracks()){auto& c=bank[t].cells;std::move_backward(c.begin()+row,c.end()-n,c.end());std::fill_n(c.begin()+row,n,Cell{});
  const std::vector<Cell>* source=t==current?&clipboard:nullptr;
  if(linked&&channelCount()>1&&clipboardGrouped)for(int ch=0;ch<3;ch++)if(groups[activeGroup].tracks[ch]==t)source=&clipboardChannels[ch];
  if(source&&!source->empty())std::copy_n(source->begin(),std::min(n,int(source->size())),c.begin()+row);
 }shiftLoop(row,n,false);anchor=row;selectionEnd=row+n-1;redraw();}
INT_PTR CALLBACK channelsProc(HWND dlg,UINT message,WPARAM wp,LPARAM){
 if(message==WM_INITDIALOG){
  HDC dc=GetDC(dlg);auto old=SelectObject(dc,(HFONT)SendMessage(dlg,WM_GETFONT,0,0));TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(dlg,dc);
  for(int id:{1000,1005,1006}){RECT r;GetWindowRect(GetDlgItem(dlg,id),&r);SetWindowPos(GetDlgItem(dlg,id),nullptr,0,0,r.right-r.left,metrics.tmHeight+2*GetSystemMetrics(SM_CYEDGE),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);}
  for(int i=0;i<6;i++){RECT label,field;auto l=GetDlgItem(dlg,1010+i);GetWindowRect(l,&label);GetWindowRect(GetDlgItem(dlg,i<4?1000+i:1001+i),&field);MapWindowPoints(nullptr,dlg,(POINT*)&label,2);MapWindowPoints(nullptr,dlg,(POINT*)&field,2);SetWindowLongPtr(l,GWL_STYLE,GetWindowLongPtr(l,GWL_STYLE)|SS_CENTERIMAGE);SetWindowPos(l,nullptr,label.left,field.top,label.right-label.left,field.bottom-field.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);}
  auto& g=groups[activeGroup];SetDlgItemTextW(dlg,1000,widen(g.name).c_str());SendDlgItemMessage(dlg,1000,EM_SETLIMITTEXT,255,0);
  for(int ch=0;ch<3;ch++){auto combo=GetDlgItem(dlg,1001+ch);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"Unused");SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"New blank track");
   for(size_t i=0;i<bank.size();i++){auto name=std::to_wstring(i)+L"  "+widen(bank[i].name);SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)name.c_str());}SendMessage(combo,CB_SETCURSEL,g.tracks[ch]<0?0:g.tracks[ch]+2,0);}
  CheckDlgButton(dlg,1004,g.loopStart>=0?BST_CHECKED:BST_UNCHECKED);SetDlgItemInt(dlg,1005,std::max(0,g.loopStart),FALSE);int end=0;for(int t:g.tracks)if(t>=0)end=std::max(end,length(bank[t]));SetDlgItemInt(dlg,1006,g.loopEnd>=0?g.loopEnd:std::max(1,end),FALSE);if(smokeChannels)SetTimer(dlg,1,100,nullptr);return TRUE;}
 if(message==WM_TIMER&&smokeChannels){KillTimer(dlg,1);dialogSmokePassed=SendDlgItemMessage(dlg,1001,CB_GETCOUNT,0,0)==bank.size()+2&&SendDlgItemMessage(dlg,1003,CB_GETCURSEL,0,0)==4;UpdateWindow(dlg);captureGrid(L"native/build/channels-dialog.bmp",true,dlg);SendMessage(dlg,WM_COMMAND,IDOK,0);return TRUE;}
 if(message==WM_COMMAND){if(LOWORD(wp)==IDCANCEL){EndDialog(dlg,0);return TRUE;}if(LOWORD(wp)==IDOK){try{
   wchar_t text[256];GetDlgItemTextW(dlg,1000,text,256);std::array<int,3> tracks;
   for(int ch=0;ch<3;ch++){int n=int(SendDlgItemMessage(dlg,1001+ch,CB_GETCURSEL,0,0));require(n>=0,"Select a channel assignment");tracks[ch]=n==0?-1:n==1?-2:n-2;}
   int start=-1,end=-1;if(IsDlgButtonChecked(dlg,1004)==BST_CHECKED){BOOL validStart,validEnd;start=int(GetDlgItemInt(dlg,1005,&validStart,FALSE));end=int(GetDlgItemInt(dlg,1006,&validEnd,FALSE));require(validStart&&validEnd,"Enter decimal loop frame numbers");}
   Bank next=bank;auto nextGroups=groups;int group=assignChannels(next,nextGroups,activeGroup,narrow(text),tracks,start,end);
   remember();stop();bank=std::move(next);groups=std::move(nextGroups);activeGroup=group;
   if(std::find(groups[group].tracks.begin(),groups[group].tracks.end(),current)==groups[group].tracks.end())for(int t:groups[group].tracks)if(t>=0){current=t;break;}
   refresh();visible();EndDialog(dlg,1);
  }catch(const std::exception& e){MessageBoxW(dlg,widen(e.what()).c_str(),L"Channels",MB_ICONERROR);}return TRUE;}}
 return FALSE;
}
void note(int semitone){remember();int midi=(octave+1)*12+semitone;auto& c=effect().cells[row];c.tone=std::clamp(int(std::round(clockHz/(16*440*std::pow(2.0,(midi-69)/12.0)))),1,4095);c.t=true;c.volume=15;redraw();}
int effectNavigationKey(WPARAM key){if(GetKeyState(VK_CONTROL)&0x8000||GetKeyState(VK_MENU)&0x8000)return 0;
 if(key==VK_ADD||key==VK_OEM_PLUS)return NextEffect;
 if(key==VK_SUBTRACT||(key==VK_OEM_MINUS&&!(GetKeyState(VK_SHIFT)&0x8000)))return PreviousEffect;return 0;}
void selectGroup(int group){if(group<0||group>=int(groups.size())||group==activeGroup)return;stop();activeGroup=group;
 for(int t:groups[group].tracks)if(t>=0){current=t;break;}row=first=anchor=selectionEnd=0;refresh();visible();}
#include "help_ui.h"
#include "workshop_ui.h"
#include "tools_ui.h"

void command(int id){try{
 fs::path p;
 switch(id){
 case Curves:case Generate:case Envelope:case FindLibrary:case Metadata:case CaptureImport:case ExportSDK:case BatchImport:case BatchExport:case Recovery:case ReopenSource:toolCommand(id);break;
 case New:if(discard()){stop();bank=Bank(1);groups.clear();bankPath.clear();current=row=first=anchor=selectionEnd=0;dirty=false;undoStack.clear();redoStack.clear();refresh();}break;
 case Open:if(discard()){p=dialog(false,L"AY effects (*.afb;*.afx)\0*.afb;*.afx\0\0",L"afb");if(!p.empty())load(p);}break;
 case Save:saveBank(false);break;case SaveAs:saveBank(true);break;
 case SaveBare:for(auto& e:bank)requireClassic(e);p=dialog(true,bankFilter,L"afb");if(!p.empty()){write(p,encodeBank(bank,false));}break;
 case LoadFx:p=dialog(false,fxFilter,L"afx");if(!p.empty()){auto b=read(p);auto e=decode(b,0,b.size());e.name=p.stem().u8string();remember();effect()=e;row=first=anchor=selectionEnd=0;refresh();}break;
 case SaveFx:requireClassic(effect());p=dialog(true,fxFilter,L"afx");if(!p.empty())write(p,encode(effect()));break;
 case ImportWav:p=dialog(false,L"Audio (*.wav;*.mp3)\0*.wav;*.mp3\0WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0\0",L"wav");if(!p.empty())openWorkshop(p);break;
 case ImportPsg:p=dialog(false,L"PSG dump (*.psg)\0*.psg\0\0",L"psg");if(!p.empty()){auto e=importPSG(read(p),importChannel);e.name=p.stem().u8string();remember();effect()=e;row=first=anchor=selectionEnd=0;refresh();}break;
 case ExportCsv:requireClassic(effect());p=bankPath.empty()?fs::path(L"Untitled.csv"):fs::path(bankPath).replace_extension(L".csv");p=dialog(true,L"CSV (*.csv)\0*.csv\0\0",L"csv",p.wstring());if(!p.empty())write(p,csv(effect()));break;
 case ExportWav:case ExportVT:if(id!=ExportWav)requireClassic(effect());p=dialog(true,id==ExportWav?L"Wave (*.wav)\0*.wav\0\0":L"Vortex Tracker sample (*.txt)\0*.txt\0\0",id==ExportWav?L"wav":L"txt");if(!p.empty())write(p,id==ExportWav?preview(0):vt2(effect(),clockHz));break;
 case ExportRegisters:p=dialog(true,L"AY registers with envelopes (*.ayregs)\0*.ayregs\0Castlevania PSG11 (*.psg11)\0*.psg11\0\0",L"ayregs",L"",true);if(!p.empty())exportRegisters(p,bank,groups[activeGroup],clockHz,fps);break;
 case Quit:SendMessage(mainWnd,WM_CLOSE,0,0);break;
 case Undo:history(false);break;case Redo:history(true);break;case Copy:copy();break;case Cut:copy();erase();break;case DeleteRows:erase();break;
 case InsertRow:insert(false);break;case CloneRow:insert(true);break;
 case Paste:paste();break;
 case SelectAll:anchor=0;selectionEnd=std::max(0,length(effect())-1);redraw();break;
 case AddFx:case DuplicateFx:require(bank.size()<256,"A bank can hold up to 256 channel tracks");remember();{Effect e=id==DuplicateFx?effect():Effect{};if(id==DuplicateFx)e.name+=" copy";bank.push_back(e);current=int(bank.size())-1;groups.push_back({e.name,{{current,-1,-1}}});row=first=anchor=selectionEnd=0;refresh();}break;
 case RemoveChannelA:case RemoveChannelB:case RemoveChannelC:{int track=groups[activeGroup].tracks[id-RemoveChannelA];if(track>=0&&bank.size()>1){current=track;command(RemoveFx);}break;}
 case RemoveFx:if(bank.size()>1){
  int removed=current,survivor=-1;for(int track:groups[activeGroup].tracks)if(track>=0&&track!=removed){survivor=track;break;}
  remember();stop();removeTrack(bank,groups,removed);
  current=survivor>=0?survivor-(survivor>removed):std::min(removed,int(bank.size())-1);
  row=first=anchor=selectionEnd=0;refresh();visible();}break;
 case ClearFx:remember();effect().cells={};row=first=anchor=selectionEnd=0;refresh();break;
 case PreviousEffect:selectGroup(activeGroup-1);break;case NextEffect:selectGroup(activeGroup+1);break;
 case Channels:DialogBoxParamW(instance,MAKEINTRESOURCEW(2),mainWnd,channelsProc,0);break;
 case Linked:linked=!linked;CheckMenuItem(GetMenu(mainWnd),Linked,MF_BYCOMMAND|(linked?MF_CHECKED:MF_UNCHECKED));redraw();break;
 case MuteA:case MuteB:case MuteC:audibleMask^=1u<<(id-MuteA);CheckMenuItem(GetMenu(mainWnd),id,MF_BYCOMMAND|((audibleMask&(1u<<(id-MuteA)))?MF_UNCHECKED:MF_CHECKED));stop();updateChannelControls();redraw();break;
 case Play:play(0);break;case Solo:play(0,true);break;case PlayHere:play(row);break;case Stop:stop();break;
 case ChooseAuto:case ChooseA:case ChooseB:case ChooseC:importChannel=id==ChooseAuto?-1:id-ChooseA;CheckMenuRadioItem(GetMenu(mainWnd),ChooseA,ChooseAuto,id,MF_BYCOMMAND);break;
 case Linear:linear=!linear;CheckMenuItem(GetMenu(mainWnd),Linear,MF_BYCOMMAND|(linear?MF_CHECKED:MF_UNCHECKED));redraw();break;
 case Help:openGuide(mainWnd);break;
 case About:DialogBoxParamW(instance,MAKEINTRESOURCEW(5),mainWnd,aboutProc,0);break;
 default:if(id>=400&&id<412)note(id-400);
 }
 }catch(const std::exception& e){SetCursor(LoadCursor(nullptr,IDC_ARROW));fail(e);}}
void fill(HDC dc,RECT r,COLORREF color){auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
void label(HDC dc,int x,int y,const std::wstring& s,COLORREF color=RGB(28,36,48)){SetTextColor(dc,color);TextOutW(dc,x,y,s.c_str(),int(s.size()));}
int laneWidth(){RECT r;GetClientRect(grid,&r);return std::max(1,(int(r.right/scale)-65)/3);}
void updateChannelControls(){
 if(!grid||groups.empty())return;
 const int lane=laneWidth();const bool multi=channelCount()>1;
 for(int ch=0;ch<3;ch++){
  auto sound=GetDlgItem(mainWnd,MuteA+ch),remove=GetDlgItem(mainWnd,RemoveChannelA+ch);if(!sound||!remove)continue;
  const bool assigned=groups[activeGroup].tracks[ch]>=0;
  ShowWindow(sound,assigned?SW_SHOWNA:SW_HIDE);ShowWindow(remove,assigned?SW_SHOWNA:SW_HIDE);
  if(!assigned)continue;
  std::wstring name(1,wchar_t(L'A'+ch));auto caption=name+((audibleMask&(1u<<ch))?L": Sound ON":L": MUTED");
  SetWindowTextW(sound,caption.c_str());SetWindowTextW(remove,(L"Remove "+name).c_str());
  EnableWindow(remove,bank.size()>1);
  int x=282+65+(multi?ch*lane:0);
  MoveWindow(sound,px(x+3),px(254),px(120),px(30),TRUE);
  MoveWindow(remove,px(x+129),px(254),px(100),px(30),TRUE);
  InvalidateRect(sound,nullptr,FALSE);
 }
}
void paintChannels(HDC dc,int w,int h){
 int lane=laneWidth(),a=std::min(anchor,selectionEnd),b=std::max(anchor,selectionEnd);
 fill(dc,{0,0,w,28},RGB(223,229,237));label(dc,8,5,L"Frame");
 for(int ch=0;ch<3;ch++){int x=65+ch*lane,t=groups[activeGroup].tracks[ch];bool active=t==current;
  if(active)fill(dc,{x,0,x+lane,28},RGB(194,230,222));std::wstring header(1,wchar_t(L'A'+ch));header+=t<0?L" - unused":L"";label(dc,x+6,5,header,active?accent:ink);
  if(t>=0){label(dc,x+49,5,L"Tone",muted);label(dc,x+90,5,L"N",muted);label(dc,x+123,5,L"V",muted);label(dc,x+148,5,L"Vol",muted);}
  for(int i=first,y=28;i<MaxFrames&&y<h;i++,y+=22){COLORREF back=i>=a&&i<=b?RGB(214,233,255):(i%2?RGB(242,245,249):RGB(255,255,255));fill(dc,{x,y,x+lane-1,y+22},back);
   if(t<0){label(dc,x+6,y+3,L"-",muted);continue;}auto c=bank[t].cells[i];COLORREF color=i<length(bank[t])?ink:muted;wchar_t value[16];
   label(dc,x+6,y+3,c.t?L"T":L"-",color);label(dc,x+26,y+3,c.n?L"N":L"-",color);
   swprintf_s(value,L"%03X",c.tone);label(dc,x+49,y+3,value,color);swprintf_s(value,L"%02X",c.noise);label(dc,x+90,y+3,value,c.n&&c.volume&&noiseConflict(bank,groups[activeGroup],i)?RGB(200,35,40):color);if(c.envelope)wcscpy_s(value,L"E");else swprintf_s(value,L"%X",c.volume);label(dc,x+123,y+3,value,color);
   fill(dc,{x+148,y+6,x+lane-8,y+16},RGB(219,225,232));fill(dc,{x+148,y+6,x+148+std::max(0,lane-156)*c.volume/15,y+16},RGB(30,145,114));
   if(active&&i==row){int offset=column==0?49:column==1?90:123;RECT r{x+offset-1,y+1,x+offset+(column==0?32:column==1?23:14),y+21};FrameRect(dc,&r,(HBRUSH)GetStockObject(GRAY_BRUSH));}
  }
 }
 for(int i=first,y=28;i<MaxFrames&&y<h;i++,y+=22){fill(dc,{0,y,65,y+22},i>=a&&i<=b?RGB(214,233,255):RGB(249,250,252));wchar_t v[16];swprintf_s(v,L"%04X",i);label(dc,8,y+3,v,noiseConflict(bank,groups[activeGroup],i)?RGB(200,35,40):muted);if(i==groups[activeGroup].loopStart)fill(dc,{4,y,65,y+2},RGB(126,74,181));if(i==groups[activeGroup].loopEnd-1)fill(dc,{4,y+20,65,y+22},RGB(126,74,181));if(i==row)fill(dc,{0,y,4,y+22},accent);}
}
void paint(HDC dc){RECT physical;GetClientRect(grid,&physical);fill(dc,physical,RGB(249,250,252));SetGraphicsMode(dc,GM_ADVANCED);XFORM xf{scale,0,0,scale,0,0};SetWorldTransform(dc,&xf);SelectObject(dc,monoFont);SetBkMode(dc,TRANSPARENT);int w=int(physical.right/scale),h=int(physical.bottom/scale);
 if(channelCount()>1){paintChannels(dc,w,h);return;}
 fill(dc,{0,0,w,28},RGB(223,229,237));label(dc,8,5,L"Frame");label(dc,75,5,L"T");label(dc,95,5,L"N");label(dc,143,5,L"Tone");label(dc,197,5,L"Noise");label(dc,251,5,L"Vol");label(dc,300,5,L"Tone period");label(dc,600,5,L"Noise");label(dc,710,5,L"Volume");
 int a=std::min(anchor,selectionEnd),b=std::max(anchor,selectionEnd),len=length(effect());
 for(int i=first,y=28;i<MaxFrames&&y<h;i++,y+=22){auto c=effect().cells[i];COLORREF back=i>=a&&i<=b?RGB(214,233,255):(i%2?RGB(242,245,249):RGB(255,255,255));fill(dc,{0,y,w,y+22},back);
  if(i==groups[activeGroup].loopStart)fill(dc,{4,y,65,y+2},RGB(126,74,181));if(i==groups[activeGroup].loopEnd-1)fill(dc,{4,y+20,65,y+22},RGB(126,74,181));if(i==row)fill(dc,{0,y,4,y+22},RGB(0,100,210));wchar_t text[16];COLORREF ink=i<len?RGB(25,34,46):RGB(116,127,140);
  swprintf_s(text,L"%04X",i);label(dc,8,y+3,text,ink);label(dc,75,y+3,c.t?L"T":L"-",ink);label(dc,95,y+3,c.n?L"N":L"-",ink);
  swprintf_s(text,L"%03X",c.tone);label(dc,143,y+3,text,ink);swprintf_s(text,L"%02X",c.noise);label(dc,197,y+3,text,ink);swprintf_s(text,L"%X",c.volume);label(dc,251,y+3,text,ink);
  const int starts[]={300,600,710},widths[]={280,90,90},values[]={c.tone,c.noise,c.volume},maxs[]={4095,31,15};const COLORREF colors[]={RGB(43,119,188),RGB(139,91,174),RGB(30,145,114)};
  for(int j=0;j<3;j++){fill(dc,{starts[j],y+5,starts[j]+widths[j],y+17},RGB(219,225,232));double v=double(values[j])/maxs[j];if(j==0&&!linear)v=values[j]?std::clamp(std::log(std::max(8,values[j])/8.)/std::log(4095./8.),0.,1.):0;fill(dc,{starts[j],y+5,starts[j]+int(v*widths[j]),y+17},colors[j]);}
  if(i==row){int x=column==0?143:column==1?197:251;RECT r{x,y+1,x+(column==0?32:column==1?23:14),y+21};FrameRect(dc,&r,(HBRUSH)GetStockObject(GRAY_BRUSH));}
 }
}
void mouse(int x,int y,bool right,bool moving,bool shift){x=int(x/scale);y=int(y/scale);
 if(channelCount()>1&&x>=65){int lane=laneWidth(),ch=std::clamp((x-65)/lane,0,2),t=groups[activeGroup].tracks[ch];if(t<0)return;
  current=t;refreshing=true;SetWindowTextW(nameEdit,widen(effect().name).c_str());refreshing=false;
  int local=(x-65)%lane;
  if(y<28){redraw();return;}
  // Reuse the existing edit/undo path with coordinates for the selected channel.
  if(local<23)x=80;else if(local<44)x=100;else if(local<87)x=150;else if(local<120)x=200;else if(local<145)x=255;
  else x=710+std::clamp((local-148)*90/std::max(1,lane-156),0,90);
 }else if(channelCount()>1)x=12;
 if(y<28)return;int r=std::clamp(first+(y-28)/22,0,MaxFrames-1);row=r;if(!shift)anchor=r;selectionEnd=r;
 if(!moving){strokeSaved=false;dragging=true;SetCapture(grid);}
 bool edits=(x>=300&&x<=580)||(x>=600&&x<=690)||(x>=710&&x<=800)||(!moving&&x>=75&&x<120);
 if(edits&&!strokeSaved){remember();strokeSaved=true;}auto& c=effect().cells[r];
 if(x>=300&&x<=580){column=0;c.tone=right?0:linear?std::clamp((x-300)*4095/280,0,4095):int(8*std::pow(4095./8.,double(x-300)/280));}
 else if(x>=600&&x<=690){column=1;c.noise=right?0:std::clamp((x-600)*31/90,0,31);}
 else if(x>=710&&x<=800){column=2;c.volume=right?0:std::clamp((x-710)*15/90,0,15);}
 else if(!moving&&x>=75&&x<95)c.t=right?false:!c.t;
 else if(!moving&&x>=95&&x<120)c.n=right?false:!c.n;
 else if(x>=135&&x<190)column=0;else if(x>=190&&x<245)column=1;else if(x>=245&&x<290)column=2;
 redraw();}
LRESULT CALLBACK gridProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);auto mem=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bitmap);paint(mem);XFORM identity{1,0,0,1,0,0};SetWorldTransform(mem,&identity);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);EndPaint(hwnd,&ps);return 0;}
 case WM_PRINTCLIENT:paint((HDC)wp);return 0;
 case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:SetFocus(hwnd);mouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),msg==WM_RBUTTONDOWN,false,(wp&MK_SHIFT)!=0);return 0;
 case WM_MOUSEMOVE:if(dragging&&(wp&(MK_LBUTTON|MK_RBUTTON)))mouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),(wp&MK_RBUTTON)!=0,true,(wp&MK_SHIFT)!=0);return 0;
 case WM_LBUTTONUP:case WM_RBUTTONUP:dragging=false;ReleaseCapture();return 0;
 case WM_CAPTURECHANGED:dragging=false;return 0;
 case WM_MOUSEWHEEL:first=std::clamp(first-GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*4,0,std::max(0,MaxFrames-std::max(1,(height()-28)/22)));SetScrollPos(hwnd,SB_VERT,first,TRUE);InvalidateRect(hwnd,nullptr,FALSE);return 0;
 case WM_VSCROLL:{SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(hwnd,SB_VERT,&si);switch(LOWORD(wp)){case SB_LINEUP:first--;break;case SB_LINEDOWN:first++;break;case SB_PAGEUP:first-=si.nPage;break;case SB_PAGEDOWN:first+=si.nPage;break;case SB_THUMBTRACK:first=si.nTrackPos;break;}first=std::clamp(first,0,int(MaxFrames-si.nPage));SetScrollPos(hwnd,SB_VERT,first,TRUE);InvalidateRect(hwnd,nullptr,FALSE);return 0;}
 case WM_KEYDOWN:{int navigation=effectNavigationKey(wp);if(navigation){command(navigation);return 0;}bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0,shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;int id=0;
 if(ctrl){switch(wp){case 'Z':id=Undo;break;case 'Y':id=Redo;break;case 'C':id=Copy;break;case 'X':id=Cut;break;case 'V':id=Paste;break;case 'A':id=SelectAll;break;case VK_RETURN:id=PlayHere;break;case VK_INSERT:id=CloneRow;break;}}
 else{switch(wp){case VK_RETURN:id=Play;break;case VK_SPACE:id=Stop;break;case VK_DELETE:id=DeleteRows;break;case VK_INSERT:id=InsertRow;break;}}
 if(id){command(id);return 0;}int previous=row;switch(wp){case VK_UP:--row;break;case VK_DOWN:++row;break;case VK_PRIOR:row-=16;break;case VK_NEXT:row+=16;break;case VK_HOME:row=0;break;case VK_END:row=std::max(0,length(effect())-1);break;case VK_LEFT:column=std::max(0,column-1);break;case VK_RIGHT:column=std::min(2,column+1);break;}
 row=std::clamp(row,0,MaxFrames-1);if(row!=previous){if(!shift)anchor=row;selectionEnd=row;visible();return 0;}
 if(!ctrl){int digit=wp>='0'&&wp<='9'?int(wp-'0'):wp>='A'&&wp<='F'?int(wp-'A'+10):-1;if(digit>=0){remember();auto& c=effect().cells[row];if(column==0)c.tone=((c.tone<<4)|digit)&4095;if(column==1){int n=(c.noise<<4)|digit;c.noise=n<=31?n:digit;}if(column==2)c.volume=digit;}
 if(wp=='T'||wp=='N'){remember();if(wp=='T')effect().cells[row].t=!effect().cells[row].t;else effect().cells[row].n=!effect().cells[row].n;}}
 redraw();return 0;}
 }return DefWindowProcW(hwnd,msg,wp,lp);}
HWND child(const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){if(wcscmp(cls,L"BUTTON")==0)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;auto hwnd=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),mainWnd,(HMENU)(INT_PTR)id,instance,nullptr);SendMessage(hwnd,WM_SETFONT,(WPARAM)uiFont,TRUE);return hwnd;}
void menuItem(HMENU m,int id,const wchar_t* label){AppendMenuW(m,MF_STRING,id,label);}
void createMenu(){HMENU root=CreateMenu(),file=CreatePopupMenu(),edit=CreatePopupMenu(),sound=CreatePopupMenu(),help=CreatePopupMenu(),tools=CreatePopupMenu();
 menuItem(file,New,L"New bank\tCtrl+N");menuItem(file,Open,L"Open bank / effect...\tCtrl+O");menuItem(file,Save,L"Save bank\tCtrl+S");menuItem(file,SaveAs,L"Save bank as...");menuItem(file,SaveBare,L"Export bank without names...");AppendMenu(file,MF_SEPARATOR,0,nullptr);
 menuItem(file,LoadFx,L"Load current effect...");menuItem(file,SaveFx,L"Save current effect...");menuItem(file,ImportWav,L"WAV / MP3 workshop...");menuItem(file,ImportPsg,L"Import PSG dump...");menuItem(file,ExportWav,L"Export WAV...");menuItem(file,ExportCsv,L"Export CSV...");menuItem(file,ExportRegisters,L"Export AY registers...");menuItem(file,ExportVT,L"Export VT2 sample (C4)...");menuItem(file,Quit,L"Exit");
 menuItem(edit,Undo,L"Undo\tCtrl+Z");menuItem(edit,Redo,L"Redo\tCtrl+Y");menuItem(edit,Cut,L"Cut frames\tCtrl+X");menuItem(edit,Copy,L"Copy frames\tCtrl+C");menuItem(edit,Paste,L"Insert pasted frames\tCtrl+V");menuItem(edit,SelectAll,L"Select all frames\tCtrl+A");menuItem(edit,InsertRow,L"Insert frame\tInsert");menuItem(edit,CloneRow,L"Duplicate frame\tCtrl+Insert");menuItem(edit,DeleteRows,L"Delete frames\tDelete");menuItem(edit,ClearFx,L"Clear current effect");menuItem(edit,PreviousEffect,L"Previous effect\t-");menuItem(edit,NextEffect,L"Next effect\t+");menuItem(edit,Channels,L"Effect channels and loop...");menuItem(edit,Linked,L"Link channel frame edits");
 menuItem(sound,Play,L"Play\tEnter");menuItem(sound,PlayHere,L"Play from cursor\tCtrl+Enter");menuItem(sound,Stop,L"Stop\tSpace");menuItem(sound,Solo,L"Solo active channel");menuItem(sound,MuteA,L"Mute A");menuItem(sound,MuteB,L"Mute B");menuItem(sound,MuteC,L"Mute C");AppendMenu(sound,MF_SEPARATOR,0,nullptr);menuItem(sound,ChooseA,L"PSG import: channel A");menuItem(sound,ChooseB,L"PSG import: channel B");menuItem(sound,ChooseC,L"PSG import: channel C");menuItem(sound,ChooseAuto,L"PSG import: automatic");menuItem(sound,Linear,L"Linear tone bars");menuItem(help,Help,L"User guide\tF1");menuItem(help,About,L"About AYFXEDIT - NEXT GEN...");
 menuItem(tools,Curves,L"Curve designer...");menuItem(tools,Generate,L"Sound generator...");menuItem(tools,Envelope,L"Envelope and pitch...");menuItem(tools,CaptureImport,L"Extract PSG / VGM capture...");menuItem(tools,ExportSDK,L"Export game SDK (MSX / Spectrum)...");AppendMenu(tools,MF_SEPARATOR,0,nullptr);menuItem(tools,FindLibrary,L"Find in library...");menuItem(tools,Metadata,L"Tags, favourite and priority...");menuItem(tools,BatchImport,L"Batch import audio / AFX...");menuItem(tools,BatchExport,L"Batch export WAV + project...");menuItem(tools,ReopenSource,L"Reopen saved audio source...");menuItem(tools,Recovery,L"Recover autosaved project...");
 AppendMenuW(root,MF_POPUP,(UINT_PTR)file,L"File");AppendMenuW(root,MF_POPUP,(UINT_PTR)edit,L"Edit");AppendMenuW(root,MF_POPUP,(UINT_PTR)sound,L"Sound");AppendMenuW(root,MF_POPUP,(UINT_PTR)tools,L"Tools");AppendMenuW(root,MF_POPUP,(UINT_PTR)help,L"Help");SetMenu(mainWnd,root);CheckMenuRadioItem(root,ChooseA,ChooseAuto,ChooseAuto,MF_BYCOMMAND);CheckMenuItem(root,Linear,MF_CHECKED);CheckMenuItem(root,Linked,MF_CHECKED);
}
void layout(){RECT r;GetClientRect(mainWnd,&r);int w=int(r.right/scale),h=int(r.bottom/scale);
 MoveWindow(effects,px(16),px(142),px(240),px(h-228),TRUE);
 MoveWindow(grid,px(282),px(292),px(w-306),px(h-402),TRUE);updateChannelControls();
 MoveWindow(nameEdit,px(282),px(104),px(404),px(30),TRUE);
 MoveWindow(clockBox,px(710),px(104),px(208),px(180),TRUE);MoveWindow(rateBox,px(932),px(104),px(100),px(140),TRUE);
 MoveWindow(modelBox,px(w-234),px(28),px(210),px(160),TRUE);
 MoveWindow(statusLabel,px(16),px(h-29),px(w-32),px(23),TRUE);
 int ids[]={AddFx,DuplicateFx};for(int i=0;i<2;i++)MoveWindow(GetDlgItem(mainWnd,ids[i]),px(16+i*122),px(h-73),px(116),px(30),TRUE);
 if(octaveBox){MoveWindow(GetDlgItem(mainWnd,300),px(282),px(h-68),px(56),px(24),TRUE);MoveWindow(octaveBox,px(340),px(h-73),px(50),px(160),TRUE);for(int i=0;i<12;i++)MoveWindow(GetDlgItem(mainWnd,400+i),px(410+i*52),px(h-73),px(46),px(32),TRUE);}visible();
}
void chromeText(HDC dc,int x,int y,const std::wstring& value,HFONT font,COLORREF color){SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);TextOutW(dc,px(x),px(y),value.c_str(),int(value.size()));}
void paintChrome(HDC dc){RECT r;GetClientRect(mainWnd,&r);int w=int(r.right/scale),h=int(r.bottom/scale);
 fill(dc,r,bg);fill(dc,{0,0,px(270),px(h-38)},side);fill(dc,{px(270),0,r.right,px(82)},RGB(255,255,255));
 chromeText(dc,20,20,L"AYFXEdit",titleFont,RGB(243,249,255));chromeText(dc,22,57,L"SOUND EFFECT STUDIO",smallFont,RGB(139,164,188));
 chromeText(dc,18,110,L"EFFECT LIBRARY",smallFont,RGB(149,172,196));chromeText(dc,195,110,std::to_wstring(activeGroup+1)+L" / "+std::to_wstring(groups.size()),smallFont,RGB(149,172,196));
 int multiCount=0;for(auto& g:groups){int n=0;for(int t:g.tracks)if(t>=0)n++;if(n>1)multiCount++;}
 chromeText(dc,18,128,std::to_wstring(multiCount)+(multiCount==1?L" multichannel effect":L" multichannel effects"),smallFont,RGB(99,220,199));
 chromeText(dc,282,263,L"Channels",smallFont,muted);
 chromeText(dc,282,85,L"ACTIVE TRACK",smallFont,muted);chromeText(dc,710,85,L"CHIP CLOCK",smallFont,muted);chromeText(dc,932,85,L"FRAME RATE",smallFont,muted);
 fill(dc,{px(282),px(149),px(w-24),px(245)},RGB(255,255,255));auto c=effect().cells[row];
 wchar_t tone[48],noise[48],vol[48],duration[48];swprintf_s(tone,L"%.1f Hz",clockHz/(16.0*std::max(1,c.tone)));swprintf_s(noise,L"%.2f kHz",clockHz/(16000.0*std::max(1,c.noise)));
 if(c.volume)swprintf_s(vol,L"%.1f dB",20*std::log10(amplitude(c.volume,referenceRequired()?PreviewModel::Reference:previewModel)));else wcscpy_s(vol,L"Silent");swprintf_s(duration,L"%.3f s",double(length(effect()))/fps);
 const wchar_t* labels[]={L"TONE",L"NOISE CLOCK",L"LEVEL / FULL SCALE",L"DURATION",L"ENCODED"};
 std::wstring vals[]={c.t?tone:L"Disabled",c.n?noise:L"Disabled",vol,duration,std::to_wstring(encode(effect()).size())+L" bytes"};
 int step=(w-340)/5;for(int i=0;i<5;i++){chromeText(dc,298+i*step,160,labels[i],smallFont,muted);chromeText(dc,298+i*step,181,vals[i],boldFont,i==2?accent:ink);}
 int width=w-338,len=length(effect());fill(dc,{px(298),px(221),px(w-40),px(233)},RGB(232,238,243));
 if(len){for(int x=0;x<width;x++){int f=std::min(len-1,x*len/width);int v=int(amplitude(effect().cells[f].volume,referenceRequired()?PreviewModel::Reference:previewModel)*12);fill(dc,{px(298+x),px(233-v),px(299+x),px(233)},RGB(27,157,145));}
 int cursor=298+std::min(width-1,row*width/len);fill(dc,{px(cursor),px(218),px(cursor+2),px(236)},ink);}
}
void drawControl(DRAWITEMSTRUCT* d){HDC dc=d->hDC;RECT r=d->rcItem;SetBkMode(dc,TRANSPARENT);
 if(d->CtlID==BankList){if(d->itemID==UINT(-1)||d->itemID>=groups.size())return;bool selected=(d->itemState&ODS_SELECTED)!=0;
 fill(dc,r,selected?RGB(40,67,83):side);if(selected)fill(dc,{r.left,r.top,r.left+px(3),r.bottom},RGB(51,212,187));
 auto& group=groups[d->itemID];bool favourite=false;for(int t:group.tracks)if(t>=0)favourite|=bank[t].favorite;auto name=(favourite?L"* ":L"")+widen(group.name);std::wstring secondary;int frames=0,channels=0;
 for(int ch=0;ch<3;ch++)if(group.tracks[ch]>=0){if(channels++)secondary+=L" + ";secondary+=wchar_t(L'A'+ch);frames=std::max(frames,length(bank[group.tracks[ch]]));}
 secondary+=L" /  "+std::to_wstring(frames)+L" frames";if(group.loopStart>=0)secondary+=L" / loop";
 int textRight=r.right-px(8);
 if(channels>1){RECT badge{r.right-px(53),r.top+px(8),r.right-px(7),r.top+px(28)};
  fill(dc,badge,channels==3?RGB(91,75,158):RGB(0,113,104));SelectObject(dc,smallFont);SetTextColor(dc,RGB(255,255,255));auto caption=std::to_wstring(channels)+L" CH";
  DrawTextW(dc,caption.c_str(),-1,&badge,DT_CENTER|DT_VCENTER|DT_SINGLELINE);textRight=badge.left-px(5);}
 RECT text{r.left+px(13),r.top+px(8),textRight,r.top+px(30)};SelectObject(dc,uiFont);SetTextColor(dc,RGB(236,244,252));DrawTextW(dc,name.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
 text.top=r.top+px(31);text.bottom=r.bottom;text.right=r.right-px(8);SelectObject(dc,smallFont);SetTextColor(dc,RGB(146,172,192));DrawTextW(dc,secondary.c_str(),-1,&text,DT_SINGLELINE|DT_NOPREFIX);return;}
 if(d->CtlType==ODT_BUTTON){bool pressed=(d->itemState&ODS_SELECTED)!=0,primary=d->CtlID==Play,piano=d->CtlID>=400&&d->CtlID<412;
 bool sharp=piano&&(d->CtlID==401||d->CtlID==403||d->CtlID==406||d->CtlID==408||d->CtlID==410);bool sidebar=d->CtlID==AddFx||d->CtlID==DuplicateFx||d->CtlID==Channels||d->CtlID==PreviousEffect||d->CtlID==NextEffect;
 bool channelSound=d->CtlID>=MuteA&&d->CtlID<=MuteC,channelRemove=d->CtlID>=RemoveChannelA&&d->CtlID<=RemoveChannelC;
 bool soundOn=channelSound&&(audibleMask&(1u<<(d->CtlID-MuteA)));
 COLORREF back=channelSound?(soundOn?accent:RGB(166,55,49)):channelRemove?RGB(250,225,222):primary?accent:sharp?RGB(45,59,76):sidebar?RGB(43,61,79):RGB(228,235,241);if(pressed)back=RGB(172,204,208);
 fill(dc,r,sidebar?side:bg);auto brush=CreateSolidBrush(back);auto pen=CreatePen(PS_SOLID,1,back);auto oldB=SelectObject(dc,brush),oldP=SelectObject(dc,pen);RoundRect(dc,r.left,r.top,r.right,r.bottom,px(8),px(8));SelectObject(dc,oldB);SelectObject(dc,oldP);DeleteObject(brush);DeleteObject(pen);
 wchar_t text[100];GetWindowTextW(d->hwndItem,text,100);SelectObject(dc,uiFont);SetTextColor(dc,(d->itemState&ODS_DISABLED)?muted:(primary||sharp||sidebar||channelSound)&&!pressed?RGB(255,255,255):ink);DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
 if(d->itemState&ODS_FOCUS){InflateRect(&r,-4,-4);DrawFocusRect(dc,&r);}}
}
LRESULT CALLBACK mainProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 static HWND lastFocusedChild=nullptr;
 try{switch(msg){
 case WM_ACTIVATE:
  // DefWindowProc focuses the top-level window on return; remember its child first.
  if(LOWORD(wp)==WA_INACTIVE){auto focus=GetFocus();if(IsChild(hwnd,focus))lastFocusedChild=focus;}
  break;
 case WM_SETFOCUS:{
  // Do not redirect input while a modal dialog has disabled the editor.
  if(!IsWindowEnabled(hwnd)||IsIconic(hwnd))return 0;
  HWND target=lastFocusedChild;
  if(!IsWindow(target)||!IsChild(hwnd,target)||!IsWindowVisible(target)||!IsWindowEnabled(target))target=grid;
  if(IsWindow(target)&&IsChild(hwnd,target)&&IsWindowVisible(target)&&IsWindowEnabled(target))SetFocus(target);
  return 0;}
 case WM_TIMER:if(wp==90)autosave();return 0;
 case MM_WOM_DONE:if((HWAVEOUT)wp==loopDevice)refillLoop((WAVEHDR*)lp);return 0;
 // Compose the complete chrome offscreen so slider edits never expose its clear pass.
 case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);
  if(r.right>0&&r.bottom>0){auto mem=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bitmap);
   paintChrome(mem);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);}
  EndPaint(hwnd,&ps);return 0;}
 case WM_PRINTCLIENT:paintChrome((HDC)wp);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_DRAWITEM:drawControl((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_CTLCOLORLISTBOX:if((HWND)lp==effects){SetBkColor((HDC)wp,side);return (LRESULT)sideBrush;}SetTextColor((HDC)wp,ink);SetBkColor((HDC)wp,RGB(255,255,255));return (LRESULT)whiteBrush;
 case WM_CTLCOLOREDIT:SetTextColor((HDC)wp,ink);SetBkColor((HDC)wp,RGB(255,255,255));return (LRESULT)whiteBrush;
 case WM_CTLCOLORSTATIC:SetTextColor((HDC)wp,muted);SetBkColor((HDC)wp,bg);return (LRESULT)bgBrush;
 case WM_SIZE:if(grid)layout();return 0;
 case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={px(1140),px(690)};return 0;
 case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);
 if(id==BankList&&event==LBN_SELCHANGE){selectGroup(int(SendMessage(effects,LB_GETCURSEL,0,0)));return 0;}
 if(id==NameEdit&&event==EN_CHANGE&&!refreshing){wchar_t s[256];GetWindowTextW(nameEdit,s,256);auto n=narrow(s);if(n.size()>255)return 0;if(n!=effect().name){remember();if(channelCount()==1&&groups[activeGroup].name==effect().name)groups[activeGroup].name=n;effect().name=n;library();redraw();}return 0;}
 if(lp!=0&&event==CBN_SELCHANGE){if(id==ClockBox||id==RateBox)dirty=true;if(id==ClockBox){int i=int(SendMessage(clockBox,CB_GETCURSEL,0,0));clockHz=i==0?1789773:i==1?1773400:2000000;}if(id==ModelBox){previewModel=SendMessage(modelBox,CB_GETCURSEL,0,0)==0?PreviewModel::Reference:PreviewModel::Legacy;stop();}if(id==RateBox)fps=SendMessage(rateBox,CB_GETCURSEL,0,0)==0?50:60;if(id==OctaveBox)octave=int(SendMessage(octaveBox,CB_GETCURSEL,0,0));redraw();return 0;}
 if(event==BN_CLICKED||lp==0){command(id);if(id!=Quit)SetFocus(grid);}return 0;}
 case WM_DROPFILES:{HDROP drop=(HDROP)wp;wchar_t p[32768];DragQueryFileW(drop,0,p,32768);DragFinish(drop);if(discard())load(p);return 0;}
 case WM_CLOSE:if(discard()){clearRecovery();stop();DestroyWindow(hwnd);}return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }}catch(const std::exception& e){fail(e);}return DefWindowProcW(hwnd,msg,wp,lp);}
void captureGrid(const fs::path& path,bool full,HWND window){if(!window)window=mainWnd;RECT r;if(full)GetWindowRect(window,&r);else GetClientRect(grid,&r);r.right-=r.left;r.bottom-=r.top;BITMAPINFO bi{};bi.bmiHeader={sizeof(BITMAPINFOHEADER),r.right,-r.bottom,1,32,BI_RGB};void* bits;HDC dc=GetDC(grid),mem=CreateCompatibleDC(dc);auto bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(mem,bmp);if(full)PrintWindow(window,mem,0);else paint(mem);GdiFlush();BITMAPFILEHEADER bf{0x4d42,DWORD(54+r.right*r.bottom*4),0,0,54};std::ofstream f(path,std::ios::binary);f.write((char*)&bf,14);f.write((char*)&bi.bmiHeader,40);f.write((char*)bits,r.right*r.bottom*4);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(grid,dc);}
HACCEL editorAccelerators(){ACCEL acc[]={{FVIRTKEY|FCONTROL,'S',Save},{FVIRTKEY|FCONTROL,'O',Open},{FVIRTKEY|FCONTROL,'N',New},{FVIRTKEY,VK_F1,Help}};return CreateAcceleratorTable(acc,4);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){instance=inst;SetProcessDPIAware();HDC screen=GetDC(nullptr);scale=GetDeviceCaps(screen,LOGPIXELSX)/96.f;ReleaseDC(nullptr,screen);bgBrush=CreateSolidBrush(bg);whiteBrush=CreateSolidBrush(RGB(255,255,255));sideBrush=CreateSolidBrush(side);
 uiFont=CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");monoFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
 titleFont=CreateFontW(-px(28),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 smallFont=CreateFontW(-px(11),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 boldFont=CreateFontW(-px(20),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=mainProc;wc.lpszClassName=L"AYFXNative";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.hIcon=LoadIcon(inst,MAKEINTRESOURCE(1));RegisterClassW(&wc);wc.lpszClassName=L"AYFXGrid";wc.lpfnWndProc=gridProc;RegisterClassW(&wc);
 mainWnd=CreateWindowW(L"AYFXNative",L"AYFXEdit Native",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,px(1200),px(860),nullptr,nullptr,inst,nullptr);if(!mainWnd)return 1;createMenu();
 child(L"BUTTON",L"Channels / loop...",BS_PUSHBUTTON,Channels,16,78,156,28);child(L"BUTTON",L"-",BS_PUSHBUTTON,PreviousEffect,180,78,34,28);child(L"BUTTON",L"+",BS_PUSHBUTTON,NextEffect,222,78,34,28);child(L"BUTTON",L"Open",BS_PUSHBUTTON,Open,282,25,72,34);child(L"BUTTON",L"Save",BS_PUSHBUTTON,Save,362,25,72,34);child(L"BUTTON",L"Play",BS_PUSHBUTTON,Play,454,25,100,34);child(L"BUTTON",L"Stop",BS_PUSHBUTTON,Stop,562,25,72,34);
 child(L"BUTTON",L"Solo",BS_PUSHBUTTON,Solo,770,25,64,34);child(L"BUTTON",L"Export WAV",BS_PUSHBUTTON,ExportWav,648,25,114,34);clockBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,ClockBox,420,12,210,180);for(auto s:{L"MSX - 1.789773 MHz",L"Spectrum - 1.7734 MHz",L"2 MHz"})SendMessageW(clockBox,CB_ADDSTRING,0,(LPARAM)s);SendMessage(clockBox,CB_SETCURSEL,0,0);
 rateBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,RateBox,645,12,100,140);for(auto s:{L"50 Hz",L"60 Hz"})SendMessageW(rateBox,CB_ADDSTRING,0,(LPARAM)s);SendMessage(rateBox,CB_SETCURSEL,0,0);
 nameEdit=child(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,NameEdit,112,54,370,28);SendMessage(nameEdit,EM_SETLIMITTEXT,255,0);
 child(L"BUTTON",L"+ Track",BS_PUSHBUTTON,AddFx,500,54,100,28);child(L"BUTTON",L"Copy",BS_PUSHBUTTON,DuplicateFx,608,54,100,28);for(int ch=0;ch<3;ch++){
  child(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,MuteA+ch,0,0,120,30);
  child(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,RemoveChannelA+ch,0,0,100,30);
 }
 effects=child(L"LISTBOX",L"",WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,BankList,12,96,210,500);
 SendMessage(effects,LB_SETITEMHEIGHT,0,px(55));
 grid=child(L"AYFXGrid",L"",WS_VSCROLL|WS_TABSTOP,0,235,96,850,500);
 statusLabel=child(L"STATIC",L"",0,0,12,690,1100,24);
 // Piano controls are placed below the editor on each resize via fixed bottom anchors.
 child(L"STATIC",L"Octave",0,300,235,645,60,24);octaveBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,OctaveBox,297,640,50,200);for(int i=0;i<=8;i++){auto s=std::to_wstring(i);SendMessageW(octaveBox,CB_ADDSTRING,0,(LPARAM)s.c_str());}SendMessage(octaveBox,CB_SETCURSEL,octave,0);
 const wchar_t* notes[]={L"C",L"C#",L"D",L"D#",L"E",L"F",L"F#",L"G",L"G#",L"A",L"A#",L"B"};for(int i=0;i<12;i++)child(L"BUTTON",notes[i],BS_PUSHBUTTON,400+i,365+i*53,640,48,30);
 modelBox=child(L"COMBOBOX",L"",CBS_DROPDOWNLIST,ModelBox,900,25,210,160);for(auto text:{L"Reference AY preview",L"Legacy AYFX preview"})SendMessageW(modelBox,CB_ADDSTRING,0,(LPARAM)text);SendMessage(modelBox,CB_SETCURSEL,0,0);
 DragAcceptFiles(mainWnd,TRUE);refresh();layout();int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool smoke=argc>1&&wcscmp(argv[1],L"--smoke-test")==0;
 if((argc==4||argc==5)&&wcscmp(argv[1],L"--export-registers")==0){int result=0;try{load(argv[2]);size_t used=0;int index=argc==5?std::stoi(argv[4],&used):0;require(index>=0&&index<int(groups.size())&&(argc!=5||used==wcslen(argv[4])),"Invalid group index");exportRegisters(argv[3],bank,groups[index],clockHz,fps);}catch(const std::exception& e){auto error=fs::path(argv[3]);error+=L".error.txt";std::ofstream(error)<<e.what();result=2;}stop();DestroyWindow(mainWnd);LocalFree(argv);return result;}
 if(argc==4&&wcscmp(argv[1],L"--export-sdk")==0){int result=0;try{load(argv[2]);exportBankData(argv[3],bank,{groups,clockHz,fps});}catch(const std::exception& e){auto error=fs::path(argv[3]);error+=L".error.txt";std::ofstream(error)<<e.what();result=2;}stop();DestroyWindow(mainWnd);LocalFree(argv);return result;}
 try{if(argc>1&&!smoke)load(argv[1]);if(smoke){ShowWindow(mainWnd,SW_SHOWNOACTIVATE);UpdateWindow(mainWnd);load(L"native/build/fixtures/sample.afb");int old=effect().cells[0].volume;column=2;SendMessage(grid,WM_KEYDOWN,'E',0);require(effect().cells[0].volume==14,"GUI hex edit failed");command(Undo);require(effect().cells[0].volume==old,"GUI undo failed");require(!dirty || !undoStack.empty() || !redoStack.empty(),"Missing edit history");
 // Exercise real top-level activation changes, then send keys to the actual focus owner.
 {
  HWND other=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"AYFX focus regression",WS_OVERLAPPEDWINDOW,0,0,160,80,nullptr,nullptr,instance,nullptr);
  require(other!=nullptr,"Cannot create focus test window");ShowWindow(other,SW_SHOWNOACTIVATE);
  auto leave=[&](){SetActiveWindow(other);SetFocus(other);require(GetFocus()==other,"Focus test did not leave editor");};
  auto back=[&](){SetActiveWindow(mainWnd);};
  for(HWND control:{grid,effects,nameEdit,clockBox}){
   SetActiveWindow(mainWnd);SetFocus(control);require(GetFocus()==control,"Cannot focus test control");
   if(control==nameEdit)SendMessage(nameEdit,EM_SETSEL,1,3);
   leave();back();require(GetFocus()==control,"Editor did not restore keyboard focus after activation");
   if(control==nameEdit){DWORD start,end;SendMessage(nameEdit,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);require(start==1&&end==3,"Activation lost text selection");}
  }
  SetFocus(grid);leave();back();int previousRow=row;
  SendMessage(GetFocus(),WM_KEYDOWN,VK_DOWN,0);require(row==previousRow+1,"Keyboard navigation failed after activation");
  SendMessage(GetFocus(),WM_KEYDOWN,VK_UP,0);require(row==previousRow,"Keyboard navigation did not return to row");
  SetFocus(nameEdit);leave();EnableWindow(nameEdit,FALSE);back();require(GetFocus()==grid,"Disabled focus target did not fall back to grid");EnableWindow(nameEdit,TRUE);
  HWND temporary=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE,0,0,20,20,mainWnd,nullptr,instance,nullptr);
  SetFocus(temporary);leave();DestroyWindow(temporary);back();require(GetFocus()==grid,"Destroyed focus target did not fall back to grid");
  DestroyWindow(other);
 }
 dirty=false;mouse(px(12),px(32),false,false,false);ReleaseCapture();dragging=false;require(!dirty,"Selection changed document");
 int before=effect().cells[0].volume;mouse(px(800),px(32),false,false,false);ReleaseCapture();dragging=false;require(effect().cells[0].volume==15,"Volume bar edit failed");command(Undo);require(effect().cells[0].volume==before,"Mouse edit undo failed");
 auto original=encode(effect());anchor=0;selectionEnd=2;copy();row=3;command(Paste);command(Undo);require(encode(effect())==original,"Paste undo failed");row=anchor=selectionEnd=0;
 int count=int(bank.size());command(DuplicateFx);require(bank.size()==count+1,"Duplicate effect failed");command(Undo);require(bank.size()==count,"Bank undo failed");
 // Scrolling changes only the viewport: no document edit or surrounding repaint.
 visible();RedrawWindow(mainWnd,nullptr,nullptr,RDW_UPDATENOW|RDW_ALLCHILDREN);
 int scrollRow=row;bool scrollDirty=dirty;auto scrollData=encode(effect());
 SendMessage(grid,WM_VSCROLL,SB_PAGEDOWN,0);require(first>0,"Page scroll did not move viewport");
 require(!GetUpdateRect(mainWnd,nullptr,FALSE),"Scrolling invalidated surrounding chrome");
 SendMessage(grid,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
 require(!GetUpdateRect(mainWnd,nullptr,FALSE),"Mouse wheel invalidated surrounding chrome");
 require(row==scrollRow&&dirty==scrollDirty&&encode(effect())==scrollData,"Scrolling changed selection or effect data");
 first=0;visible();
 command(Play);stop();
 // Generic channels, linked timeline edits, undo, persistence and real loop output.
 bank=Bank(3);for(int t=0;t<3;t++){bank[t].name=t==0?"Zap body":t==1?"Air burst":"Low ring";for(int f=0;f<8;f++)bank[t].cells[f]={180+t*60+f,7,12,true,false};}
 groups=groupBank(bank);current=row=first=anchor=selectionEnd=0;undoStack.clear();redoStack.clear();activeGroup=assignChannels(bank,groups,0,"Laser shot",{{0,1,2}},2,6);refresh();visible();
 auto genericOriginal=encodeBank(bank,true);row=3;insert(false);for(auto& e:bank)require(e.cells[3].volume==0&&e.cells[4].volume==12,"Linked insert lost alignment");require(groups[0].loopEnd==7,"Linked insert did not shift loop");command(Undo);require(encodeBank(bank,true)==genericOriginal&&groups[0].loopEnd==6,"Linked insert undo failed");
 anchor=1;selectionEnd=2;copy();row=4;paste();for(int t=0;t<3;t++)require(bank[t].cells[4].tone==181+t*60,"Linked paste used wrong channel");command(Undo);require(encodeBank(bank,true)==genericOriginal,"Linked paste undo failed");
 anchor=1;selectionEnd=2;erase();for(int t=0;t<3;t++)require(bank[t].cells[1].tone==183+t*60,"Linked deletion lost alignment");command(Undo);
 linked=false;row=1;insert(false);require(bank[1].cells[1].volume==12&&bank[0].cells[1].volume==0,"Single-track edit changed another track");command(Undo);linked=true;
 SetWindowTextW(nameEdit,L"Arbitrary renamed track");require(groups.size()==1&&channelCount()==3,"Renaming broke generic grouping");
 saveProjectBank(L"native/build/gui-generic.afb",bank,{groups,clockHz,fps});load(L"native/build/gui-generic.afb");require(groups.size()==1&&groups[0].name=="Laser shot"&&groups[0].loopStart==2,"GUI project reload failed");
 // Native per-channel controls affect only the named channel and expose their state.
 auto clickChannel=[&](int id){auto button=GetDlgItem(mainWnd,id);require(IsWindowVisible(button)&&IsWindowEnabled(button),"Channel control unavailable");SendMessage(button,BM_CLICK,0,0);};
 clickChannel(MuteB);require(audibleMask==5,"Mute B failed");wchar_t soundCaption[40];GetWindowTextW(GetDlgItem(mainWnd,MuteB),soundCaption,40);require(wcscmp(soundCaption,L"B: MUTED")==0,"Muted state is not labeled");
 clickChannel(MuteB);require(audibleMask==7,"Unmute B failed");GetWindowTextW(GetDlgItem(mainWnd,MuteB),soundCaption,40);require(wcscmp(soundCaption,L"B: Sound ON")==0,"Sound-on state is not labeled");
 {
  // Include another group so deleting C cannot silently select an unrelated effect.
  bank.push_back(Effect{});groups.push_back({"Other effect",{{3,-1,-1}}});refresh();
  auto original=encodeBank(bank,true);auto originalGroups=groups;
  for(int ch:{1,2}){
   auto left=encode(bank[0]),right=encode(bank[ch==1?2:1]);
   clickChannel(RemoveChannelA+ch);require(bank.size()==3&&channelCount()==2&&activeGroup==0,"Channel removal selected another effect");
   require(groups[0].tracks[ch]==-1&&encode(bank[0])==left&&encode(bank[1])==right,"Channel removal changed a surviving track");
   require(!IsWindowVisible(GetDlgItem(mainWnd,RemoveChannelA+ch)),"Removed channel still has a remove button");
   command(Undo);require(encodeBank(bank,true)==original&&groups[0].tracks==originalGroups[0].tracks,"Undo did not restore removed channel");
  }
  bank.pop_back();groups.pop_back();current=0;undoStack.clear();redoStack.clear();refresh();
  clickChannel(RemoveChannelB);clickChannel(RemoveChannelC);
  require(channelCount()==1&&bank.size()==1&&!IsWindowEnabled(GetDlgItem(mainWnd,RemoveChannelA)),"Last bank track can still be removed");
  require(IsWindowVisible(GetDlgItem(mainWnd,MuteA)),"Single-channel sound control is missing");
  command(Undo);command(Undo);require(channelCount()==3,"Channel removal undo lost group");
 }

 play(0);require(loopDevice&&(loopHeaders[1].dwFlags&WHDR_INQUEUE),"Loop playback did not queue repeat buffer");auto deadline=GetTickCount64()+350;while(GetTickCount64()<deadline){MSG queued;while(PeekMessage(&queued,nullptr,0,0,PM_REMOVE)){TranslateMessage(&queued);DispatchMessage(&queued);}Sleep(5);}require(loopDevice&&liveStream,"Continuous playback stopped unexpectedly");stop();require(!loopDevice,"Loop device did not stop");
 smokeChannels=true;command(Channels);smokeChannels=false;require(dialogSmokePassed&&groups.size()==1&&groups[0].loopStart==2,"Channel dialog did not retain assignments/loop");
 captureGrid(L"native/build/generic-smoke.bmp",true);
 bank[0].cells[4].n=bank[2].cells[4].n=true;bank[0].cells[4].noise=3;bank[2].cells[4].noise=9;row=4;redraw();captureGrid(L"native/build/conflict-smoke.bmp",true);

 // Preview/cancel and apply/undo for the new tools, with actual modal dialogs.
 bank[0].cells[4].n=bank[2].cells[4].n=false;row=anchor=0;selectionEnd=5;refresh();
 for(int tool:{Curves,Generate,Envelope,Metadata,FindLibrary}){
  auto bytes=encodeBank(bank,true),metadata=encodeProject(bank,bytes,{groups,clockHz,fps});auto oldUndo=undoStack.size();
  toolSmoke=1;toolSmokePassed=false;toolSmokeError.clear();toolCommand(tool);require(toolSmokePassed&&toolSmokeError.empty(),toolSmokeError.empty()?"Tool preview failed":toolSmokeError.c_str());require(encodeProject(bank,encodeBank(bank,true),{groups,clockHz,fps})==metadata&&undoStack.size()==oldUndo,"Tool cancel modified data");
  if(tool!=FindLibrary){toolSmoke=2;toolCommand(tool);require(toolSmokeError.empty(),toolSmokeError.c_str());require(undoStack.size()==oldUndo+1,"Tool apply missing undo");history(false);require(encodeProject(bank,encodeBank(bank,true),{groups,clockHz,fps})==metadata,"Tool undo changed project");}
 }
 toolSmoke=0;dirty=true;autosave();require(fs::exists(ownRecovery())&&fs::exists(projectPath(ownRecovery())),"Autosave missing");clearRecovery();
 const wchar_t* expectedMenus[]={L"File",L"Edit",L"Sound",L"Tools",L"Help"};for(int i=0;i<5;i++){wchar_t title[32];GetMenuStringW(GetMenu(mainWnd),i,title,32,MF_BYPOSITION);require(wcscmp(title,expectedMenus[i])==0,"Top-level menu order changed");}
 // Verify every menu has an article and exercise guide navigation and layout.
 validateGuideMenus(GetMenu(mainWnd));guideSmoke=true;guideVisited=0;
 auto helpAccelerators=editorAccelerators();
 for(HWND focus:{grid,nameEdit,clockBox}){
  SetFocus(focus);MSG key{};key.hwnd=focus;key.message=WM_KEYDOWN;key.wParam=VK_F1;key.lParam=1;
  auto visited=guideVisited;require(TranslateAcceleratorW(mainWnd,helpAccelerators,&key)!=0,"F1 was not recognized");
  require(guideSmokeError.empty()&&guideVisited==visited+helpTopicCount,"F1 did not open the user guide");
 }
 DestroyAcceleratorTable(helpAccelerators);
 command(About);guideSmoke=false;
 // Workshop Cancel and preview leave the bank intact; Apply creates an undoable group.
 auto beforeWorkshop=encodeBank(bank,true);auto beforeWorkshopGroups=groups.size();auto beforeWorkshopUndo=undoStack.size();bool beforeWorkshopDirty=dirty;
 workshopSmoke=1;openWorkshop(L"native/build/test-workshop-source.wav");require(workshopSmokePassed&&workshopSmokeError.empty(),workshopSmokeError.empty()?"Workshop cancel smoke failed":workshopSmokeError.c_str());
 require(encodeBank(bank,true)==beforeWorkshop&&undoStack.size()==beforeWorkshopUndo&&dirty==beforeWorkshopDirty,"Workshop preview/cancel mutated bank");
 workshopSmoke=2;openWorkshop(L"native/build/test-workshop-source.wav");workshopSmoke=0;require(workshopSmokeError.empty()&&groups.size()==beforeWorkshopGroups+1,"Workshop Apply did not add a group");require(!effect().sourceAudio.empty()&&!effect().recipe.empty(),"Workshop source/settings not saved");auto sourceBytes=effect().sourceAudio;auto sourceRecipe=effect().recipe;auto sourceName=effect().sourcePath;
 workshopSmoke=1;openWorkshop(L"native/build/test-workshop-source.wav",sourceRecipe,sourceName);require(workshopSmokeError.empty()&&effect().sourceAudio==sourceBytes,"Reopening source changed project");workshopSmoke=0;
 command(Undo);require(encodeBank(bank,true)==beforeWorkshop&&groups.size()==beforeWorkshopGroups,"Workshop Apply undo failed");
 if(argc>3){workshopSmoke=2;openWorkshop(argv[3]);workshopSmoke=0;require(workshopSmokeError.empty()&&groups.size()==beforeWorkshopGroups+1,"MP3 workshop Apply failed");command(Undo);require(encodeBank(bank,true)==beforeWorkshop,"MP3 workshop undo changed bank");}
 if(argc>2){load(argv[2]);require(fps==60&&clockHz==1789773,"Companion playback timing was not applied");
  require(bank.size()==73&&groups.size()==60,"Castlevania grouped effect count differs");bool importedLoop=false;for(auto& g:groups)if(g.loopStart==610&&g.loopEnd==2122)importedLoop=true;require(importedLoop,"Original loop metadata was not imported");
  auto navigationBytes=encodeBank(bank,true);auto navigationUndo=undoStack.size();bool navigationDirty=dirty;
  SendMessage(grid,WM_KEYDOWN,VK_ADD,0);require(activeGroup==1,"Plus did not select next effect");SendMessage(grid,WM_KEYDOWN,VK_SUBTRACT,0);require(activeGroup==0,"Minus did not select previous effect");
  command(PreviousEffect);require(activeGroup==0&&!IsWindowEnabled(GetDlgItem(mainWnd,PreviousEffect)),"Previous navigation passed first effect");
  int fireGroup=0;for(size_t g=0;g<groups.size();g++)if(groups[g].name=="0F  fire_whip")fireGroup=int(g);selectGroup(fireGroup);require(channelCount()==2,"Navigation failed to select grouped effect");
  SendMessage(grid,WM_KEYDOWN,VK_OEM_PLUS,0);require(activeGroup==fireGroup+1,"Plus entered another channel instead of next group");SendMessage(grid,WM_KEYDOWN,VK_OEM_MINUS,0);require(activeGroup==fireGroup,"OEM minus navigation failed");
  selectGroup(int(groups.size())-1);command(NextEffect);require(activeGroup==int(groups.size())-1&&!IsWindowEnabled(GetDlgItem(mainWnd,NextEffect)),"Next navigation passed last effect");
  require(encodeBank(bank,true)==navigationBytes&&dirty==navigationDirty&&undoStack.size()==navigationUndo,"Navigation edited data or history");selectGroup(0);
  auto originalBank=encodeBank(bank,true);
  for(size_t i=0;i<bank.size();i++)if(bank[i].name=="0F_A_fire_whip")current=int(i);
  refresh();visible();require(channelCount()==2,"Fire whip channels were not grouped");
  int fireA=current,fireC=groups[activeGroup].tracks[2];auto mixed=preview(0);auto alone=preview(0,true);require(mixed!=alone,"Combined and solo playback are identical");
  dirty=false;mouse(px(65+2*laneWidth()+52),px(32),false,false,false);ReleaseCapture();dragging=false;
  require(current==fireC&&!dirty,"Channel selection edited document or selected wrong channel");
  int oldTone=effect().cells[0].tone;SendMessage(grid,WM_KEYDOWN,'1',0);require(effect().cells[0].tone==((oldTone<<4|1)&4095),"Multichannel tone edit failed");
  require(encode(bank[fireA])==encode(decodeBank(originalBank)[fireA]),"Editing C changed A");command(Undo);
  require(encodeBank(bank,true)==originalBank,"Multichannel edit undo changed bank");
  refresh();visible();captureGrid(L"native/build/multichannel-smoke.bmp",true);write(L"native/build/fire-whip-combined.wav",mixed);

  for(size_t i=0;i<bank.size();i++)if(bank[i].name=="0E_C_chain_whip")current=int(i);
  refresh();visible();write(L"native/build/chain-reference.wav",render(effect(),clockHz,fps));write(L"native/build/chain-legacy.wav",render(effect(),clockHz,fps,0,PreviewModel::Legacy));}
 captureGrid(L"native/build/gui-smoke.bmp");captureGrid(L"native/build/window-smoke.bmp",true);write(L"native/build/gui-smoke.txt",Bytes{'P','A','S','S','\n'});dirty=false;DestroyWindow(mainWnd);LocalFree(argv);return 0;}}catch(const std::exception& e){if(smoke){std::ofstream("native/build/gui-smoke.txt")<<e.what();return 2;}fail(e);}LocalFree(argv);
 SetTimer(mainWnd,90,60000,nullptr);ShowWindow(mainWnd,show);UpdateWindow(mainWnd);SetFocus(grid);auto table=editorAccelerators();MSG msg;while(GetMessage(&msg,nullptr,0,0)>0){if(msg.message==WM_KEYDOWN&&(msg.hwnd==grid||msg.hwnd==effects)){int navigation=effectNavigationKey(msg.wParam);if(navigation){command(navigation);continue;}}if(!TranslateAccelerator(mainWnd,table,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}
 DestroyAcceleratorTable(table);DeleteObject(uiFont);DeleteObject(monoFont);DeleteObject(titleFont);DeleteObject(smallFont);DeleteObject(boldFont);DeleteObject(bgBrush);DeleteObject(whiteBrush);DeleteObject(sideBrush);return int(msg.wParam);
}
