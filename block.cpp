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
     return block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
}

Coord_t block_get_coord(Block_t* block){
     Pixel_t center = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
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
