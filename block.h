#pragma once

#include "position.h"
#include "vec.h"
#include "direction.h"
#include "element.h"
#include "pixel.h"
#include "coord.h"
#include "rect.h"

struct Block_t{
     Position_t pos;
     Vec_t accel;
     Vec_t vel;
     Direction_t face;
     Element_t element;
     Pixel_t push_start;
     F32 fall_time;
};

S16 get_object_x(Block_t* block);
S16 get_object_y(Block_t* block);
Pixel_t block_center_pixel(Block_t* block);
Position_t block_get_center(Block_t* block);
Coord_t block_get_coord(Block_t* block);
bool blocks_at_collidable_height(Block_t* a, Block_t* b);
Rect_t block_get_rect(Block_t* block);
bool block_x_tile_aligned(Block_t* block);
bool block_y_tile_aligned(Block_t* block);

Pixel_t block_bottom_left_pixel(Pixel_t block);
Pixel_t block_bottom_right_pixel(Pixel_t block);
Pixel_t block_top_left_pixel(Pixel_t block);
Pixel_t block_top_right_pixel(Pixel_t block);
