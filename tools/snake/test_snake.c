#include "snake.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void game_tests(void) {
    Snake s;
    snake_init(&s, 123);
    assert(s.state == SNAKE_READY && s.length == 3 && s.score == 0);
    uint16_t head = s.body[0];
    snake_tick(&s); assert(s.body[0] == head);
    assert(!snake_turn(&s, SNAKE_UP));
    snake_start(&s);
    assert(!snake_turn(&s, SNAKE_LEFT));
    assert(snake_turn(&s, SNAKE_UP));
    assert(!snake_turn(&s, SNAKE_LEFT)); /* Two gestures cannot reverse in one tick. */
    s.food = 0;
    snake_tick(&s); assert(s.body[0] == head-SNAKE_W);
    assert(snake_turn(&s, SNAKE_LEFT));
    snake_tick(&s); assert(s.body[0] == head-SNAKE_W-1);

    snake_init(&s, 123); snake_start(&s);
    s.food = s.body[0]+1;
    snake_tick(&s);
    assert(s.length == 4 && s.score == 1);
    for (unsigned i=0;i<s.length;i++) assert(s.food != s.body[i]);
    s.body[0]=SNAKE_W-1; s.food=0;
    snake_tick(&s); assert(s.state == SNAKE_DEAD);
    head=s.body[0]; snake_tick(&s); assert(s.body[0] == head);

    /* Following the old tail is legal when it moves out of this cell. */
    snake_init(&s, 1); snake_start(&s);
    s.length=4; s.body[0]=21; s.body[1]=22; s.body[2]=42; s.body[3]=41;
    s.direction=SNAKE_LEFT; s.food=0;
    assert(snake_turn(&s,SNAKE_DOWN)); snake_tick(&s);
    assert(s.state==SNAKE_RUNNING && s.body[0]==41);
    /* Same movement into an interior body cell is a collision. */
    s.body[0]=21; s.body[1]=22; s.body[2]=41; s.body[3]=42;
    s.direction=SNAKE_LEFT;
    assert(snake_turn(&s,SNAKE_DOWN)); snake_tick(&s); assert(s.state==SNAKE_DEAD);

    /* All but one cell occupied: eating the final free cell wins. */
    snake_init(&s, 1); snake_start(&s);
    s.length=SNAKE_CELLS-1; s.body[0]=0;
    for(unsigned i=1;i<s.length;i++) s.body[i]=(uint16_t)(i+1);
    s.direction=SNAKE_RIGHT; s.food=1;
    snake_tick(&s); assert(s.state==SNAKE_WON && s.length==SNAKE_CELLS);
    assert(s.food==SNAKE_NO_CELL);
    snake_init(&s, 0); assert(s.state==SNAKE_READY && s.score==0);
}

static void swipe_tests(void) {
    Swipe g={0};
    assert(swipe_move(&g,1,50,50)==SNAKE_NO_DIRECTION);
    const int dx[]={0,40,0,-40},dy[]={-40,0,40,0};
    for(int i=0;i<4;i++) {
        swipe_begin(&g,7,100,100);
        assert(swipe_move(&g,8,100+dx[i],100+dy[i])==SNAKE_NO_DIRECTION);
        assert(swipe_move(&g,7,100+dx[i],100+dy[i])==(SnakeDirection)i);
        assert(swipe_move(&g,7,100-dx[i],100-dy[i])==SNAKE_NO_DIRECTION);
        assert(swipe_end(&g,7,100+dx[i],100+dy[i])==SNAKE_NO_DIRECTION);
    }
    swipe_begin(&g,1,0,0);
    assert(swipe_move(&g,1,8,4)==SNAKE_NO_DIRECTION);
    assert(swipe_move(&g,1,30,29)==SNAKE_NO_DIRECTION);
    assert(swipe_end(&g,1,40,10)==SNAKE_RIGHT);
    assert(swipe_move(&g,1,100,0)==SNAKE_NO_DIRECTION);
    swipe_begin(&g,1,0,0); swipe_cancel(&g);
    assert(swipe_end(&g,1,100,0)==SNAKE_NO_DIRECTION);
    swipe_begin(&g,1,0,0); swipe_begin(&g,2,100,100);
    assert(swipe_end(&g,2,200,100)==SNAKE_NO_DIRECTION);
    assert(swipe_end(&g,1,-40,0)==SNAKE_LEFT);
}

int main(void) { game_tests(); swipe_tests(); puts("PASS: game rules and swipe controls"); }
