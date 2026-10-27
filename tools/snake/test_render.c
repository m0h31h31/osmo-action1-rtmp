#include "render.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls,head,food,button;
static void fill(void *ctx,int x,int y,int right,int bottom,uint32_t color) {
    (void)ctx;
    assert(x>=0 && y>=0 && right<=640 && bottom<=360);
    assert(x<right && y<bottom && (color>>24)==255);
    ++calls;
    if(color==0xff99ef68) ++head;
    if(color==0xff559aff) ++food;
    if(x==240 && y==328 && right==400 && bottom==360) ++button;
}
int main(void) {
    SnakeApp a={0};
    snake_render(&a,fill,0); assert(calls==0);
    assert(snake_app_enter(&a,1,0,42));
    snake_render(&a,fill,0);
    assert(head==1 && food==1 && button==1);
    a.game.state=SNAKE_WON; a.game.food=SNAKE_NO_CELL;
    head=food=0; snake_render(&a,fill,0); assert(head==1 && food==0);
    a.game.state=SNAKE_DEAD; snake_render(&a,fill,0);
    a.game.state=SNAKE_RUNNING; snake_render(&a,fill,0);
    snake_render_entry(fill,0);
    puts("PASS: renderer bounds, alpha, head/food, start/restart states, menu entry");
}
