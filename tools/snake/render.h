#ifndef OA_SNAKE_RENDER_H
#define OA_SNAKE_RENDER_H
#include "app.h"
/* Half-open rectangles in logical 640x360 coordinates; color is 0xAABBGGRR.
 * Adapter MUST intersect with the native update clip and respect orientation.
 * Rendering has no state changes or native API dependencies. */
typedef void (*SnakeFill)(void *,int,int,int,int,uint32_t);
void snake_render(const SnakeApp *,SnakeFill,void *);
void snake_render_entry(SnakeFill,void *);
#endif
