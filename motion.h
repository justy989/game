#pragma once

#include "position.h"
#include "vec.h"

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
};

MotionComponent_t* motion_x_component(Motion_t* motion);
MotionComponent_t* motion_y_component(Motion_t* motion);

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             float dt, float accel, float accel_distance);

float calc_position_motion(float v, float a, float dt);
float calc_velocity_motion(float v, float a, float dt);

float calc_accel_component_move(Move_t move, float accel);
