/* GDI+ is supplied by Windows. Its stable flat ABI keeps the library C99. */
#include "cui_draw_internal.h"
#include <windows.h>
#include <stdlib.h>

typedef struct { UINT32 version; void *callback; BOOL suppress_thread, suppress_codecs; } icon_startup;
#define GP(name,args) __declspec(dllimport) int WINAPI name args
GP(GdiplusStartup,(ULONG_PTR *,const icon_startup *,void *));
__declspec(dllimport) void WINAPI GdiplusShutdown(ULONG_PTR);
GP(GdipCreateFromHDC,(HDC,void **)); GP(GdipDeleteGraphics,(void *));
GP(GdipSetSmoothingMode,(void *,int)); GP(GdipSetPixelOffsetMode,(void *,int));
GP(GdipCreatePath,(int,void **)); GP(GdipDeletePath,(void *));
GP(GdipStartPathFigure,(void *)); GP(GdipClosePathFigure,(void *));
GP(GdipAddPathLine,(void *,float,float,float,float));
GP(GdipAddPathBezier,(void *,float,float,float,float,float,float,float,float));
GP(GdipSetPathFillMode,(void *,int)); GP(GdipFillPath,(void *,void *,void *));
GP(GdipCreateSolidFill,(UINT32,void **)); GP(GdipDeleteBrush,(void *));
GP(GdipCreatePen1,(UINT32,float,int,void **)); GP(GdipDeletePen,(void *));
GP(GdipSetPenLineCap197819,(void *,int,int,int)); GP(GdipSetPenLineJoin,(void *,int));
GP(GdipDrawPath,(void *,void *,void *));GP(GdipSetPenMiterLimit,(void *,float));
GP(GdipCreateBitmapFromScan0,(int,int,int,int,unsigned char *,void **));
GP(GdipDrawImageRect,(void *,void *,float,float,float,float)); GP(GdipDisposeImage,(void *));
static ULONG_PTR icon_token;
void cui__win_icons_shutdown(void) { if(icon_token){GdiplusShutdown(icon_token);icon_token=0;} }
static UINT32 icon_color(const cui_icon_command *c,COLORREF color,int enabled)
{
    unsigned a=c->current_color?255:c->rgba&255;
    if(!enabled)a=a*45/100;
    return (a<<24)|(c->current_color?(GetRValue(color)<<16)|(GetGValue(color)<<8)|GetBValue(color):c->rgba>>8);
}
static void icon_paint(void *graphics,void *path,const cui_icon_command *c,COLORREF color,float scale,int enabled)
{
    UINT32 argb=icon_color(c,color,enabled);
    if(c->op==CUI_ICON_FILL){void *brush=NULL;GdipSetPathFillMode(path,c->values[0]?0:1);GdipCreateSolidFill(argb,&brush);if(brush){GdipFillPath(graphics,brush,path);GdipDeleteBrush(brush);}}
    else {void *pen=NULL;GdipCreatePen1(argb,c->values[0]*scale,2,&pen);if(pen){int cap=c->values[1]==1?2:c->values[1]==2?1:0;int join=c->values[2]==1?2:c->values[2]==2?1:0;GdipSetPenLineCap197819(pen,cap,cap,0);GdipSetPenLineJoin(pen,join);GdipSetPenMiterLimit(pen,4);GdipDrawPath(graphics,pen,path);GdipDeletePen(pen);}}
}
static void icon_bitmap(void *graphics,cui_icon_asset *a,float x,float y,float scale,int enabled)
{
    int width=(int)a->width,height=(int)a->height;size_t n=(size_t)width*height;
    unsigned char *bgra=malloc(n*4);if(!bgra)return;
    for(size_t i=0;i<n;i++){bgra[i*4]=a->pixels[i*4+2];bgra[i*4+1]=a->pixels[i*4+1];bgra[i*4+2]=a->pixels[i*4];bgra[i*4+3]=(unsigned char)(a->pixels[i*4+3]*(enabled?100:45)/100);}
    void *bitmap=NULL;GdipCreateBitmapFromScan0(width,height,width*4,0x26200a,bgra,&bitmap);
    if(bitmap){GdipDrawImageRect(graphics,bitmap,x,y,width*scale,height*scale);GdipDisposeImage(bitmap);}free(bgra);
}
static void render_icon(cui_widget *w,void *graphics,RECT r,COLORREF color)
{
    cui_icon_asset *a=w->icon;
    GdipSetSmoothingMode(graphics,4);GdipSetPixelOffsetMode(graphics,4);
    float scale=min((float)(r.right-r.left)/a->width,(float)(r.bottom-r.top)/a->height);
    float x=r.left+((r.right-r.left)-a->width*scale)/2,y=r.top+((r.bottom-r.top)-a->height*scale)/2;
    if(a->pixels)icon_bitmap(graphics,a,x,y,scale,w->enabled);
    else{
        void *path=NULL;float px=0,py=0,sx=0,sy=0;GdipCreatePath(1,&path);
        for(size_t i=0;path&&i<a->count;i++){
            const cui_icon_command *c=a->commands+i;const float *v=c->values;
            switch(c->op){
            case CUI_ICON_MOVE:GdipStartPathFigure(path);px=sx=x+v[0]*scale;py=sy=y+v[1]*scale;break;
            case CUI_ICON_LINE:GdipAddPathLine(path,px,py,x+v[0]*scale,y+v[1]*scale);px=x+v[0]*scale;py=y+v[1]*scale;break;
            case CUI_ICON_CUBIC:GdipAddPathBezier(path,px,py,x+v[0]*scale,y+v[1]*scale,x+v[2]*scale,y+v[3]*scale,x+v[4]*scale,y+v[5]*scale);px=x+v[4]*scale;py=y+v[5]*scale;break;
            case CUI_ICON_CLOSE:GdipClosePathFigure(path);px=sx;py=sy;break;
            case CUI_ICON_FILL:case CUI_ICON_STROKE:icon_paint(graphics,path,c,color,scale,w->enabled);GdipDeletePath(path);path=NULL;GdipCreatePath(1,&path);break;
            }
        }
        if(path)GdipDeletePath(path);
    }
 }
void cui__win_icon(cui_widget *w,HDC dc,RECT r,COLORREF color)
{
    cui_icon_asset *a=w->icon;if(!a)return;
    if(!icon_token){icon_startup startup={1,NULL,FALSE,FALSE};if(GdiplusStartup(&icon_token,&startup,NULL))return;}
    void *graphics=NULL;if(GdipCreateFromHDC(dc,&graphics))return;
    render_icon(w,graphics,r,color);
    GdipDeleteGraphics(graphics);
}


GP(GdipCreateBitmapFromFile,(const WCHAR *,void **));
GP(GdipGetImageWidth,(void *,UINT *));GP(GdipGetImageHeight,(void *,UINT *));
typedef struct { UINT width,height;INT stride;INT format;void *scan;UINT_PTR reserved; } icon_bitmap_data;
GP(GdipBitmapLockBits,(void *,const void *,UINT,INT,icon_bitmap_data *));
GP(GdipBitmapUnlockBits,(void *,icon_bitmap_data *));
cui_icon_asset *cui_icon_load_image(const char *path)
{
    if(!path)return NULL;
    if(!icon_token){icon_startup startup={1,NULL,FALSE,FALSE};if(GdiplusStartup(&icon_token,&startup,NULL))return NULL;}
    int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);if(!length)return NULL;
    WCHAR *wide=malloc((size_t)length*sizeof(WCHAR));if(!wide)return NULL;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,length);
    void *image=NULL;GdipCreateBitmapFromFile(wide,&image);free(wide);if(!image)return NULL;
    UINT width=0,height=0;GdipGetImageWidth(image,&width);GdipGetImageHeight(image,&height);cui_icon_asset *asset=NULL;
    if(width&&height&&width<=4096&&height<=4096){
        icon_bitmap_data data={0};
        if(!GdipBitmapLockBits(image,NULL,1,0x26200a,&data)){
            unsigned char *rgba=malloc((size_t)width*height*4);
            if(rgba){for(UINT y=0;y<height;y++)for(UINT x=0;x<width;x++){
                const unsigned char *p=(const unsigned char *)data.scan+(ptrdiff_t)y*data.stride+x*4;
                unsigned char *q=rgba+((size_t)y*width+x)*4;q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=p[3];
            }asset=cui_icon_rgba(rgba,(int)width,(int)height);free(rgba);}
            GdipBitmapUnlockBits(image,&data);
        }
    }
    GdipDisposeImage(image);return asset;
}

GP(GdipGetImageGraphicsContext,(void *,void **));
uint32_t *cui__draw_asset(const cui_icon_asset *source,int width,int height,unsigned color)
{
    if(!icon_token){icon_startup startup={1,NULL,FALSE,FALSE};if(GdiplusStartup(&icon_token,&startup,NULL))return NULL;}
    uint32_t *out=calloc((size_t)width*height,4);if(!out)return NULL;
    cui_icon_asset asset=*source;cui_icon_command *commands=NULL;
    if(asset.count){commands=malloc(asset.count*sizeof(*commands));if(!commands){free(out);return NULL;}memcpy(commands,asset.commands,asset.count*sizeof(*commands));for(size_t i=0;i<asset.count;++i)if(commands[i].current_color){commands[i].current_color=0;commands[i].rgba=color;}asset.commands=commands;}
    void *bitmap=NULL,*graphics=NULL;
    if(GdipCreateBitmapFromScan0(width,height,width*4,0xE200B,(unsigned char *)out,&bitmap)||GdipGetImageGraphicsContext(bitmap,&graphics)){if(bitmap)GdipDisposeImage(bitmap);free(commands);free(out);return NULL;}
    cui_widget w={0};w.icon=&asset;w.enabled=1;RECT r={0,0,width,height};render_icon(&w,graphics,r,RGB(color>>24,(color>>16)&255,(color>>8)&255));
    GdipDeleteGraphics(graphics);GdipDisposeImage(bitmap);free(commands);return out;
}
