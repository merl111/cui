#include "cui_internal.h"
#include "cui_desktop.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define ICON_LIMIT 65536
static int icon_widget(const cui_widget *w)
{ return w && (w->kind==CUI_BUTTON || w->kind==CUI_TOGGLE || w->kind==CUI_ICON); }
cui_icon_asset *cui_icon_retain(cui_icon_asset *a) { if(a) ++a->refs; return a; }
void cui_icon_release(cui_icon_asset *a)
{ if(a && !--a->refs){ free(a->commands);free(a->pixels);free(a); } }
int cui_set_icon(cui_widget *w,cui_icon_asset *a)
{
    if(!icon_widget(w)) return 0;
    cui_icon_retain(a);cui_icon_release(w->icon);w->icon=a;
    cui__backend_icon(w);cui__backend_refresh(w->window);return 1;
}
cui_icon_asset *cui_get_icon(const cui_widget *w) { return icon_widget(w)?w->icon:NULL; }
int cui_set_icon_size(cui_widget *w,int size)
{
    if(!icon_widget(w)||size<1||size>512)return 0;
    w->icon_size=size;cui__backend_icon(w);cui__backend_refresh(w->window);return 1;
}
int cui_set_icon_only(cui_widget *w,int only)
{
    if(!w||(w->kind!=CUI_BUTTON&&w->kind!=CUI_TOGGLE))return 0;
    if(only&&!cui_get_text(w,NULL,0))return 0;
    w->icon_only=!!only;cui__backend_icon(w);cui__backend_refresh(w->window);return 1;
}
int cui_set_icon_trailing(cui_widget *w,int trailing)
{
    if(!w||(w->kind!=CUI_BUTTON&&w->kind!=CUI_TOGGLE))return 0;
    w->icon_trailing=!!trailing;cui__backend_icon(w);cui__backend_refresh(w->window);return 1;
}
cui_widget *cui_icon(cui_widget *parent,cui_icon_asset *a)
{
    cui_widget *w=cui__append(parent,CUI_ICON,"",CUI_VERTICAL,0);
    if(w)cui_set_icon(w,a);
    return w;
}
cui_widget *cui_icon_button(cui_widget *parent,cui_icon_asset *a,const char *label)
{
    if(!label||!*label)return NULL;
    cui_widget *w=cui_button(parent,label);
    if(w){w->icon_only=1;cui_set_role(w,CUI_ROLE_FLAT);cui_set_icon(w,a);cui_set_tooltip(w,label);cui_accessibility(w,label,label);}return w;
}
static int valid_command(const cui_icon_command *c)
{
    if(c->op<CUI_ICON_MOVE||c->op>CUI_ICON_STROKE)return 0;
    for(int j=0;j<6;j++)if(!isfinite(c->values[j])||fabsf(c->values[j])>1000000)return 0;
    if(c->op==CUI_ICON_FILL)return c->values[0]==0||c->values[0]==1;
    if(c->op!=CUI_ICON_STROKE)return 1;
    if(c->values[0]<=0)return 0;
    for(int j=1;j<=2;j++)if(c->values[j]!=0&&c->values[j]!=1&&c->values[j]!=2)return 0;
    return 1;
}
static int valid_commands(const cui_icon_command *v,size_t count)
{
    int open=0;
    for(size_t i=0;i<count;i++){
        if(!valid_command(v+i))return 0;
        if(v[i].op==CUI_ICON_MOVE)open=1;
        else if(!open)return 0;
        if(v[i].op==CUI_ICON_FILL||v[i].op==CUI_ICON_STROKE)open=0;
    }
    return !open;
}
cui_icon_asset *cui_icon_vector(float width,float height,const cui_icon_command *v,size_t count)
{
    if(!isfinite(width)||!isfinite(height)||width<=0||height<=0||width>1000000||height>1000000||!v||!count||count>ICON_LIMIT||!valid_commands(v,count))return NULL;
    cui_icon_asset *a=calloc(1,sizeof(*a));if(!a)return NULL;
    a->commands=malloc(count*sizeof(*v));if(!a->commands){free(a);return NULL;}
    memcpy(a->commands,v,count*sizeof(*v));a->refs=1;a->count=count;a->width=width;a->height=height;return a;
}
cui_icon_asset *cui_icon_rgba(const unsigned char *pixels,int width,int height)
{
    if(!pixels||width<1||height<1||width>4096||height>4096)return NULL;
    cui_icon_asset *a=calloc(1,sizeof(*a));if(!a)return NULL;
    size_t bytes=(size_t)width*(size_t)height*4;a->pixels=malloc(bytes);
    if(!a->pixels){free(a);return NULL;}memcpy(a->pixels,pixels,bytes);
    a->refs=1;a->width=(float)width;a->height=(float)height;return a;
}
static unsigned read_u32(const unsigned char *p)
{ return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24; }
static float read_float(const unsigned char *p)
{ unsigned v=read_u32(p);float f;memcpy(&f,&v,4);return f; }
cui_icon_asset *cui_icon_decode(const void *bytes,size_t length)
{
    const unsigned char *p=bytes;
    if(!p||length<20||memcmp(p,"CUIICON1",8))return NULL;
    unsigned n=read_u32(p+16);if(!n||n>ICON_LIMIT||length!=20+(size_t)n*36)return NULL;
    cui_icon_command *v=calloc(n,sizeof(*v));if(!v)return NULL;
    for(unsigned i=0;i<n;i++){
        const unsigned char *s=p+20+i*36;
        v[i].op=(cui_icon_op)read_u32(s);v[i].rgba=read_u32(s+4);v[i].current_color=(int)read_u32(s+8);
        for(int j=0;j<6;j++)v[i].values[j]=read_float(s+12+j*4);
    }
    cui_icon_asset *a=cui_icon_vector(read_float(p+8),read_float(p+12),v,n);free(v);return a;
}
static FILE *icon_file(const char *path)
{
#ifdef _WIN32
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);if(!n)return NULL;
    wchar_t *wide=malloc((size_t)n*sizeof(*wide));if(!wide)return NULL;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,n);
    FILE *file=_wfopen(wide,L"rb");free(wide);return file;
#else
    return fopen(path,"rb");
#endif
}
cui_icon_asset *cui_icon_load(const char *path)
{
    if(!path)return NULL;
    FILE *f=icon_file(path);if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<20||n>20+ICON_LIMIT*36||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    void *bytes=malloc((size_t)n);cui_icon_asset *a=NULL;
    if(bytes && fread(bytes,1,(size_t)n,f)==(size_t)n)a=cui_icon_decode(bytes,(size_t)n);
    free(bytes);fclose(f);return a;
}

/* Paths use a 24-unit design grid. Each native backend strokes these at its
 * current scale. Polylines are deliberately small, auditable original artwork. */
#define LINE(...) do { const float xy[]={__VA_ARGS__}; path(context,xy,sizeof(xy)/sizeof(*xy)/2,0); } while(0)
#define FILL(...) do { const float xy[]={__VA_ARGS__}; path(context,xy,sizeof(xy)/sizeof(*xy)/2,1); } while(0)
static void ring(cui_icon_path path,void *context,float x,float y,float radius)
{
    static const float unit[]={1,0,.924f,.383f,.707f,.707f,.383f,.924f,0,1,-.383f,.924f,-.707f,.707f,-.924f,.383f,-1,0,-.924f,-.383f,-.707f,-.707f,-.383f,-.924f,0,-1,.383f,-.924f,.707f,-.707f,.924f,-.383f,1,0};
    float xy[34]; for(size_t i=0;i<17;i++){xy[i*2]=x+unit[i*2]*radius;xy[i*2+1]=y+unit[i*2+1]*radius;} path(context,xy,17,0);
}
static void media_icon(cui_symbol symbol,cui_icon_path path,void *context)
{
    switch(symbol){
    case CUI_SYMBOL_PLAY: FILL(7,4,20,12,7,20); break;
    case CUI_SYMBOL_PAUSE: FILL(6,4,10,4,10,20,6,20);FILL(14,4,18,4,18,20,14,20);break;
    case CUI_SYMBOL_PREVIOUS: FILL(18,5,7,12,18,19);LINE(5,5,5,19);break;
    case CUI_SYMBOL_NEXT: FILL(6,5,17,12,6,19);LINE(19,5,19,19);break;
    case CUI_SYMBOL_VOLUME: LINE(16,8,18,10,18,14,16,16);LINE(19,5,22,9,22,15,19,19); /* fall through */
    case CUI_SYMBOL_MUTED: LINE(3,9,7,9,12,5,12,19,7,15,3,15,3,9);if(symbol==CUI_SYMBOL_MUTED){LINE(17,9,22,15);LINE(22,9,17,15);}break;
    case CUI_SYMBOL_SHUFFLE: LINE(3,6,6,6,17,18,21,18,18,15);LINE(21,18,18,21);LINE(3,18,6,18,17,6,21,6,18,3);LINE(21,6,18,9);break;
    case CUI_SYMBOL_REPEAT: LINE(3,10,3,7,6,4,20,4,17,1);LINE(20,4,17,7);LINE(21,14,21,17,18,20,4,20,7,23);LINE(4,20,7,17);break;
    default:break;
    }
}
void cui__draw_icon(cui_symbol symbol,cui_icon_path path,void *context)
{
    if(symbol>=CUI_SYMBOL_PLAY && symbol<=CUI_SYMBOL_REPEAT){media_icon(symbol,path,context);return;}
    switch(symbol){
    case CUI_SYMBOL_SEARCH:ring(path,context,10,10,6);LINE(15,15,21,21);break;
    case CUI_SYMBOL_MENU:LINE(4,6,20,6);LINE(4,12,20,12);LINE(4,18,20,18);break;
    case CUI_SYMBOL_MORE:FILL(10,4,14,4,14,7,10,7);FILL(10,10,14,10,14,13,10,13);FILL(10,16,14,16,14,19,10,19);break;
    case CUI_SYMBOL_ATTACH:LINE(8,15,16,7,18,7,19,9,18,11,9,20,6,20,3,17,3,14,14,3,18,3,21,6,21,10,11,20);break;
    case CUI_SYMBOL_SEND:LINE(3,3,22,12,3,21,7,12,3,3);LINE(7,12,22,12);break;
    case CUI_SYMBOL_HEART:LINE(12,21,3,12,2,8,4,4,8,3,12,7,16,3,20,4,22,8,21,12,12,21);break;
    case CUI_SYMBOL_HEART_FILLED:FILL(12,21,3,12,2,8,4,4,8,3,12,7,16,3,20,4,22,8,21,12);break;
    case CUI_SYMBOL_REPLY:LINE(10,5,3,12,10,19);LINE(3,12,15,12,20,15,21,20);break;
    case CUI_SYMBOL_INFO:ring(path,context,12,12,9);LINE(12,11,12,17);FILL(11,6,13,6,13,8,11,8);break;
    case CUI_SYMBOL_CLOSE:LINE(6,6,18,18);LINE(18,6,6,18);break;
    case CUI_SYMBOL_PLUS:LINE(12,4,12,20);LINE(4,12,20,12);break;
    case CUI_SYMBOL_CHECK:LINE(4,12,9,17,20,6);break;
    case CUI_SYMBOL_UP:LINE(5,15,12,8,19,15);break;
    case CUI_SYMBOL_DOWN:LINE(5,9,12,16,19,9);break;
    case CUI_SYMBOL_PIN:LINE(8,3,16,3,15,11,19,15,5,15,9,11,8,3);LINE(12,15,12,22);break;
    case CUI_SYMBOL_ARCHIVE:LINE(3,4,21,4,21,8,3,8,3,4);LINE(5,8,5,21,19,21,19,8);LINE(9,12,15,12);break;
    case CUI_SYMBOL_MAIL:LINE(3,5,21,5,21,19,3,19,3,5,12,13,21,5);break;
    case CUI_SYMBOL_EDIT:LINE(4,20,5,14,17,2,22,7,10,19,4,20);LINE(14,5,19,10);break;
    default:break;
    }
}

typedef struct symbol_builder { cui_icon_command commands[512]; size_t count; } symbol_builder;
static void symbol_path(void *context,const float *xy,size_t n,int fill)
{
    symbol_builder *b=context;if(b->count+n+2>512)return;
    for(size_t i=0;i<n;i++){
        cui_icon_command c={0};c.op=i?CUI_ICON_LINE:CUI_ICON_MOVE;c.values[0]=xy[2*i];c.values[1]=xy[2*i+1];b->commands[b->count++]=c;
    }
    cui_icon_command paint={0};paint.op=fill?CUI_ICON_FILL:CUI_ICON_STROKE;paint.current_color=1;
    if(!fill){paint.values[0]=2;paint.values[1]=1;paint.values[2]=1;}b->commands[b->count++]=paint;
}
#include "cui_chat_icons.h"
cui_icon_asset *cui_icon_symbol(cui_symbol symbol)
{
    if(symbol<=CUI_SYMBOL_NONE||symbol>=CUI_SYMBOL_COUNT)return NULL;
    if(symbol>=CUI_SYMBOL_HOME)return chat_symbol(symbol);
    symbol_builder b={0};cui__draw_icon(symbol,symbol_path,&b);
    return cui_icon_vector(24,24,b.commands,b.count);
}
