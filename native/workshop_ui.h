// Win32 Audio workshop; included after the editor's model/undo helpers.
#include "audio_decode.h"
#include <thread>
#include <atomic>
constexpr UINT WorkshopDone=WM_APP+61,WorkshopProgress=WM_APP+62;
int workshopSmoke=0;bool workshopSmokePassed=false,workshopHelpPassed=false;std::string workshopSmokeError;
struct WorkshopState {
 fs::path path;std::shared_ptr<SourceWave> source;ConvertOptions options;ConvertedWave converted,pending;
 Bytes original,ay,playback,pendingOriginal,pendingAY;std::thread worker;std::atomic<bool> cancel{false};std::string error;
 bool busy=false,valid=false,changing=false,dragging=false;double anchor=0,playOffset=0;ULONGLONG playTick=0;int playing=0,peakWidth=0;
 std::vector<std::pair<float,float>> peaks;double viewStart=0,viewEnd=0;std::vector<uint32_t> spectrogram;bool showSpectrum=false;std::string savedRecipe,savedSourcePath;
 ~WorkshopState(){cancel=true;if(worker.joinable())worker.join();}
};
WorkshopState* workshopState(HWND dlg){return (WorkshopState*)GetWindowLongPtr(dlg,DWLP_USER);}
void workshopStop(WorkshopState& state){PlaySoundW(nullptr,nullptr,0);state.playing=0;state.playback.clear();}
void workshopMessage(HWND dlg,const std::wstring& message){SetDlgItemTextW(dlg,1124,message.c_str());}
void workshopChanged(HWND dlg){auto& state=*workshopState(dlg);if(state.changing||state.busy)return;workshopStop(state);state.valid=false;state.original.clear();state.ay.clear();EnableWindow(GetDlgItem(dlg,1122),FALSE);EnableWindow(GetDlgItem(dlg,IDOK),FALSE);workshopMessage(dlg,L"Settings changed. Click Convert to update the AY preview.");InvalidateRect(GetDlgItem(dlg,1101),nullptr,FALSE);}
ConvertOptions workshopOptions(HWND dlg){auto& state=*workshopState(dlg);ConvertOptions o;o.clock=clockHz;o.rate=fps;
 auto integer=[&](int id){BOOL valid;UINT n=GetDlgItemInt(dlg,id,&valid,FALSE);require(valid,"Enter whole numbers for trim times and fades");return n;};
 o.toneSensitivity=integer(1153);o.noiseSensitivity=integer(1154);o.start=integer(1110)/1000.;o.end=integer(1111)/1000.;if(o.end<=std::ceil(state.source->duration()*1000)/1000.)o.end=std::min(o.end,state.source->duration());
 o.channels=int(SendDlgItemMessage(dlg,1113,CB_GETCURSEL,0,0))+1;o.normalize=IsDlgButtonChecked(dlg,1114)==BST_CHECKED;o.attackMs=integer(1115);o.decayMs=integer(1116);o.pitch=int(SendDlgItemMessage(dlg,1117,CB_GETCURSEL,0,0))-24;o.brightness=double(SendDlgItemMessage(dlg,1118,TBM_GETPOS,0,0));validateConversion(*state.source,o);return o;
}
void workshopRange(HWND dlg,double start,double end){auto& state=*workshopState(dlg);state.changing=true;SetDlgItemInt(dlg,1110,UINT(std::max(0.,std::floor(start*1000))),FALSE);SetDlgItemInt(dlg,1111,UINT(std::max(1.,std::floor(end*1000))),FALSE);state.changing=false;workshopChanged(dlg);}
void workshopLoad(HWND dlg,const fs::path& path){auto& state=*workshopState(dlg);auto source=std::make_shared<SourceWave>(decodeAudioFile(path));workshopStop(state);state.source=source;state.path=path;state.peaks.clear();state.peakWidth=0;state.viewStart=0;state.viewEnd=source->duration();state.spectrogram.clear();state.showSpectrum=false;CheckDlgButton(dlg,1152,BST_UNCHECKED);
 std::wstringstream label;label<<path.filename().wstring()<<L"  |  "<<source->rate<<L" Hz  |  "<<(source->channels==2?L"stereo to mono":L"mono")<<L"  |  "<<std::fixed<<std::setprecision(3)<<source->duration()<<L" s";SetDlgItemTextW(dlg,1102,label.str().c_str());
 workshopRange(dlg,0,std::min(source->duration(),double(MaxFrames)/fps));workshopMessage(dlg,L"Select a range, choose 1-3 channels, then Convert. Maximum: 4096 AY frames.");
}
void workshopBusy(HWND dlg,bool busy){auto& state=*workshopState(dlg);state.busy=busy;
 for(int id:{1100,1101,1110,1111,1112,1113,1114,1115,1116,1117,1118,1150,1151,1152,1153,1154,1121,1122,1125,IDOK})EnableWindow(GetDlgItem(dlg,id),!busy);
 EnableWindow(GetDlgItem(dlg,IDOK),!busy&&state.valid);EnableWindow(GetDlgItem(dlg,1122),!busy&&state.valid);SetDlgItemTextW(dlg,1120,busy?L"Cancel conversion":L"Convert");
}
void workshopConvert(HWND dlg){auto& state=*workshopState(dlg);if(state.busy){state.cancel=true;workshopMessage(dlg,L"Cancelling conversion...");return;}
 auto options=workshopOptions(dlg);workshopStop(state);if(state.worker.joinable())state.worker.join();state.options=options;state.valid=false;state.error.clear();state.cancel=false;workshopBusy(dlg,true);workshopMessage(dlg,L"Analyzing audio...");auto source=state.source;
 try{state.worker=std::thread([dlg,&state,source,options](){try{auto cancel=[&state](){return state.cancel.load();};state.pending=convertWave(*source,options,cancel,[dlg](int n){PostMessageW(dlg,WorkshopProgress,n,0);});
  state.pendingOriginal=pcmWave(selectedSamples(*source,options.start,options.end,SampleRate,cancel));if(cancel())throw std::runtime_error("Conversion cancelled");state.pendingAY=convertedPreview(state.pending,options);
 }catch(const std::exception& e){state.error=e.what();}PostMessageW(dlg,WorkshopDone,0,0);});}catch(...){workshopBusy(dlg,false);throw;}
}
void workshopListen(HWND dlg,int which){auto& state=*workshopState(dlg);if(which==2)require(state.valid,"Convert the selection first");
 if(state.original.empty()){auto o=workshopOptions(dlg);state.original=pcmWave(selectedSamples(*state.source,o.start,o.end,SampleRate));}
 double duration=double(state.original.size()-44)/(2*SampleRate),offset=0;if(state.playing){offset=state.playOffset+(GetTickCount64()-state.playTick)/1000.;if(offset>=duration)offset=0;}
 workshopStop(state);state.playOffset=offset;state.playTick=GetTickCount64();state.playback=seekWave(which==1?state.original:state.ay,offset);
 require(PlaySoundW((LPCWSTR)state.playback.data(),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT)!=FALSE,"Cannot play WAV preview");state.playing=which;workshopMessage(dlg,which==1?L"Playing original selection. Switch to AY result to compare at the same position.":L"Playing AY result. Switch to Original to compare at the same position.");
}
void workshopSpectrum(WorkshopState& s){s.spectrogram.clear();if(!s.showSpectrum)return;
 auto samples=selectedSamples(*s.source,s.viewStart,s.viewEnd,AnalysisRate);std::vector<double> power(256*64);double peak=1e-12;
 for(int x=0;x<256;x++){auto bins=spectrum(samples,int((x+.5)*samples.size()/256));for(int y=0;y<64;y++){double hz=80*std::pow(125.,double(63-y)/63);int bin=std::clamp(int(hz*FFTSize/AnalysisRate),1,FFTSize/2);double v=bins[bin];power[y*256+x]=v;peak=std::max(peak,v);}}
 s.spectrogram.resize(power.size());for(size_t i=0;i<power.size();i++){double v=std::clamp((10*std::log10(power[i]/peak+1e-12)+65)/65,0.,1.);s.spectrogram[i]=(uint32_t(20+230*v)<<16)|(uint32_t(20+200*v*v)<<8)|uint32_t(45+70*(1-v));}
}
void workshopApply(HWND dlg){auto& state=*workshopState(dlg);require(state.valid&&!state.busy,"Convert the current settings before applying");Bank next=bank;auto nextGroups=groups;bool replace=SendDlgItemMessage(dlg,1125,CB_GETCURSEL,0,0)==1;
 auto converted=state.converted;converted.tracks[0].sourceAudio=state.original;converted.tracks[0].sourcePath=state.savedSourcePath.empty()?state.path.u8string():state.savedSourcePath;std::ostringstream recipe;auto& o=state.options;recipe<<o.channels<<' '<<o.attackMs<<' '<<o.decayMs<<' '<<o.brightness<<' '<<o.pitch<<' '<<o.normalize<<' '<<o.toneSensitivity<<' '<<o.noiseSensitivity;converted.tracks[0].recipe=recipe.str();
 int group=applyConverted(next,nextGroups,activeGroup,converted,(state.savedSourcePath.empty()?state.path:fs::u8path(state.savedSourcePath)).stem().u8string(),replace);remember();stop();bank=std::move(next);groups=std::move(nextGroups);activeGroup=group;for(int t:groups[group].tracks)if(t>=0){current=t;break;}row=first=anchor=selectionEnd=0;refresh();visible();workshopStop(state);EndDialog(dlg,1);
}
LRESULT CALLBACK workshopWaveProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){HWND dlg=GetParent(hwnd);auto* state=workshopState(dlg);if(!state)return DefWindowProcW(hwnd,message,wp,lp);
 switch(message){case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT r;GetClientRect(hwnd,&r);auto mem=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,std::max(1,int(r.right)),std::max(1,int(r.bottom)));auto old=SelectObject(mem,bitmap);
  auto brush=CreateSolidBrush(RGB(23,38,54));FillRect(mem,&r,brush);DeleteObject(brush);
  if(state->source){int w=r.right,h=std::max(1,int(r.bottom)-px(26));auto& samples=state->source->mono;
   if(state->peakWidth!=w){state->peaks.assign(w,{0.f,0.f});for(int x=0;x<w;x++){size_t begin=size_t(state->viewStart*state->source->rate),count=size_t((state->viewEnd-state->viewStart)*state->source->rate);size_t a=begin+size_t(uint64_t(x)*count/w),b=std::max(a+1,begin+size_t(uint64_t(x+1)*count/w));float low=0,high=0;for(size_t i=a;i<std::min(b,samples.size());i++){low=std::min(low,samples[i]);high=std::max(high,samples[i]);}state->peaks[x]={low,high};}state->peakWidth=w;}
   double duration=state->viewEnd-state->viewStart;double a=GetDlgItemInt(dlg,1110,nullptr,FALSE)/1000.,b=GetDlgItemInt(dlg,1111,nullptr,FALSE)/1000.;int left=int(std::clamp((a-state->viewStart)/duration,0.,1.)*w),right=int(std::clamp((b-state->viewStart)/duration,0.,1.)*w);
   RECT selected{left,0,right,h};brush=CreateSolidBrush(RGB(30,79,83));FillRect(mem,&selected,brush);DeleteObject(brush);
   if(state->showSpectrum&&!state->spectrogram.empty()){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=256;info.bmiHeader.biHeight=-64;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;StretchDIBits(mem,0,0,w,h,0,0,256,64,state->spectrogram.data(),&info,DIB_RGB_COLORS,SRCCOPY);RECT edge{left,0,right,h};FrameRect(mem,&edge,(HBRUSH)GetStockObject(WHITE_BRUSH));}
   auto pen=CreatePen(PS_SOLID,1,RGB(84,218,194));auto oldPen=SelectObject(mem,pen);if(!state->showSpectrum)for(int x=0;x<w;x++){MoveToEx(mem,x,h/2-int(state->peaks[x].second*h*.44),nullptr);LineTo(mem,x,h/2-int(state->peaks[x].first*h*.44)+1);}SelectObject(mem,oldPen);DeleteObject(pen);
   if(state->playing){double time=a+state->playOffset+(GetTickCount64()-state->playTick)/1000.;int x=int((time-state->viewStart)/duration*w);RECT cursor{x,0,x+2,h};FillRect(mem,&cursor,(HBRUSH)GetStockObject(WHITE_BRUSH));}
   SelectObject(mem,uiFont);SetBkMode(mem,TRANSPARENT);SetTextColor(mem,RGB(190,211,222));wchar_t text[80];RECT ruler{6,h+2,w-6,r.bottom};swprintf_s(text,L"%.3f s",state->viewStart);DrawTextW(mem,text,-1,&ruler,DT_LEFT|DT_SINGLELINE|DT_VCENTER);swprintf_s(text,L"%.3f s selected",std::max(0.,b-a));DrawTextW(mem,text,-1,&ruler,DT_CENTER|DT_SINGLELINE|DT_VCENTER);swprintf_s(text,L"%.3f s",state->viewEnd);DrawTextW(mem,text,-1,&ruler,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
  }BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);EndPaint(hwnd,&ps);return 0;}
 case WM_LBUTTONDOWN:if(!state->busy&&state->source){RECT r;GetClientRect(hwnd,&r);state->anchor=state->viewStart+std::clamp(double(GET_X_LPARAM(lp))/std::max(1,int(r.right)),0.,1.)*(state->viewEnd-state->viewStart);state->dragging=true;SetCapture(hwnd);}return 0;
 case WM_MOUSEMOVE:if(state->dragging&&(wp&MK_LBUTTON)){RECT r;GetClientRect(hwnd,&r);double end=state->viewStart+std::clamp(double(GET_X_LPARAM(lp))/std::max(1,int(r.right)),0.,1.)*(state->viewEnd-state->viewStart);workshopRange(dlg,std::min(end,state->anchor),std::max(end,state->anchor));}return 0;
 case WM_LBUTTONUP:state->dragging=false;ReleaseCapture();return 0;case WM_CAPTURECHANGED:state->dragging=false;return 0;
 }return DefWindowProcW(hwnd,message,wp,lp);
}
// Modal dialogs have their own message loop; route F1 from every child control.
LRESULT CALLBACK workshopHelpKey(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR owner){
 if(message==WM_GETDLGCODE&&lp&&((MSG*)lp)->wParam==VK_F1)
  return DefSubclassProc(hwnd,message,wp,lp)|DLGC_WANTMESSAGE;
 if(message==WM_KEYDOWN&&wp==VK_F1){if(!(lp&(LPARAM(1)<<30)))SendMessage((HWND)owner,WM_COMMAND,IDHELP,0);return 0;}
 if(message==WM_KEYUP&&wp==VK_F1)return 0;
 if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,workshopHelpKey,id);
 return DefSubclassProc(hwnd,message,wp,lp);
}
BOOL CALLBACK workshopHelpControl(HWND child,LPARAM owner){SetWindowSubclass(child,workshopHelpKey,1,(DWORD_PTR)owner);return TRUE;}
void workshopHelp(HWND dlg){
 auto before=guideVisited;openGuide(dlg,ImportWav);
 if(workshopSmoke==1){workshopHelpPassed=guideSmokeError.empty()&&guideVisited==before+helpTopicCount;guideSmoke=false;KillTimer(dlg,2);require(workshopHelpPassed,"Workshop F1 did not open its help topic");PostMessage(dlg,WM_COMMAND,1120,0);}
}
// Size single-line fields to their actual dialog font and align label centers
// with the closed controls (a combo's resource height describes its dropdown).
void workshopAlignFields(HWND dlg){
 HDC dc=GetDC(dlg);auto old=SelectObject(dc,(HFONT)SendDlgItemMessage(dlg,1110,WM_GETFONT,0,0));
 TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(dlg,dc);
 int fieldHeight=metrics.tmHeight+2*GetSystemMetrics(SM_CYEDGE);
 for(int id:{1110,1111,1115,1116,1153,1154}){
  HWND field=GetDlgItem(dlg,id);RECT r;GetWindowRect(field,&r);
  SetWindowPos(field,nullptr,0,0,r.right-r.left,fieldHeight,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
 }
 for(auto pair:{std::pair<int,int>{1140,1110},{1141,1111},{1142,1113},{1143,1115},{1144,1116},{1145,1117},{1146,1125},{1155,1153},{1156,1154}}){
  HWND label=GetDlgItem(dlg,pair.first);RECT r,field;
  GetWindowRect(label,&r);MapWindowPoints(nullptr,dlg,(POINT*)&r,2);
  GetWindowRect(GetDlgItem(dlg,pair.second),&field);MapWindowPoints(nullptr,dlg,(POINT*)&field,2);
  SetWindowLongPtr(label,GWL_STYLE,GetWindowLongPtr(label,GWL_STYLE)|SS_CENTERIMAGE);
  SetWindowPos(label,nullptr,r.left,field.top,r.right-r.left,field.bottom-field.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
 }
 // The range button and checkbox share the compact first row.
 RECT row;GetWindowRect(GetDlgItem(dlg,1110),&row);MapWindowPoints(nullptr,dlg,(POINT*)&row,2);
 for(int id:{1112,1114}){HWND control=GetDlgItem(dlg,id);RECT r;GetWindowRect(control,&r);MapWindowPoints(nullptr,dlg,(POINT*)&r,2);
  SetWindowPos(control,nullptr,r.left,row.top,r.right-r.left,fieldHeight,SWP_NOZORDER|SWP_NOACTIVATE);}
}
INT_PTR CALLBACK workshopProc(HWND dlg,UINT message,WPARAM wp,LPARAM lp){if(message==WM_INITDIALOG)SetWindowLongPtr(dlg,DWLP_USER,lp);auto* state=workshopState(dlg);if(!state)return FALSE;
 try{switch(message){case WM_INITDIALOG:{EnumChildWindows(dlg,workshopHelpControl,(LPARAM)dlg);state->changing=true;for(auto s:{L"1",L"2",L"3"})SendDlgItemMessageW(dlg,1113,CB_ADDSTRING,0,(LPARAM)s);SendDlgItemMessage(dlg,1113,CB_SETCURSEL,1,0);
  for(int i=-24;i<=24;i++){auto label=std::to_wstring(i);SendDlgItemMessageW(dlg,1117,CB_ADDSTRING,0,(LPARAM)label.c_str());}SendDlgItemMessage(dlg,1117,CB_SETCURSEL,24,0);SendDlgItemMessage(dlg,1118,TBM_SETRANGE,TRUE,MAKELPARAM(-100,100));SendDlgItemMessage(dlg,1118,TBM_SETTICFREQ,25,0);
  for(auto s:{L"New effect (keep existing sounds)",L"Replace selected effect channels"})SendDlgItemMessageW(dlg,1125,CB_ADDSTRING,0,(LPARAM)s);SendDlgItemMessage(dlg,1125,CB_SETCURSEL,0,0);
  SetDlgItemInt(dlg,1153,50,FALSE);SetDlgItemInt(dlg,1154,50,FALSE);workshopAlignFields(dlg);CheckDlgButton(dlg,1114,BST_CHECKED);SetDlgItemInt(dlg,1115,0,FALSE);SetDlgItemInt(dlg,1116,0,FALSE);state->changing=false;workshopLoad(dlg,state->path);if(!state->savedRecipe.empty()){ConvertOptions o;std::istringstream recipe(state->savedRecipe);if(recipe>>o.channels>>o.attackMs>>o.decayMs>>o.brightness>>o.pitch>>o.normalize>>o.toneSensitivity>>o.noiseSensitivity){require(o.channels>=1&&o.channels<=3&&o.attackMs>=0&&o.attackMs<=2000&&o.decayMs>=0&&o.decayMs<=2000&&o.pitch>=-24&&o.pitch<=24&&o.brightness>=-100&&o.brightness<=100&&o.toneSensitivity>=0&&o.toneSensitivity<=100&&o.noiseSensitivity>=0&&o.noiseSensitivity<=100,"Saved conversion settings are invalid");state->changing=true;SendDlgItemMessage(dlg,1113,CB_SETCURSEL,o.channels-1,0);SetDlgItemInt(dlg,1115,int(o.attackMs),FALSE);SetDlgItemInt(dlg,1116,int(o.decayMs),FALSE);SendDlgItemMessage(dlg,1118,TBM_SETPOS,TRUE,int(o.brightness));SetDlgItemTextW(dlg,1119,std::to_wstring(int(o.brightness)).c_str());SendDlgItemMessage(dlg,1117,CB_SETCURSEL,int(o.pitch)+24,0);CheckDlgButton(dlg,1114,o.normalize?BST_CHECKED:BST_UNCHECKED);SetDlgItemInt(dlg,1153,int(o.toneSensitivity),FALSE);SetDlgItemInt(dlg,1154,int(o.noiseSensitivity),FALSE);state->changing=false;}}workshopBusy(dlg,false);SetTimer(dlg,1,33,nullptr);if(workshopSmoke){workshopRange(dlg,0,std::min(.5,state->source->duration()));
   if(workshopSmoke==1){workshopHelpPassed=false;guideSmoke=true;SetTimer(dlg,2,3000,nullptr);SetFocus(GetDlgItem(dlg,1110));PostMessage(GetDlgItem(dlg,1110),WM_KEYDOWN,VK_F1,1);PostMessage(GetDlgItem(dlg,1110),WM_KEYUP,VK_F1,LPARAM(0xc0000001));}
   else PostMessage(dlg,WM_COMMAND,1120,0);
  }return TRUE;}
 case WorkshopProgress:if(state->busy)workshopMessage(dlg,L"Analyzing and rendering: "+std::to_wstring(wp)+L"%");return TRUE;
 case WorkshopDone:if(state->worker.joinable())state->worker.join();if(state->cancel&&state->error.empty())state->error="Conversion cancelled";
  if(state->error.empty()){state->converted=std::move(state->pending);state->original=std::move(state->pendingOriginal);state->ay=std::move(state->pendingAY);state->valid=true;workshopMessage(dlg,L"Ready: "+std::to_wstring(state->converted.frames)+L" frames, "+std::to_wstring(state->converted.tracks.size())+L" channels. Compare Original / AY, then Apply.");}else workshopMessage(dlg,widen(state->error));workshopBusy(dlg,false);
  if(workshopSmoke){require(state->valid,"Workshop conversion failed");SendMessage(dlg,WM_COMMAND,1150,0);CheckDlgButton(dlg,1152,BST_CHECKED);SendMessage(dlg,WM_COMMAND,1152,0);require(state->spectrogram.size()==256*64&&state->viewEnd<=.501,"Workshop zoom/spectrum failed");workshopListen(dlg,1);workshopListen(dlg,2);SendMessage(dlg,WM_COMMAND,1123,0);require(IsWindowEnabled(GetDlgItem(dlg,IDOK))&&IsWindowEnabled(GetDlgItem(dlg,1100)),"Workshop controls remained disabled");RedrawWindow(dlg,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);workshopSmokePassed=state->valid;captureGrid(L"native/build/workshop-smoke.bmp",true,dlg);if(workshopSmoke==2)workshopApply(dlg);else EndDialog(dlg,0);}return TRUE;
 case WM_TIMER:if(wp==2&&workshopSmoke==1){KillTimer(dlg,2);guideSmoke=false;require(workshopHelpPassed,"Workshop F1 was not delivered");}if(state->playing){double duration=double(state->original.size()-44)/(2*SampleRate);if(state->playOffset+(GetTickCount64()-state->playTick)/1000.>=duration)workshopStop(*state);InvalidateRect(GetDlgItem(dlg,1101),nullptr,FALSE);}return TRUE;
 case WM_HSCROLL:SetDlgItemTextW(dlg,1119,std::to_wstring(SendDlgItemMessage(dlg,1118,TBM_GETPOS,0,0)).c_str());workshopChanged(dlg);return TRUE;
 case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);
  if(id==IDCANCEL){state->cancel=true;if(state->worker.joinable())state->worker.join();workshopStop(*state);EndDialog(dlg,0);return TRUE;}
  if(id==IDHELP){workshopHelp(dlg);return TRUE;}
  if(state->changing)return TRUE;
  if(id==1120){workshopConvert(dlg);return TRUE;}if(id==1121||id==1122){workshopListen(dlg,id==1121?1:2);return TRUE;}if(id==1123){workshopStop(*state);workshopMessage(dlg,state->valid?L"Stopped. Compare Original / AY, or Apply the result.":L"Stopped. Click Convert to create the AY preview.");InvalidateRect(GetDlgItem(dlg,1101),nullptr,FALSE);return TRUE;}if(id==IDOK){workshopApply(dlg);return TRUE;}
  if(id==1150||id==1151||id==1152){if(id==1150){auto o=workshopOptions(dlg);state->viewStart=o.start;state->viewEnd=o.end;}if(id==1151){state->viewStart=0;state->viewEnd=state->source->duration();}state->showSpectrum=IsDlgButtonChecked(dlg,1152)==BST_CHECKED;state->peakWidth=0;workshopSpectrum(*state);InvalidateRect(GetDlgItem(dlg,1101),nullptr,FALSE);return TRUE;}
  if(id==1112){workshopRange(dlg,0,state->source->duration());return TRUE;}
  if(id==1100){wchar_t path[32768]={};OPENFILENAMEW open{sizeof(open)};open.hwndOwner=dlg;open.lpstrFilter=L"Audio (*.wav;*.mp3)\0*.wav;*.mp3\0WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0\0";open.lpstrFile=path;open.nMaxFile=32768;open.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&open)){state->savedSourcePath.clear();workshopLoad(dlg,path);}return TRUE;}
  if(((id==1110||id==1111||id==1115||id==1116||id==1153||id==1154)&&event==EN_CHANGE)||((id==1113||id==1117)&&event==CBN_SELCHANGE)||id==1114){workshopChanged(dlg);return TRUE;}break;}
 case WM_KEYDOWN:if(wp==VK_F1){if(!(lp&(LPARAM(1)<<30)))workshopHelp(dlg);return TRUE;}break;
 case WM_HELP:workshopHelp(dlg);return TRUE;
 case WM_CLOSE:SendMessage(dlg,WM_COMMAND,IDCANCEL,0);return TRUE;
 }}catch(const std::exception& e){if(workshopSmoke){guideSmoke=false;workshopSmokeError=e.what();state->cancel=true;if(state->worker.joinable())state->worker.join();workshopStop(*state);EndDialog(dlg,-1);}else {MessageBoxW(dlg,widen(e.what()).c_str(),L"Audio workshop",MB_ICONERROR);if(message==WM_INITDIALOG)EndDialog(dlg,-1);}}return FALSE;
}
void openWorkshop(const fs::path& path,const std::string& savedRecipe="",const std::string& savedSourcePath=""){stop();INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=workshopWaveProc;wc.lpszClassName=L"AYFXWave";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);RegisterClassW(&wc);
 WorkshopState state;state.path=path;state.savedRecipe=savedRecipe;state.savedSourcePath=savedSourcePath;DialogBoxParamW(instance,MAKEINTRESOURCEW(3),mainWnd,workshopProc,(LPARAM)&state);workshopStop(state);
}
