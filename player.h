#ifndef PLAYER_H
#define PLAYER_H

#define PLAYER_RADIUS (3.5f / 272.0f)
#define PLAYER_SPEED 5.5f
#define PLAYER_WALK_DELAY 0.15f
#define PLAYER_IDLE_SPEED 0.0025f
#define PLAYER_BOW_DRAW_DELAY 0.3f

struct Player_t{
     Position_t pos;
     Vec_t accel;
     Vec_t vel;
     Direction_t face;
     F32 push_time;
     S8 walk_frame;
     S8 walk_frame_delta;
     F32 walk_frame_time;
     bool has_bow;
     F32 bow_draw_time;
};

#endif
