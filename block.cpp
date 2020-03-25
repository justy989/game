#include "block.h"
#include "defines.h"
#include "conversion.h"

S16 get_object_x(Block_t* block){
     if(block->teleport) return block->teleport_pos.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
     return block->pos.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
}

S16 get_object_y(Block_t* block){
     if(block->teleport) return block->teleport_pos.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
     return block->pos.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
}

static Pixel_t block_center_pixel_offset(BlockCut_t cut){
    return Pixel_t{(S16)(block_get_width_in_pixels(cut) / 2), (S16)(block_get_height_in_pixels(cut) / 2)};
}

Pixel_t block_center_pixel(Block_t* block){
     return block->pos.pixel + block_center_pixel_offset(block->cut);
}

Pixel_t block_center_pixel(Position_t pos, BlockCut_t cut){
     return pos.pixel + block_center_pixel_offset(cut);
}

Pixel_t block_center_pixel(Pixel_t pixel, BlockCut_t cut){
     return pixel + block_center_pixel_offset(cut);
}

Position_t block_get_center(Block_t* block){
     Position_t pos = block->pos;
     pos.pixel += block_center_pixel_offset(block->cut);
     return pos;
}

Position_t block_get_center(Position_t pos, BlockCut_t cut){
     pos.pixel += block_center_pixel_offset(cut);
     return pos;
}

void block_set_pos_from_center(Block_t* block, Position_t pos){
    block->pos = pos - block_center_pixel_offset(block->cut);
}

Coord_t block_get_coord(Block_t* block){
     Pixel_t center = block->pos.pixel + block_center_pixel_offset(block->cut);
     return pixel_to_coord(center);
}

Coord_t block_get_coord(Position_t pos, BlockCut_t cut){
     Pixel_t center = pos.pixel + block_center_pixel_offset(cut);
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

Rect_t block_get_inclusive_rect(Block_t* b){
     Rect_t block_rect = {(S16)(b->pos.pixel.x),
                          (S16)(b->pos.pixel.y),
                          (S16)(b->pos.pixel.x + block_get_width_in_pixels(b) - 1),
                          (S16)(b->pos.pixel.y + block_get_height_in_pixels(b) - 1)};
     return block_rect;
}

Rect_t block_get_inclusive_rect(Pixel_t pixel, BlockCut_t cut){
     Rect_t block_rect = {(S16)(pixel.x),
                          (S16)(pixel.y),
                          (S16)(pixel.x + block_get_width_in_pixels(cut) - 1),
                          (S16)(pixel.y + block_get_height_in_pixels(cut) - 1)};
     return block_rect;
}

Pixel_t block_bottom_right_pixel(Pixel_t block_pixel, BlockCut_t cut){
     return Pixel_t{block_get_right_inclusive_pixel(block_pixel.x, cut), block_pixel.y};
}

Pixel_t block_top_left_pixel(Pixel_t block_pixel, BlockCut_t cut){
     return Pixel_t{block_pixel.x, block_get_right_inclusive_pixel(block_pixel.y, cut)};
}

Pixel_t block_top_right_pixel(Pixel_t block_pixel, BlockCut_t cut){
     return Pixel_t{block_get_right_inclusive_pixel(block_pixel.x, cut),
                    block_get_right_inclusive_pixel(block_pixel.y, cut)};
}

S16 block_get_width_in_pixels(BlockCut_t cut){
    switch(cut){
    default:
        return 0;
     case BLOCK_CUT_WHOLE:
     case BLOCK_CUT_TOP_HALF:
     case BLOCK_CUT_BOTTOM_HALF:
        return TILE_SIZE_IN_PIXELS;
     case BLOCK_CUT_LEFT_HALF:
     case BLOCK_CUT_RIGHT_HALF:
     case BLOCK_CUT_TOP_LEFT_QUARTER:
     case BLOCK_CUT_TOP_RIGHT_QUARTER:
     case BLOCK_CUT_BOTTOM_LEFT_QUARTER:
     case BLOCK_CUT_BOTTOM_RIGHT_QUARTER:
        return HALF_TILE_SIZE_IN_PIXELS;
    }

    return 0;
}

S16 block_get_width_in_pixels(Block_t* block){
    return block_get_width_in_pixels(block->cut);
}

S16 block_get_height_in_pixels(BlockCut_t cut){
    switch(cut){
    default:
        return 0;
     case BLOCK_CUT_WHOLE:
     case BLOCK_CUT_LEFT_HALF:
     case BLOCK_CUT_RIGHT_HALF:
        return TILE_SIZE_IN_PIXELS;
     case BLOCK_CUT_TOP_HALF:
     case BLOCK_CUT_BOTTOM_HALF:
     case BLOCK_CUT_TOP_LEFT_QUARTER:
     case BLOCK_CUT_TOP_RIGHT_QUARTER:
     case BLOCK_CUT_BOTTOM_LEFT_QUARTER:
     case BLOCK_CUT_BOTTOM_RIGHT_QUARTER:
        return HALF_TILE_SIZE_IN_PIXELS;
    }

    return 0;
}

S16 block_get_height_in_pixels(Block_t* block){
    return block_get_height_in_pixels(block->cut);
}

S16 block_get_right_inclusive_pixel(Block_t* block){
    return block_get_right_inclusive_pixel(block->pos.pixel.x, block->cut);
}

S16 block_get_right_inclusive_pixel(S16 pixel, BlockCut_t cut){
    return pixel + block_get_width_in_pixels(cut) - 1;
}

S16 block_get_top_inclusive_pixel(Block_t* block){
    return block_get_right_inclusive_pixel(block->pos.pixel.y, block->cut);
}

S16 block_get_top_inclusive_pixel(S16 pixel, BlockCut_t cut){
    return pixel + block_get_height_in_pixels(cut) - 1;
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
     case BLOCK_COAST_AIR:
          return "BLOCK_COAST_AIR";
     case BLOCK_COAST_ENTANGLED_PLAYER:
          return "BLOCK_COAST_ENTANGLED_PLAYER";
     }

     return "BLOCK_COAST_UNKNOWN";
}

S8 blocks_rotations_between(Block_t* a, Block_t* b){
     return direction_rotations_between(static_cast<Direction_t>(a->rotation), static_cast<Direction_t>(b->rotation));
}

S16 block_get_mass(Block_t* b){
     return block_get_width_in_pixels(b) * block_get_height_in_pixels(b);
}
