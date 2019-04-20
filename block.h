#pragma once

#include "position.h"
#include "motion.h"
#include "direction.h"
#include "element.h"
#include "pixel.h"
#include "coord.h"
#include "rect.h"

enum BlockCoast_t{
     BLOCK_COAST_NONE,
     BLOCK_COAST_ICE,
     BLOCK_COAST_PLAYER,
     BLOCK_COAST_AIR, // lol coaster
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

     bool       held_up;
     bool       was_on_ice_or_air;

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

     Vec_t accel_magnitudes;
};

S16 get_object_x(Block_t* block);
S16 get_object_y(Block_t* block);
Pixel_t block_center_pixel(Block_t* block);
Pixel_t block_center_pixel(Position_t pos);
Position_t block_get_center(Block_t* block);
Position_t block_get_center(Position_t pos);
Coord_t block_get_coord(Block_t* block);
Coord_t block_get_coord(Position_t pos);
bool blocks_at_collidable_height(S8 a_z, S8 b_z);
Rect_t block_get_rect(Block_t* block);
Rect_t block_get_rect(Pixel_t pixel);

Pixel_t block_bottom_right_pixel(Pixel_t block);
Pixel_t block_top_left_pixel(Pixel_t block);
Pixel_t block_top_right_pixel(Pixel_t block);

void block_stop_horizontally(Block_t* block);
void block_stop_vertically(Block_t* block);

const char* block_coast_to_string(BlockCoast_t coast);

S8 blocks_rotations_between(Block_t* a, Block_t* b);
