#ifndef OA_SNAKE_H
#define OA_SNAKE_H
#include <stdint.h>
#define SNAKE_W 20
#define SNAKE_H 12
#define SNAKE_CELLS (SNAKE_W*SNAKE_H)
#define SNAKE_NO_CELL UINT16_MAX
#define SNAKE_STEP_MS 200
#define SWIPE_MIN_PX 12
typedef enum { SNAKE_UP, SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT,
               SNAKE_NO_DIRECTION } SnakeDirection;
typedef enum { SNAKE_READY, SNAKE_RUNNING, SNAKE_DEAD, SNAKE_WON } SnakeState;
typedef struct {
    uint16_t body[SNAKE_CELLS], length, food, score;
    uint32_t random;
    SnakeDirection direction, pending;
    SnakeState state;
    uint8_t turn_pending;
} Snake;
typedef struct {
    int16_t x, y;
    uint16_t pointer;
    uint8_t active, fired;
} Swipe;
void snake_init(Snake *s, uint32_t seed);
void snake_start(Snake *s);
int snake_turn(Snake *s, SnakeDirection direction);
void snake_tick(Snake *s);
void swipe_begin(Swipe *s, uint16_t pointer, int16_t x, int16_t y);
SnakeDirection swipe_move(Swipe *s, uint16_t pointer, int16_t x, int16_t y);
SnakeDirection swipe_end(Swipe *s, uint16_t pointer, int16_t x, int16_t y);
void swipe_cancel(Swipe *s);
#endif
