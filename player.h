#pragma once

#include "position.h"
#include "motion.h"
#include "coord.h"
#include "direction.h"
#include "rect.h"

#define PLAYER_RADIUS_IN_SUB_PIXELS 3.5f
#define PLAYER_RADIUS (PLAYER_RADIUS_IN_SUB_PIXELS / 272.0f)
#define PLAYER_SPEED 5.5f
#define PLAYER_WALK_DELAY 0.15f
#define PLAYER_IDLE_SPEED 0.0025f
#define PLAYER_BOW_DRAW_DELAY 0.3f
#define PLAYER_STOP_IDLE_BLOCK_TIMER 0.2f

struct Player_t : public Motion_t {
     Position_t  pos;
     Vec_t       pos_delta_save;

     bool        teleport = false;
     Position_t  teleport_pos;
     Vec_t       teleport_pos_delta;
     Vec_t       teleport_vel;
     Vec_t       teleport_accel; // TODO: I don't know if we will ever need this field
     S16         teleport_pushing_block = 0;
     Direction_t teleport_pushing_block_dir = DIRECTION_COUNT;
     S8          teleport_pushing_block_rotation = 0;
     U8          teleport_rotation = 0;
     Direction_t teleport_face = DIRECTION_LEFT;

     Direction_t face = DIRECTION_LEFT;
     bool        reface = false;

     F32         push_time = 0.0f;
     S8          walk_frame = 0;
     S8          walk_frame_delta = 1;
     F32         walk_frame_time = 0.0;

     bool        has_bow = false;
     F32         bow_draw_time = 0.0;

     Coord_t     clone_start;
     S8          clone_id = 0;
     S32         clone_instance = 0;

     U8          rotation = 0;
     U8          move_rotation[DIRECTION_COUNT];

     S16         pushing_block = -1;
     S16         prev_pushing_block = -1;
     Direction_t pushing_block_dir = DIRECTION_COUNT; // we would use face but the pushing can be through rotated portal
     S8          pushing_block_rotation = 0;

     Direction_t stopping_block_from = DIRECTION_COUNT;
     F32         stopping_block_from_time = 0.0;

     bool        successfully_moved = false;
     bool        carried_by_block = false;
};

// rough estimate since player is 3.5 pixels radius
void get_player_adjacent_positions(Player_t* player, Direction_t direction, Position_t* a, Position_t* b);
