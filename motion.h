#pragma once

#include "position.h"
#include "direction.h"

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

enum MoveDirection_t :U8{
     MOVE_DIRECTION_LEFT = 0,
     MOVE_DIRECTION_UP = 1,
     MOVE_DIRECTION_RIGHT = 2,
     MOVE_DIRECTION_DOWN = 3,
     MOVE_DIRECTION_LEFT_UP = 4,
     MOVE_DIRECTION_RIGHT_UP = 5,
     MOVE_DIRECTION_LEFT_DOWN = 6,
     MOVE_DIRECTION_RIGHT_DOWN = 7,
     MOVE_DIRECTION_COUNT = 8,
};

struct Move_t{
     MoveState_t state;
     MoveSign_t sign;

     union{
          F32 distance;
          F32 time_left;
     };
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

struct MotionComponentRef_t{
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

struct MotionComponent_t{
     MotionComponentRef_t* ref = nullptr;
     bool is_x = false;
};

struct DecelToStopResult_t{
     float accel = 0;
     float time = 0;
};

MotionComponent_t motion_x_component(Motion_t* motion);
MotionComponent_t motion_y_component(Motion_t* motion);

Motion_t copy_motion_from_component(MotionComponent_t* motion);

float calc_accel_from_stop(float distance, float time);
DecelToStopResult_t calc_decel_to_stop(float initial_pos, float final_pos, float initial_velocity);

float begin_stopping_grid_aligned_motion(MotionComponent_t* motion, float pos);

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             float dt, float accel, float accel_distance);
void update_motion_grid_aligned(Move_t* move, MotionComponent_t* motion, bool coast, float dt, float pos);

float calc_position_motion(float v, float a, float dt);
float calc_velocity_motion(float v, float a, float dt);

float calc_accel_component_move(Move_t move, float accel);

float calc_distance_from_derivatives(float v, float a);

bool operator==(const Move_t& a, const Move_t& b);
bool operator!=(const Move_t& a, const Move_t& b);

void reset_move(Move_t* move);
void move_flip_sign(Move_t* move);
MoveSign_t move_sign_from_vel(F32 vel);

const char* move_state_to_string(MoveState_t state);
const char* move_sign_to_string(MoveSign_t sign);

bool grid_motion_moving_in_direction(GridMotion_t* grid_motion, Direction_t direction);

const char* move_direction_to_string(MoveDirection_t move_direction);
MoveDirection_t move_direction_between(Position_t a, Position_t b);
void move_direction_to_directions(MoveDirection_t move_direction, Direction_t* a, Direction_t* b);
