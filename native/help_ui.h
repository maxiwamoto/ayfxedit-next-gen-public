#pragma once
#include "help_content.h"
bool guideSmoke=false;std::string guideSmokeError;size_t guideVisited=0;
struct GuideState {int initial=GuideStart;std::vector<HTREEITEM> items;};
int guideIndex(int command){for(size_t i=0;i<helpTopicCount;i++)if(helpTopics[i].command==command)return int(i);return 0;}
void guideArticle(HWND dlg,int index){
 const auto& topic=helpTopics[index];HWND text=GetDlgItem(dlg,1202);
 std::wstring article=std::wstring(topic.title)+L"\r\n\r\n"+topic.body;
 SetWindowTextW(text,article.c_str());
 CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_FACE|CFM_SIZE|CFM_COLOR|CFM_BOLD;
 format.yHeight=200;format.crTextColor=RGB(36,49,63);wcscpy_s(format.szFaceName,L"Segoe UI");
 SendMessageW(text,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&format);
 SendMessage(text,EM_SETSEL,0,wcslen(topic.title));format.yHeight=290;format.dwEffects=CFE_BOLD;format.crTextColor=RGB(16,115,107);
 SendMessageW(text,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
 SendMessage(text,EM_SETSEL,0,0);SendMessage(text,WM_VSCROLL,SB_TOP,0);
}
void guideLayout(HWND dlg){
 RECT r;GetClientRect(dlg,&r);RECT u{10,25,158,18};MapDialogRect(dlg,&u);
 int margin=u.left,top=u.top,treeWidth=u.right,buttonHeight=u.bottom;
 int footer=r.bottom-margin-buttonHeight,contentBottom=footer-margin;
 MoveWindow(GetDlgItem(dlg,1200),margin,margin,treeWidth,top-margin,TRUE);
 MoveWindow(GetDlgItem(dlg,1201),margin,top,treeWidth,contentBottom-top,TRUE);
 int left=margin*2+treeWidth;
 MoveWindow(GetDlgItem(dlg,1202),left,margin,r.right-left-margin,contentBottom-margin,TRUE);
 RECT button{0,0,66,18};MapDialogRect(dlg,&button);
 MoveWindow(GetDlgItem(dlg,IDCANCEL),r.right-margin-button.right,footer,button.right,buttonHeight,TRUE);
 MoveWindow(GetDlgItem(dlg,1203),margin,footer,r.right-button.right-margin*3,buttonHeight,TRUE);
 RECT text;GetClientRect(GetDlgItem(dlg,1202),&text);InflateRect(&text,-margin/2,-margin/2);
 SendDlgItemMessage(dlg,1202,EM_SETRECT,0,(LPARAM)&text);
}
INT_PTR CALLBACK guideProc(HWND dlg,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_INITDIALOG)SetWindowLongPtr(dlg,DWLP_USER,lp);
 auto* state=(GuideState*)GetWindowLongPtr(dlg,DWLP_USER);if(!state)return FALSE;
 try{switch(message){
 case WM_INITDIALOG:{
  HWND tree=GetDlgItem(dlg,1201);SetWindowTheme(tree,L"Explorer",nullptr);SendDlgItemMessage(dlg,1202,EM_SETBKGNDCOLOR,0,RGB(255,255,255));
  const wchar_t* previous=nullptr;HTREEITEM parent=nullptr;
  for(size_t i=0;i<helpTopicCount;i++){
   const auto& topic=helpTopics[i];TVINSERTSTRUCTW insert{};insert.hInsertAfter=TVI_LAST;insert.item.mask=TVIF_TEXT|TVIF_PARAM;
   if(!previous||wcscmp(previous,topic.section)!=0){
    insert.hParent=TVI_ROOT;insert.item.pszText=const_cast<wchar_t*>(topic.section);insert.item.lParam=i;
    parent=(HTREEITEM)SendMessageW(tree,TVM_INSERTITEMW,0,(LPARAM)&insert);previous=topic.section;
   }
   insert.hParent=parent;insert.item.pszText=const_cast<wchar_t*>(topic.title);insert.item.lParam=i;
   state->items.push_back((HTREEITEM)SendMessageW(tree,TVM_INSERTITEMW,0,(LPARAM)&insert));
  }
  auto selected=state->items[guideIndex(state->initial)];TreeView_EnsureVisible(tree,selected);TreeView_SelectItem(tree,selected);
  guideLayout(dlg);if(guideSmoke)SetTimer(dlg,1,80,nullptr);return TRUE;
 }
 case WM_SIZE:guideLayout(dlg);return TRUE;
 case WM_GETMINMAXINFO:{RECT min{0,0,440,240};MapDialogRect(dlg,&min);AdjustWindowRectEx(&min,(DWORD)GetWindowLongPtr(dlg,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtr(dlg,GWL_EXSTYLE));((MINMAXINFO*)lp)->ptMinTrackSize={min.right-min.left,min.bottom-min.top};return TRUE;}
 case WM_NOTIFY:{auto* event=(NMHDR*)lp;if(event->idFrom==1201&&event->code==TVN_SELCHANGEDW){auto* change=(NMTREEVIEWW*)lp;guideArticle(dlg,int(change->itemNew.lParam));return TRUE;}break;}
 case WM_COMMAND:if(LOWORD(wp)==IDCANCEL){EndDialog(dlg,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(dlg,0);return TRUE;
 case WM_TIMER:if(guideSmoke){
  KillTimer(dlg,1);
  for(size_t i=0;i<state->items.size();i++){TreeView_SelectItem(GetDlgItem(dlg,1201),state->items[i]);require(GetWindowTextLengthW(GetDlgItem(dlg,1202))>int(wcslen(helpTopics[i].title)+10),"Missing help article");++guideVisited;}
  TreeView_SelectItem(GetDlgItem(dlg,1201),state->items[guideIndex(ImportWav)]);
  TreeView_SelectSetFirstVisible(GetDlgItem(dlg,1201),TreeView_GetParent(GetDlgItem(dlg,1201),state->items[guideIndex(ImportWav)]));
  UpdateWindow(dlg);captureGrid(L"native/build/guide-smoke.bmp",true,dlg);
  RECT r;GetWindowRect(dlg,&r);SetWindowPos(dlg,nullptr,0,0,int((r.right-r.left)*.85),int((r.bottom-r.top)*.85),SWP_NOMOVE|SWP_NOZORDER);
  TreeView_SelectItem(GetDlgItem(dlg,1201),state->items[guideIndex(GuideCredits)]);
  UpdateWindow(dlg);captureGrid(L"native/build/guide-small-smoke.bmp",true,dlg);EndDialog(dlg,0);return TRUE;
 }break;
 }}catch(const std::exception& e){guideSmokeError=e.what();if(!guideSmoke)MessageBoxW(dlg,widen(e.what()).c_str(),L"User guide",MB_ICONERROR);EndDialog(dlg,-1);}return FALSE;
}
void openGuide(HWND owner,int topic=GuideStart){
 static HMODULE richEdit=LoadLibraryW(L"Msftedit.dll");require(richEdit!=nullptr,"Cannot load Windows Rich Edit");
 INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_TREEVIEW_CLASSES};InitCommonControlsEx(&controls);
 GuideState state;state.initial=topic;require(DialogBoxParamW(instance,MAKEINTRESOURCEW(4),owner,guideProc,(LPARAM)&state)!=-1,"Cannot open the user guide");
}
INT_PTR CALLBACK aboutProc(HWND dlg,UINT message,WPARAM wp,LPARAM){
 switch(message){
 case WM_INITDIALOG:
  SendDlgItemMessage(dlg,1300,WM_SETFONT,(WPARAM)boldFont,TRUE);
  SetDlgItemTextW(dlg,1301,L"\u00A9 2026 Max Iwamoto\nNEXT GEN interface and additions.");
  if(guideSmoke)SetTimer(dlg,1,80,nullptr);return TRUE;
 case WM_COMMAND:
  switch(LOWORD(wp)){
  case IDCANCEL:EndDialog(dlg,0);return TRUE;
  case 1302:case 1303:{
   auto url=LOWORD(wp)==1302?L"https://shiru.untergrund.net/software.shtml":L"https://github.com/Threetwosevensixseven/ayfxedit-improved";
   if((INT_PTR)ShellExecuteW(dlg,L"open",url,nullptr,nullptr,SW_SHOWNORMAL)<=32)MessageBoxW(dlg,url,L"Open this address in your browser",MB_OK);return TRUE;}
  case 1304:try{openGuide(dlg,GuideCredits);}catch(const std::exception& e){MessageBoxW(dlg,widen(e.what()).c_str(),L"User guide",MB_ICONERROR);}return TRUE;
  }break;
 case WM_TIMER:if(guideSmoke){KillTimer(dlg,1);UpdateWindow(dlg);captureGrid(L"native/build/about-smoke.bmp",true,dlg);EndDialog(dlg,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(dlg,0);return TRUE;
 }return FALSE;
}
void validateGuideMenus(HMENU menu){
 for(int i=0;i<GetMenuItemCount(menu);i++){auto submenu=GetSubMenu(menu,i);if(submenu){validateGuideMenus(submenu);continue;}UINT id=GetMenuItemID(menu,i);if(id==0||id==UINT(-1))continue;
 bool found=false;for(const auto& topic:helpTopics)if(topic.command==int(id))found=true;require(found,"A menu command is missing from the user guide");
 }
}
