#ifndef OA_SNAKE_APP_H
#define OA_SNAKE_APP_H
#include "snake.h"
/* Portable controller, NOT a registered native camera page yet.
 * Zero-initialize SnakeApp before first use.
 * Caller supplies normalized 640x360 coordinates and authoritative idle state.
 * Touch types: 1 down, 2 up, 3 move. Pointer IDs must be normalized to 0..15.
 * All UI callbacks run on one GUI task. Keep forwarding touch tails after
 * exit so captured fingers can be drained before returning them to native UI.
 * On active->inactive the native adapter MUST restore/invalidate camera UI.
 */
typedef struct {
    Snake game;
    Swipe swipe;
    uint32_t last_step, seed;
    uint16_t pointer, captures;
    int16_t down_x, down_y;
    uint8_t active, down, zone, dragged, paused;
} SnakeApp;
int snake_app_enter(SnakeApp *a,int allowed,uint32_t now,uint32_t seed);
int snake_app_touch(SnakeApp *a,unsigned type,uint16_t pointer,int16_t x,int16_t y,uint32_t now);
void snake_app_update(SnakeApp *a,uint32_t now,int allowed);
void snake_app_set_foreground(SnakeApp *a,int foreground,uint32_t now);
void snake_app_exit(SnakeApp *a);
#endif
