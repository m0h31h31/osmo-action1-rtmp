#include "app.h"

enum { Z_NONE, Z_BOARD, Z_ACTION, Z_EXIT };
static unsigned zone(int x,int y) {
    if(x<0 || x>=640 || y<0 || y>=360) return Z_NONE;
    if(x>=560 && y<32) return Z_EXIT;
    if(x>=240 && x<400 && y>=328) return Z_ACTION;
    if(x>=80 && x<560 && y>=36 && y<324) return Z_BOARD;
    return Z_NONE;
}

void snake_app_exit(SnakeApp *a) {
    a->active=0; a->down=0; swipe_cancel(&a->swipe);
}

int snake_app_enter(SnakeApp *a,int allowed,uint32_t now,uint32_t seed) {
    if(!allowed || a->active || a->captures) return 0;
    snake_init(&a->game,seed); swipe_cancel(&a->swipe);
    a->seed=seed; a->last_step=now; a->active=1;
    a->down=0; a->zone=Z_NONE; a->dragged=0; a->paused=0; return 1;
}

void snake_app_set_foreground(SnakeApp *a,int foreground,uint32_t now) {
    if(a->paused==(uint8_t)!foreground) return;
    a->paused=(uint8_t)!foreground; a->last_step=now;
    a->down=0; swipe_cancel(&a->swipe);
    a->game.turn_pending=0;
}

int snake_app_touch(SnakeApp *a,unsigned type,uint16_t pointer,int16_t x,int16_t y,uint32_t now) {
    if(pointer>=16) return 1; /* Reject invalid IDs rather than forwarding them. */
    uint16_t bit=(uint16_t)(1u<<pointer);
    int captured=(a->captures&bit)!=0;
    if(type==2) a->captures&=(uint16_t)~bit;
    if(!a->active || a->paused) return captured;
    if(type==1) a->captures|=bit;
    /* While active even non-board touches are consumed, never passed through. */
    if(type==1) {
        if(a->down) return 1;
        a->down=1; a->pointer=pointer; a->down_x=x; a->down_y=y;
        a->zone=(uint8_t)zone(x,y); a->dragged=0;
        if(a->zone==Z_BOARD) swipe_begin(&a->swipe,pointer,x,y);
        return 1;
    }
    if(!a->down || a->pointer!=pointer) return 1;
    if(type!=2 && type!=3) return 1;
    int32_t dx=(int32_t)x-a->down_x,dy=(int32_t)y-a->down_y;
    if(dx>=SWIPE_MIN_PX || dx<=-SWIPE_MIN_PX || dy>=SWIPE_MIN_PX || dy<=-SWIPE_MIN_PX) a->dragged=1;
    if(a->zone==Z_BOARD) {
        SnakeDirection d=type==2?swipe_end(&a->swipe,pointer,x,y):swipe_move(&a->swipe,pointer,x,y);
        if(d!=SNAKE_NO_DIRECTION) snake_turn(&a->game,d);
    }
    if(type==2) {
        a->down=0;
        if(!a->dragged && zone(x,y)==a->zone) {
            if(a->zone==Z_EXIT) snake_app_exit(a);
            else if(a->zone==Z_ACTION && a->game.state!=SNAKE_RUNNING) {
                if(a->game.state!=SNAKE_READY) snake_init(&a->game,++a->seed);
                snake_start(&a->game); a->last_step=now;
            }
        }
    }
    return 1;
}

void snake_app_update(SnakeApp *a,uint32_t now,int allowed) {
    if(!a->active) return;
    if(!allowed) { snake_app_exit(a); return; }
    if(a->paused) return;
    if(a->game.state==SNAKE_RUNNING && (uint32_t)(now-a->last_step)>=SNAKE_STEP_MS) {
        snake_tick(&a->game); a->last_step=now;
    }
}
