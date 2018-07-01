#include "block.h"
#include "defines.h"
#include "conversion.h"

S16 get_object_x(Block_t* block){
     return block->pos.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
}

S16 get_object_y(Block_t* block){
     return block->pos.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
}

Pixel_t block_center_pixel(Block_t* block){
     return block->pos.pixel + HALF_TILE_SIZE_PIXEL;
}

Position_t block_get_center(Block_t* block){
     Position_t pos = block->pos;
     pos.pixel += HALF_TILE_SIZE_PIXEL;
     return pos;
}

Coord_t block_get_coord(Block_t* block){
     Pixel_t center = block->pos.pixel + HALF_TILE_SIZE_PIXEL;
     return pixel_to_coord(center);
}

bool blocks_at_collidable_height(Block_t* a, Block_t* b){
     S8 a_top = a->pos.z + HEIGHT_INTERVAL - 1;
     S8 b_top = b->pos.z + HEIGHT_INTERVAL - 1;

     if(a_top >= b->pos.z && a_top <= b_top){
          return true;
     }

     if(a->pos.z >= b->pos.z && a->pos.z <= b_top){
          return true;
     }

     return false;
}

Rect_t block_get_rect(Block_t* b){
     Rect_t block_rect = {(S16)(b->pos.pixel.x),
                          (S16)(b->pos.pixel.y),
                          (S16)(b->pos.pixel.x + TILE_SIZE_IN_PIXELS),
                          (S16)(b->pos.pixel.y + TILE_SIZE_IN_PIXELS)};
     return block_rect;
}

bool block_y_tile_aligned(Block_t* block){
     return (block->pos.decimal.y == 0.0f &&
             (block->pos.pixel.y % TILE_SIZE_IN_PIXELS) == 0);
}

bool block_x_tile_aligned(Block_t* block){
     return (block->pos.decimal.x == 0.0f &&
             (block->pos.pixel.x % TILE_SIZE_IN_PIXELS) == 0);
}

Pixel_t block_bottom_right_pixel(Pixel_t block_pixel){
     return Pixel_t{(S16)(block_pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS), block_pixel.y};
}

Pixel_t block_top_left_pixel(Pixel_t block_pixel){
     return Pixel_t{block_pixel.x, (S16)(block_pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS)};
}

Pixel_t block_top_right_pixel(Pixel_t block_pixel){
     return Pixel_t{(S16)(block_pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS),
                    (S16)(block_pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS)};
}
