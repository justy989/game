#pragma once

#include "position.h"
#include "vec.h"
#include "direction.h"

#define PLAYER_RADIUS (3.5f / 272.0f)
#define PLAYER_SPEED 5.5f
#define PLAYER_WALK_DELAY 0.15f
#define PLAYER_IDLE_SPEED 0.0025f
#define PLAYER_BOW_DRAW_DELAY 0.3f

struct Player_t{
     Position_t pos;
     Vec_t accel;
     Vec_t vel;
     Direction_t face = DIRECTION_LEFT;
     F32 push_time = 0.0f;
     S8 walk_frame = 0;
     S8 walk_frame_delta = 0;
     F32 walk_frame_time = 0.0;
     bool has_bow = false;
     F32 bow_draw_time = 0.0;
};
