#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <limits.h>
#include <new>
typedef struct{double x1,y1,x2,y2,width;unsigned color;}Segment;
static std::vector<Segment> lines;
static size_t count=0;static int down=1,dirty=1,generation=0;
static double px=0,py=0,angle=0;
static unsigned ink=0x7055da;
static CRITICAL_SECTION guard;
static HWND window;
static HDC canvas;
static HBITMAP bitmap;
static HGDIOBJ oldBitmap;
static int canvasWidth,canvasHeight,paintGeneration=-1;static size_t painted=0;
// The retained geometry has no coordinate cap. Clamp only when converting to
// the integer-coordinate Windows raster API, never in the language or SVG.
static int raster(long double n){const long double lo=INT_MIN/4,hi=INT_MAX/4;if(n<lo)return (int)lo;if(n>hi)return (int)hi;return (int)roundl(n);}
static COLORREF color(unsigned c){return RGB((c>>16)&255,(c>>8)&255,c&255);}
static DWORD WINAPI reader(void *unused){
 char b[512];
 while(fgets(b,sizeof(b),stdin)){
  Segment s;double x,y,a;int p;unsigned c;
  EnterCriticalSection(&guard);
  if(b[0]=='C'){count=0;lines.clear();generation++;}
  else if(sscanf(b,"L %lf %lf %lf %lf %x %lf",&s.x1,&s.y1,&s.x2,&s.y2,&s.color,&s.width)==6&&isfinite(s.x1)&&isfinite(s.y1)&&isfinite(s.x2)&&isfinite(s.y2)&&isfinite(s.width)&&s.width>0){try{lines.push_back(s);count=lines.size();}catch(const std::bad_alloc&){LeaveCriticalSection(&guard);fclose(stdin);MessageBoxW(window,L"Not enough memory to retain more drawing segments.",L"Tally drawing",MB_ICONERROR);return 1;}}
  else if(sscanf(b,"P %lf %lf %lf %d %x",&x,&y,&a,&p,&c)==5&&isfinite(x)&&isfinite(y)&&isfinite(a)){px=x;py=y;angle=a;down=p;ink=c;}
  dirty=1;LeaveCriticalSection(&guard);
 }
 return 0;
}
static void releaseCanvas(void){if(canvas){SelectObject(canvas,oldBitmap);DeleteObject(bitmap);DeleteDC(canvas);canvas=NULL;}}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_TIMER:{EnterCriticalSection(&guard);int repaint=dirty;dirty=0;LeaveCriticalSection(&guard);if(repaint)InvalidateRect(h,NULL,FALSE);return 0;}
 case WM_SIZE:InvalidateRect(h,NULL,FALSE);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{
  PAINTSTRUCT ps;HDC target=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);int width=r.right,height=r.bottom;
  if(width<=0||height<=0){EndPaint(h,&ps);return 0;}
  if(!canvas||width!=canvasWidth||height!=canvasHeight){releaseCanvas();canvas=CreateCompatibleDC(target);bitmap=CreateCompatibleBitmap(target,width,height);if(!canvas||!bitmap){if(canvas)DeleteDC(canvas);if(bitmap)DeleteObject(bitmap);canvas=NULL;EndPaint(h,&ps);return 0;}oldBitmap=SelectObject(canvas,bitmap);canvasWidth=width;canvasHeight=height;paintGeneration=-1;}
  double scale=fmin(width/800.0,height/600.0),cx=width/2.0,cy=height/2.0;
  EnterCriticalSection(&guard);
  if(paintGeneration!=generation){HBRUSH bg=CreateSolidBrush(RGB(250,249,246));FillRect(canvas,&r,bg);DeleteObject(bg);painted=0;paintGeneration=generation;}
  // Only new segments are rasterized. Reuse the pen while its style is unchanged.
  HPEN pen=NULL;HGDIOBJ oldPen=NULL;unsigned lastColor=0;int lastWidth=0;
  for(size_t i=painted;i<count;i++){
   Segment s=lines[i];int pw=raster(fmaxl(1,(long double)s.width*scale));
   if(!pen||s.color!=lastColor||pw!=lastWidth){if(pen){SelectObject(canvas,oldPen);DeleteObject(pen);}pen=CreatePen(PS_SOLID,pw,color(s.color));oldPen=SelectObject(canvas,pen);lastColor=s.color;lastWidth=pw;}
   MoveToEx(canvas,raster(cx+(long double)s.x1*scale),raster(cy-(long double)s.y1*scale),NULL);LineTo(canvas,raster(cx+(long double)s.x2*scale),raster(cy-(long double)s.y2*scale));
  }
  if(pen){SelectObject(canvas,oldPen);DeleteObject(pen);}painted=count;
  BitBlt(target,0,0,width,height,canvas,0,0,SRCCOPY);
  double a=angle*3.141592653589793/180.0,vx[3]={0,-6,6},vy[3]={-9,6,6};POINT pts[3];
  for(int i=0;i<3;i++){pts[i].x=raster(cx+((long double)px+vx[i]*cos(a)-vy[i]*sin(a))*scale);pts[i].y=raster(cy+(-(long double)py+vx[i]*sin(a)+vy[i]*cos(a))*scale);}
  HBRUSH brush=CreateSolidBrush(down?color(ink):RGB(140,140,140));HGDIOBJ old=SelectObject(target,brush);Polygon(target,pts,3);SelectObject(target,old);DeleteObject(brush);
  LeaveCriticalSection(&guard);EndPaint(h,&ps);return 0;
 }
 case WM_DESTROY:KillTimer(h,1);releaseCanvas();PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE prev,LPWSTR cmd,int show){
 // EOF means rendering is finished; the drawing remains until its runner exits.
 HANDLE owner=NULL;int argc=0;LPWSTR*argv=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(!argv)return 1;
 if(argc==3&&wcscmp(argv[1],L"--parent-handle")==0){
  wchar_t*end=NULL;unsigned long long value=wcstoull(argv[2],&end,10);
  owner=(HANDLE)(uintptr_t)value;
  if(!value||!end||*end||WaitForSingleObject(owner,0)==WAIT_FAILED){LocalFree(argv);return 1;}
 }else if(argc!=1){LocalFree(argv);return 1;}
 LocalFree(argv);
 InitializeCriticalSection(&guard);SetProcessDPIAware();WNDCLASSW c={0};c.lpfnWndProc=proc;c.hInstance=instance;c.lpszClassName=L"TallyDrawingWindow";c.hCursor=LoadCursor(NULL,IDC_ARROW);c.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
 if(!RegisterClassW(&c))return 1;
 window=CreateWindowExW(0,c.lpszClassName,L"Tally — Drawing",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,840,660,NULL,NULL,instance,NULL);if(!window)return 1;
 SetTimer(window,1,16,NULL);ShowWindow(window,SW_SHOW);UpdateWindow(window);HANDLE thread=CreateThread(NULL,0,reader,NULL,0,NULL);if(thread)CloseHandle(thread);
 MSG msg;bool running=true;
 while(running){
  DWORD result=MsgWaitForMultipleObjects(owner?1:0,owner?&owner:NULL,FALSE,INFINITE,QS_ALLINPUT);
  if(owner&&result==WAIT_OBJECT_0){DestroyWindow(window);break;}
  if(result==WAIT_FAILED){DestroyWindow(window);break;}
  while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
   if(msg.message==WM_QUIT){running=false;break;}
   TranslateMessage(&msg);DispatchMessageW(&msg);
  }
 }
 if(owner)CloseHandle(owner);return 0;
}
