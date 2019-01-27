#pragma once

#include "vec.h"
#include "pixel.h"

enum MoveState_t{
     MOVE_STATE_IDLING,
     MOVE_STATE_STARTING,
     MOVE_STATE_COASTING,
     MOVE_STATE_STOPPING,
};

enum MoveSign_t{
     MOVE_SIGN_ZERO,
     MOVE_SIGN_POSITIVE,
     MOVE_SIGN_NEGATIVE,
};

struct Move_t{
     MoveState_t state;
     MoveSign_t sign;
     F32 distance;
};

struct Motion_t{
     Vec_t pos_delta;
     Vec_t prev_vel;
     Vec_t vel;
     Vec_t accel;

     // if you add fields to the end of this struct, you need to update MotionComponent_t
};

struct GridMotion_t : public Motion_t{
     union{
          S16 started_on_pixel_x;
          F32 padding_2;
     };

     S16 started_on_pixel_y;

     union{
          S16 stop_on_pixel_x;
          F32 padding_0;
     };

     S16 stop_on_pixel_y;

     Move_t horizontal_move;
     Move_t vertical_move;
};

struct MotionComponent_t{
     F32 pos_delta;
     F32 padding_0;
     F32 prev_vel;
     F32 padding_1;
     F32 vel;
     F32 padding_3;
     F32 accel;
     F32 padding_4;
     S16 start_on_pixel;
     F32 padding_5;
     S16 stop_on_pixel;
};

MotionComponent_t* motion_x_component(Motion_t* motion);
MotionComponent_t* motion_y_component(Motion_t* motion);

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             float dt, float accel, float accel_distance);
void update_motion_grid_aligned(Move_t* move, MotionComponent_t* motion, bool coast, float dt, float accel,
                                float accel_distance, float pos);

float calc_position_motion(float v, float a, float dt);
float calc_velocity_motion(float v, float a, float dt);

float calc_accel_component_move(Move_t move, float accel);

float calc_distance_from_derivatives(float v, float a);

bool operator==(const Move_t& a, const Move_t& b);
bool operator!=(const Move_t& a, const Move_t& b);

void reset_move(Move_t* move);

const char* move_state_to_string(MoveState_t state);
const char* move_sign_to_string(MoveSign_t sign);
