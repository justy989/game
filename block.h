#pragma once

#include "position.h"
#include "motion.h"
#include "direction.h"
#include "element.h"
#include "pixel.h"
#include "coord.h"
#include "rect.h"
#include "carried_pos_delta.h"

enum BlockCoast_t{
     BLOCK_COAST_NONE,
     BLOCK_COAST_ICE,
     BLOCK_COAST_PLAYER,
     BLOCK_COAST_AIR, // lol coaster
     BLOCK_COAST_ENTANGLED_PLAYER,
};

enum BlockHeldBy_t{
     BLOCK_HELD_BY_NONE = 0,
     BLOCK_HELD_BY_SOLID = 1,
     BLOCK_HELD_BY_ENTANGLE = 2,
};

enum BlockCut_t{
     BLOCK_CUT_WHOLE,
     BLOCK_CUT_LEFT_HALF,
     BLOCK_CUT_RIGHT_HALF,
     BLOCK_CUT_TOP_HALF,
     BLOCK_CUT_BOTTOM_HALF,
     BLOCK_CUT_TOP_LEFT_QUARTER,
     BLOCK_CUT_TOP_RIGHT_QUARTER,
     BLOCK_CUT_BOTTOM_LEFT_QUARTER,
     BLOCK_CUT_BOTTOM_RIGHT_QUARTER,
};

struct TransferMomentum_t{
     S16 mass;
     F32 vel;
};

struct Block_t : public GridMotion_t{
     Position_t      pos;
     Element_t       element;
     F32             fall_time;
     S16             entangle_index; // -1 means not entangled, 0 - N = entangled with that block
     U8              rotation; // one day we will use this !

     Coord_t         clone_start; // the portal where the clone started
     S8              clone_id; // helps when we are entangled in determining which portal we come out of
     DirectionMask_t cur_push_mask;
     DirectionMask_t prev_push_mask;

     BlockCut_t cut = BLOCK_CUT_WHOLE;

     // these could all be flags one day when they grow up
     // TODO: this is set to a mix of enum values and true/false so fix it man
     S8   held_up;

     bool       teleport;
     Position_t teleport_pos;
     Vec_t      teleport_pos_delta;
     Vec_t      teleport_vel;
     Vec_t      teleport_accel;
     S16        teleport_stop_on_pixel_x;
     S16        teleport_stop_on_pixel_y;
     Move_t     teleport_horizontal_move;
     Move_t     teleport_vertical_move;
     S8         teleport_rotation;

     bool successfully_moved = false;
     BlockCoast_t coast_horizontal = BLOCK_COAST_NONE;
     BlockCoast_t coast_vertical = BLOCK_COAST_NONE;
     bool stopped_by_player_horizontal = false;
     bool stopped_by_player_vertical = false;
     bool already_received_forceback_from_chain = false;

     CarriedPosDelta_t carried_pos_delta;

     S16 previous_mass;
};

S16 get_object_x(Block_t* block);
S16 get_object_y(Block_t* block);
Pixel_t block_center_pixel(Block_t* block);
Pixel_t block_center_pixel(Position_t pos);
Pixel_t block_center_pixel(Pixel_t pos);
Position_t block_get_center(Block_t* block);
Position_t block_get_center(Position_t pos);
Coord_t block_get_coord(Block_t* block);
Coord_t block_get_coord(Position_t pos);
bool blocks_at_collidable_height(S8 a_z, S8 b_z);
Rect_t block_get_inclusive_rect(Block_t* block);
Rect_t block_get_inclusive_rect(Pixel_t pixel, BlockCut_t cut);
void block_set_pos_from_center(Block_t* block, Position_t pos);
S16 block_get_width_in_pixels(Block_t* block);
S16 block_get_height_in_pixels(Block_t* block);

S16 block_get_width_in_pixels(BlockCut_t cut);
S16 block_get_height_in_pixels(BlockCut_t cut);

S16 block_get_right_inclusive_pixel(Block_t* block);
S16 block_get_right_inclusive_pixel(S16 pixel, BlockCut_t cut);
S16 block_get_top_inclusive_pixel(Block_t* block);
S16 block_get_top_inclusive_pixel(S16 pixel, BlockCut_t cut);

Pixel_t block_bottom_right_pixel(Pixel_t block, BlockCut_t cut);
Pixel_t block_top_left_pixel(Pixel_t block, BlockCut_t cut);
Pixel_t block_top_right_pixel(Pixel_t block, BlockCut_t cut);

void block_stop_horizontally(Block_t* block);
void block_stop_vertically(Block_t* block);

const char* block_coast_to_string(BlockCoast_t coast);

S8 blocks_rotations_between(Block_t* a, Block_t* b);

S16 block_get_mass(Block_t* b);

#define MAX_BLOCKS_IN_LIST 128

struct BlockEntry_t{
     Block_t* block = nullptr;
     S8 rotations_through_portal = 0;
     bool counted = false;
};

struct BlockList_t{
     BlockEntry_t entries[MAX_BLOCKS_IN_LIST];
     S16 count = 0;

     bool add(Block_t* block, S8 rotations_through_portal){
          if(count >= MAX_BLOCKS_IN_LIST) return false;
          entries[count].block = block;
          entries[count].rotations_through_portal = rotations_through_portal;
          entries[count].counted = true;
          count++;
          return true;
     }
};
