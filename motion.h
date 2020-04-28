#pragma once

#include "position.h"
#include "direction.h"
#include "block_cut.h"

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

#pragma pack(push, 1)
struct Motion_t{
     Vec_t pos_delta;
     Vec_t prev_vel;
     Vec_t vel;
     Vec_t accel;

     Vec_t coast_vel; // when we want to start coasting, continue accelerating until we reach this velocity
     Vec_t target_vel;

     // if you add fields to the end of this struct, you need to update MotionComponent_t
};

struct GridMotion_t : public Motion_t{
     union{
         S16 stop_distance_pixel_x;
         F32 padding_1;
     };

     S16 stop_distance_pixel_y;

     union{
          S16 started_on_pixel_x;
          F32 padding_2;
     };

     S16 started_on_pixel_y;

     union{
          S16 stop_on_pixel_x;
          F32 padding_3;
     };

     S16 stop_on_pixel_y;

     Move_t horizontal_move;
     Move_t vertical_move;
};

struct MotionComponentRef_t{
     F32 pos_delta;
     F32 padding_1;
     F32 prev_vel;
     F32 padding_2;
     F32 vel;
     F32 padding_3;
     F32 accel;
     F32 padding_4;
     F32 coast_vel;
     F32 padding_5;
     F32 target_vel;
     F32 padding_6;
     S16 stop_distance_pixel;
     F32 padding_7;
     S16 start_on_pixel;
     F32 padding_8;
     S16 stop_on_pixel;
};
#pragma pack(pop)

struct MotionComponent_t{
     MotionComponentRef_t* ref = nullptr;
     bool is_x = false;
};

struct DecelToStopResult_t{
     F32 accel = 0;
     F32 time = 0;
};

MotionComponent_t motion_x_component(Motion_t* motion);
MotionComponent_t motion_y_component(Motion_t* motion);

GridMotion_t copy_motion_from_component(MotionComponent_t* motion);
F32 calc_coast_motion_time_left(BlockCut_t cut, MotionComponent_t* motion, F32 pos);
F32 calc_coast_motion_time_left(BlockCut_t cut, F32 pos, F32 vel);

F32 calc_accel_from_stop(F32 distance, F32 time);
F32 calc_accel_across_distance(F32 vel, F32 distance, F32 time);
F32 calc_accel_over_time(F32 initial_vel, F32 final_vel, F32 time);
F32 calc_vel_over_time(F32 initial_vel, F32 accel, F32 time);
DecelToStopResult_t calc_decel_to_stop(F32 initial_pos, F32 final_pos, F32 initial_velocity);

F32 begin_stopping_grid_aligned_motion(BlockCut_t cut, MotionComponent_t* motion, S16 pos_pixel, F32 pos_decimal, S16 mass);

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             F32 dt, F32 accel, F32 accel_distance);
void update_motion_grid_aligned(BlockCut_t cut, Move_t* move, MotionComponent_t* motion, bool coast, F32 dt, S16 pos_pixel, F32 pos_decimal, S16 mass);

F32 calc_position_motion(F32 v, F32 a, F32 dt);
F32 calc_velocity_motion(F32 v, F32 a, F32 dt);

F32 calc_accel_component_move(Move_t move, F32 accel);

F32 calc_distance_from_derivatives(F32 v, F32 a);

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
MoveDirection_t move_direction_from_directions(Direction_t a, Direction_t b);
