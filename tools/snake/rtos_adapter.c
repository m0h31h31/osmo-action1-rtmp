/* Exact-image Action 1 native UI adapter, integrated in v21.
 * Runtime loader verifies image signatures and reserves the idle UVC sample.
 * See rtos_contract.json for validation scope and remaining regression work.
 */
#include "render.h"
#include <stddef.h>
typedef struct { int x1,y1,x2,y2; } Rect;
typedef struct { int x,y; } Point;
typedef struct {
    uint32_t magic,enabled;
    volatile uint32_t allow,deadline;
    uint32_t draws,native_contacts;
    uintptr_t root;
    SnakeApp app;
    uint16_t entry;
    int16_t down_x,down_y;
    uint8_t dragged;
} Context;
const unsigned oa_layout[] __attribute__((used))={sizeof(Context),
    offsetof(Context,app)+offsetof(SnakeApp,captures),offsetof(Context,entry)};
#define C ((Context *)(uintptr_t)0x25ca890)
#define MAGIC 0x534e4b45u
#define FN(address,type) ((type)(uintptr_t)(address))
typedef struct { uint32_t reason,tick,top_class,detail; } ExitEvent;
typedef struct { uint32_t count; ExitEvent events[4]; } Debug;
#define DEBUG ((volatile Debug *)(uintptr_t)0x25cad50)
static const Rect screen={0,0,640,360};
static uint32_t word(uintptr_t p,unsigned off) { return *(uint32_t *)(p+off); }
static int valid(uintptr_t p) { return p>=0x046a0000 && p<0x10fa0000 && !(p&3); }
static uint32_t now(void) { return FN(0x25fe2c,uint32_t (*)(void))(); }
static int expired(uint32_t tick) {
    return C->deadline!=UINT32_MAX && (int32_t)(tick-C->deadline)>=0;
}
static int allowed(void) {
    if(!C->allow) return 0;
    uintptr_t status=FN(0x2601b8,uintptr_t (*)(uintptr_t,uintptr_t))(0x9824ac,0x984a78);
    return !FN(0x2134b0,int (*)(uintptr_t))(status);
}
static uintptr_t top_page(void) {
    if(!valid(C->root)) return 0;
    uintptr_t p=word(C->root,0x3c);
    for(unsigned i=0;valid(p) && i<64;i++,p=word(p,0x14)) {
        if(!(word(p,0x1c)&1)) continue;
        /* Stock dialog wrapper remains attached when its warp child is removed
         * by OnMsgDialogEmpty. Only that exact, childless class is transparent. */
        if(word(p,0)==0x976bb8 && !word(p,0x38) && !word(p,0x3c)) continue;
        return p;
    }
    return 0;
}
static uintptr_t page(void) { uintptr_t p=top_page();return p&&word(p,0)==0x8ec63c?p:0; }
static void event(unsigned reason,uint32_t tick,unsigned detail) {
    uintptr_t top=top_page();uint32_t count=DEBUG->count;
    DEBUG->events[count&3]=(ExitEvent){reason,tick,top?word(top,0):0,detail};
    DEBUG->count=count+1;
}
static void invalidate(uintptr_t p) {
    if(p) FN(0x112fd4,void (*)(uintptr_t,Rect))(p,screen);
}
typedef struct { void *canvas; Rect clip; Point offset; } Paint;
static void fill(void *opaque,int x,int y,int r,int b,uint32_t color) {
    Paint *p=opaque;
    Rect area={x+p->offset.x,y+p->offset.y,r+p->offset.x,b+p->offset.y};
    FN(0x12302c,void (*)(void *,Rect,Rect,uint32_t,uint32_t,uint32_t,uint32_t,int))
        (p->canvas,p->clip,area,color,color,color,color,1);
}
void oa_draw(uintptr_t self,void *canvas,Rect clip,Point offset,int opacity,int blend) {
    if(C->magic!=MAGIC || !C->enabled) {
        FN(0x116a44,void (*)(uintptr_t,void *,Rect,Point,int,int))(self,canvas,clip,offset,opacity,blend);
        return;
    }
    uintptr_t root=self;
    for(unsigned i=0;i<16 && valid(root) && word(root,0x18);i++) root=word(root,0x18);
    if(valid(root) && word(root,0)==0x8ec988) C->root=root;
    Paint paint={canvas,clip,offset};
    if(C->app.active && page()==self) snake_render(&C->app,fill,&paint);
    else {
        FN(0x116a44,void (*)(uintptr_t,void *,Rect,Point,int,int))(self,canvas,clip,offset,opacity,blend);
        if(allowed()) snake_render_entry(fill,&paint);
    }
}
static int inside(int x,int y) { return x>=432 && x<548 && y>=8 && y<56; }
static int touch(uintptr_t root,unsigned type,unsigned id,int x,int y) {
    if(C->magic!=MAGIC || !C->enabled) return 0;
    C->root=root;
    uintptr_t p=page(); uint32_t tick=now();
    int idle=allowed();
    if(C->app.active && (!p || !idle)) {
        event(!p?2:(!C->allow?5:3),tick,type);
        snake_app_exit(&C->app); invalidate(p);
    }
    if(!C->entry && (C->app.active || C->app.captures)) {
        unsigned was_active=C->app.active;
        int consumed=snake_app_touch(&C->app,type,(uint16_t)id,(int16_t)x,(int16_t)y,tick);
        if(was_active&&!C->app.active)event(1,tick,((unsigned)x<<16)|(unsigned)(uint16_t)y);
        if(consumed) invalidate(p);
        return consumed;
    }
    if(id>=16) return 0;
    uint16_t bit=(uint16_t)(1u<<id);
    if(C->entry) {
        if(type==1) { C->app.captures|=bit; C->dragged=1; }
        if(type==2) C->app.captures&=(uint16_t)~bit;
        if(x-C->down_x>=12 || C->down_x-x>=12 || y-C->down_y>=12 || C->down_y-y>=12) C->dragged=1;
        if(type==2 && (C->entry&bit)) {
            C->entry=0;
            if(!C->dragged && p && inside(x,y) && snake_app_enter(&C->app,idle,tick,tick))event(10,tick,0);
            invalidate(p);
        }
        return 1;
    }
    if(type==1 && p && idle && !C->native_contacts && inside(x,y)) {
        C->entry=bit; C->app.captures|=bit;
        C->down_x=(int16_t)x; C->down_y=(int16_t)y; C->dragged=0; return 1;
    }
    return 0;
}
int oa_hit(uintptr_t root,int down,unsigned id,int x,int y) {
    if(touch(root,down?1:2,id,x,y)) return 1;
    if(C->magic==MAGIC && C->enabled && id<16) {
        if(down) C->native_contacts|=1u<<id;
        else C->native_contacts&=~(1u<<id);
    }
    return FN(0x161b00,int (*)(uintptr_t,int,unsigned,int,int))(root,down,id,x,y);
}
int oa_move(uintptr_t root,unsigned id,int x,int y) {
    if(touch(root,3,id,x,y)) return 1;
    return FN(0x15e8b8,int (*)(uintptr_t,unsigned,int,int))(root,id,x,y);
}
int oa_timers(void) {
    int result=FN(0x264848,int (*)(void))();
    if(C->magic!=MAGIC || !C->enabled) return result;
    uint32_t tick=now(); uintptr_t p=page();
    C->draws=tick; /* Heartbeat for the Linux loader; fixed offset 16. */
    if(!C->deadline) C->deadline=tick+120000;
    if(expired(tick)) C->allow=0;
    uint32_t last=C->app.last_step; unsigned active=C->app.active;
    int can_play=p && allowed();
    if(active&&!can_play)event(!p?2:(!C->allow?(expired(tick)?4:5):3),tick,0);
    snake_app_update(&C->app,tick,can_play);
    if(active!=C->app.active || last!=C->app.last_step) { invalidate(p); return 1; }
    return result;
}
unsigned oa_context_size(void) { return sizeof(Context); }
