#include "app.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    SnakeApp a={0};
    assert(!snake_app_enter(&a,0,0,1));
    assert(snake_app_enter(&a,1,100,1));
    assert(a.game.state==SNAKE_READY && a.active);
    /* Start: press/release in bottom action button. */
    assert(snake_app_touch(&a,1,1,320,340,100));
    assert(snake_app_touch(&a,2,1,320,340,110));
    assert(a.game.state==SNAKE_RUNNING);
    uint16_t h=a.game.body[0]; a.game.food=0;
    snake_app_touch(&a,1,1,320,180,120);
    snake_app_touch(&a,3,1,320,140,130);
    snake_app_touch(&a,2,1,320,120,140);
    snake_app_update(&a,309,1); assert(a.game.body[0]==h);
    snake_app_update(&a,310,1); assert(a.game.body[0]==h-SNAKE_W);
    /* Frame stalls advance only once, never fast-forward through death. */
    snake_app_update(&a,10000,1); assert(a.game.body[0]==h-2*SNAKE_W);
    snake_app_set_foreground(&a,0,10001);
    snake_app_update(&a,20000,1); assert(a.game.body[0]==h-2*SNAKE_W);
    assert(!snake_app_touch(&a,1,1,320,180,20000));
    snake_app_set_foreground(&a,1,20001);
    snake_app_update(&a,20002,1); assert(a.game.body[0]==h-2*SNAKE_W);
    /* Exit press dragged onto the board must neither exit nor steer. */
    snake_app_touch(&a,1,1,600,16,10010);
    snake_app_touch(&a,3,1,320,180,10020);
    snake_app_touch(&a,2,1,320,180,10030);
    assert(a.active && !a.game.turn_pending);
    /* System state changes preempt the game. */
    snake_app_update(&a,10040,0); assert(!a.active);
    assert(!snake_app_touch(&a,1,1,320,180,10050));
    assert(snake_app_enter(&a,1,UINT32_MAX-100,1));
    snake_app_touch(&a,1,1,320,340,UINT32_MAX-100);
    snake_app_touch(&a,2,1,320,340,UINT32_MAX-100);
    h=a.game.body[0]; a.game.food=0;
    snake_app_update(&a,99,1); assert(a.game.body[0]==h+1);
    snake_app_touch(&a,1,1,600,16,100);
    snake_app_touch(&a,2,1,600,16,110); assert(!a.active);
    assert(snake_app_enter(&a,1,1000,2));
    snake_app_touch(&a,1,1,320,180,1010);
    snake_app_touch(&a,1,2,400,180,1011);
    snake_app_update(&a,1020,0);
    assert(snake_app_touch(&a,3,1,360,180,1030));
    assert(snake_app_touch(&a,2,1,360,180,1040));
    assert(snake_app_touch(&a,2,2,400,180,1050));
    assert(!snake_app_touch(&a,1,1,320,180,1060));
    assert(snake_app_enter(&a,1,2000,3));
    snake_app_touch(&a,1,1,320,180,2010);
    snake_app_set_foreground(&a,0,2020);
    assert(snake_app_touch(&a,2,1,320,180,2030));
    snake_app_set_foreground(&a,1,2040);
    snake_app_touch(&a,1,1,320,340,2050);
    snake_app_touch(&a,2,1,320,340,2060);
    h=a.game.body[0]; a.game.food=0;
    snake_app_set_foreground(&a,1,2259); /* Redundant notifications cannot freeze play. */
    snake_app_update(&a,2260,1); assert(a.game.body[0]==h+1);
    puts("PASS: app start, swipes, input isolation, preemption, exit, timer wrap");
}
