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

Pixel_t block_center_pixel(Position_t pos){
     return pos.pixel + HALF_TILE_SIZE_PIXEL;
}

Position_t block_get_center(Block_t* block){
     Position_t pos = block->pos;
     pos.pixel += HALF_TILE_SIZE_PIXEL;
     return pos;
}

Position_t block_get_center(Position_t pos){
     pos.pixel += HALF_TILE_SIZE_PIXEL;
     return pos;
}

Coord_t block_get_coord(Block_t* block){
     Pixel_t center = block->pos.pixel + HALF_TILE_SIZE_PIXEL;
     return pixel_to_coord(center);
}

Coord_t block_get_coord(Position_t pos){
     Pixel_t center = pos.pixel + HALF_TILE_SIZE_PIXEL;
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

void block_stop_horizontally(Block_t* block){
     block->horizontal_move.state = MOVE_STATE_IDLING;
     block->horizontal_move.sign = MOVE_SIGN_ZERO;
     block->horizontal_move.distance = 0;
     block->pos_delta.x = 0;
     block->vel.x = 0;
     block->prev_vel.x = 0;
     block->accel.x = 0;
}

void block_stop_vertically(Block_t* block){
     block->vertical_move.state = MOVE_STATE_IDLING;
     block->vertical_move.sign = MOVE_SIGN_ZERO;
     block->vertical_move.distance = 0;
     block->pos_delta.y = 0;
     block->vel.y = 0;
     block->prev_vel.y = 0;
     block->accel.y = 0;
}

const char* block_coast_to_string(BlockCoast_t coast){
     switch(coast){
     case BLOCK_COAST_NONE:
          return "BLOCK_COAST_NONE";
     case BLOCK_COAST_ICE:
          return "BLOCK_COAST_ICE";
     case BLOCK_COAST_PLAYER:
          return "BLOCK_COAST_PLAYER";
     }

     return "BLOCK_COAST_UNKNOWN";
}

S8 blocks_rotations_between(Block_t* a, Block_t* b){
     return direction_rotations_between(static_cast<Direction_t>(a->rotation), static_cast<Direction_t>(b->rotation));
}
