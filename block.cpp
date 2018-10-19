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

bool blocks_at_collidable_height(S8 a_z, S8 b_z){
     S8 a_top = a_z + HEIGHT_INTERVAL - (S8)(1);
     S8 b_top = b_z + HEIGHT_INTERVAL - (S8)(1);

     if(a_top >= b_z && a_top <= b_top){
          return true;
     }

     if(a_z >= b_z && a_z <= b_top){
          return true;
     }

     return false;
}

Rect_t block_get_rect(Block_t* b){
     Rect_t block_rect = {(S16)(b->pos.pixel.x),
                          (S16)(b->pos.pixel.y),
                          (S16)(b->pos.pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS),
                          (S16)(b->pos.pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS)};
     return block_rect;
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
