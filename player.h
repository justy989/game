#pragma once

#include "position.h"
#include "motion.h"
#include "coord.h"
#include "direction.h"

#define PLAYER_RADIUS_IN_SUB_PIXELS 3.5f
#define PLAYER_RADIUS (PLAYER_RADIUS_IN_SUB_PIXELS / 272.0f)
#define PLAYER_SPEED 5.5f
#define PLAYER_WALK_DELAY 0.15f
#define PLAYER_IDLE_SPEED 0.0025f
#define PLAYER_BOW_DRAW_DELAY 0.3f

struct Player_t : public Motion_t {
     Position_t  pos;

     bool        teleport;
     Position_t  teleport_pos;
     Vec_t       teleport_pos_delta;
     S16         teleport_pushing_block;
     Direction_t teleport_pushing_block_dir;
     S8          teleport_pushing_block_rotation;
     U8          teleport_rotation;
     Direction_t teleport_face;

     Direction_t face = DIRECTION_LEFT;
     bool        reface = false;

     F32         push_time = 0.0f;
     S8          walk_frame = 0;
     S8          walk_frame_delta = 0;
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

     bool successfully_moved = false;
};
