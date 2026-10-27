#include "render.h"
#include "labels.h"
enum { BG=0xff18140f, BOARD=0xff29251e, INK=0xffe9efe9,
       HEAD=0xff99ef68, BODY=0xff68aa49, FOOD=0xff559aff };
static void label(SnakeFill f,void *c,int x,int y,int w,int h,const unsigned char *bits) {
    int stride=(w+7)/8;
    for(int row=0;row<h;row++) {
        int begin=-1;
        for(int col=0;col<=w;col++) {
            int set=col<w && (bits[row*stride+col/8]&(128>>(col%8)));
            if(set && begin<0) begin=col;
            if(!set && begin>=0) { f(c,x+begin,y+row,x+col,y+row+1,INK); begin=-1; }
        }
    }
}
static void number(SnakeFill f,void *c,int x,int y,unsigned value) {
    static const unsigned char digits[10][5]={
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    unsigned hundreds=0,tens=0;
    while(value>=100) { ++hundreds; value-=100; }
    while(value>=10) { ++tens; value-=10; }
    unsigned values[3]={hundreds,tens,value};
    for(int d=0;d<3;d++) for(int r=0;r<5;r++) for(int col=0;col<3;col++)
        if(digits[values[d]%10][r]&(4>>col)) f(c,x+d*12+col*3,y+r*3,x+d*12+col*3+3,y+r*3+3,INK);
}
void snake_render_entry(SnakeFill f,void *c) {
    f(c,432,8,548,56,BOARD);
    label(f,c,462,22,LABEL_TITLE);
}
void snake_render(const SnakeApp *a,SnakeFill f,void *c) {
    if(!a->active) return;
    f(c,0,0,640,360,BG);
    f(c,80,36,560,324,BOARD);
    label(f,c,16,9,LABEL_TITLE);
    number(f,c,480,10,a->game.score);
    label(f,c,580,9,LABEL_EXIT);
    if(a->game.food!=SNAKE_NO_CELL) {
        int x=80+(a->game.food%20)*24,y=36+(a->game.food/20)*24;
        f(c,x+6,y+6,x+18,y+18,FOOD);
    }
    for(unsigned i=0;i<a->game.length;i++) {
        int x=80+(a->game.body[i]%20)*24,y=36+(a->game.body[i]/20)*24;
        f(c,x+2,y+2,x+22,y+22,i?BODY:HEAD);
    }
    if(a->game.state==SNAKE_DEAD || a->game.state==SNAKE_WON) {
        f(c,220,145,420,190,BG);
        if(a->game.state==SNAKE_WON) label(f,c,292,159,LABEL_WON);
        else label(f,c,283,159,LABEL_DEAD);
    }
    if(a->game.state!=SNAKE_RUNNING) {
        f(c,240,328,400,360,BOARD);
        if(a->game.state==SNAKE_READY) label(f,c,302,334,LABEL_START);
        else label(f,c,284,334,LABEL_RESTART);
    }
}
