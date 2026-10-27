#include "snake.h"

static int occupied(const Snake *s, uint16_t cell, unsigned length) {
    for (unsigned i=0; i<length; ++i) if(s->body[i]==cell) return 1;
    return 0;
}

static void food(Snake *s) {
    /* Bounded selection: cannot spin forever on an almost full board. */
    unsigned free_cells=SNAKE_CELLS-s->length;
    if(!free_cells) { s->food=SNAKE_NO_CELL; s->state=SNAKE_WON; return; }
    s->random=s->random*1664525u+1013904223u;
    unsigned pick=(unsigned)(((uint64_t)s->random*free_cells)>>32);
    for(uint16_t cell=0; cell<SNAKE_CELLS; ++cell) {
        if(!occupied(s,cell,s->length) && pick--==0) { s->food=cell; return; }
    }
}

void snake_init(Snake *s, uint32_t seed) {
    for(unsigned i=0; i<SNAKE_CELLS; ++i) s->body[i]=SNAKE_NO_CELL;
    s->length=3; s->score=0; s->random=seed;
    s->direction=s->pending=SNAKE_RIGHT; s->turn_pending=0;
    s->state=SNAKE_READY;
    for(unsigned i=0;i<3;++i) s->body[i]=(SNAKE_H/2)*SNAKE_W+SNAKE_W/2-i;
    food(s);
}

void snake_start(Snake *s) { if(s->state==SNAKE_READY) s->state=SNAKE_RUNNING; }

int snake_turn(Snake *s, SnakeDirection d) {
    if(s->state!=SNAKE_RUNNING || s->turn_pending || (unsigned)d>=4 ||
       d==s->direction || (unsigned)d==((s->direction+2u)%4u)) return 0;
    s->pending=d; s->turn_pending=1; return 1;
}

void snake_tick(Snake *s) {
    if(s->state!=SNAKE_RUNNING) return;
    if(s->turn_pending) s->direction=s->pending;
    s->turn_pending=0;
    int x=s->body[0]%SNAKE_W, y=s->body[0]/SNAKE_W;
    switch(s->direction) {
      case SNAKE_UP: --y; break; case SNAKE_RIGHT: ++x; break;
      case SNAKE_DOWN: ++y; break; case SNAKE_LEFT: --x; break;
      default: return;
    }
    if(x<0 || x>=SNAKE_W || y<0 || y>=SNAKE_H) { s->state=SNAKE_DEAD; return; }
    uint16_t next=(uint16_t)(y*SNAKE_W+x);
    int grows=next==s->food;
    if(occupied(s,next,s->length-(grows?0:1))) { s->state=SNAKE_DEAD; return; }
    if(grows) { ++s->length; ++s->score; }
    for(unsigned i=s->length-1; i>0; --i) s->body[i]=s->body[i-1];
    s->body[0]=next;
    if(grows) food(s);
}

void swipe_begin(Swipe *s, uint16_t pointer, int16_t x, int16_t y) {
    if(s->active) return; /* A second finger cannot steal the first gesture. */
    s->x=x; s->y=y; s->pointer=pointer; s->active=1; s->fired=0;
}

SnakeDirection swipe_move(Swipe *s, uint16_t pointer, int16_t x, int16_t y) {
    if(!s->active || s->fired || s->pointer!=pointer) return SNAKE_NO_DIRECTION;
    int32_t dx=(int32_t)x-s->x,dy=(int32_t)y-s->y;
    int32_t ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    SnakeDirection d=SNAKE_NO_DIRECTION;
    if(ax>=SWIPE_MIN_PX && ax>=2*ay) d=dx>0?SNAKE_RIGHT:SNAKE_LEFT;
    else if(ay>=SWIPE_MIN_PX && ay>=2*ax) d=dy>0?SNAKE_DOWN:SNAKE_UP;
    if(d!=SNAKE_NO_DIRECTION) s->fired=1;
    return d;
}

SnakeDirection swipe_end(Swipe *s, uint16_t pointer, int16_t x, int16_t y) {
    SnakeDirection d=swipe_move(s,pointer,x,y);
    if(s->active && s->pointer==pointer) s->active=0;
    return d;
}

void swipe_cancel(Swipe *s) { s->active=0; s->fired=0; }
