#include "motion.h"
#include "defines.h"

#include <math.h>

MotionComponent_t* motion_x_component(Motion_t* motion){
     return (MotionComponent_t*)(motion);
}

MotionComponent_t* motion_y_component(Motion_t* motion){
     return (MotionComponent_t*)((char*)(motion) + sizeof(float));
}

float calc_accel_component_move(Move_t move){
     switch(move.state){
     default:
     case MOVE_STATE_IDLING:
     case MOVE_STATE_COASTING:
          break;
     case MOVE_STATE_STARTING:
          switch(move.sign){
          default:
               break;
          case MOVE_SIGN_POSITIVE:
               return ACCEL;
          case MOVE_SIGN_NEGATIVE:
               return -ACCEL;
          }
          break;
     case MOVE_STATE_STOPPING:
          switch(move.sign){
          default:
             break;
          case MOVE_SIGN_POSITIVE:
               return -ACCEL;
          case MOVE_SIGN_NEGATIVE:
               return ACCEL;
          }
          break;
     }

     return 0.0f;
}

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             float dt){
     switch(move->state){
     default:
     case MOVE_STATE_IDLING:
          break;
     case MOVE_STATE_STARTING:
          move->distance += motion->pos_delta;

          if((!positive_key_down && move->sign == MOVE_SIGN_POSITIVE) ||
             (!negative_key_down && move->sign == MOVE_SIGN_NEGATIVE)){
               motion->accel = -motion->accel;
               move->state = MOVE_STATE_STOPPING;
          }else if(fabs(move->distance) > ACCEL_DISTANCE){
               F32 new_accel = -motion->accel;
               MoveState_t new_push_state = MOVE_STATE_STOPPING;

               if((positive_key_down && move->sign == MOVE_SIGN_POSITIVE) ||
                  (negative_key_down && move->sign == MOVE_SIGN_NEGATIVE)){
                    new_accel = 0;
                    new_push_state = MOVE_STATE_COASTING;
               }

               float distance_over = fabs(move->distance) - ACCEL_DISTANCE; // TODO: multiply by mass

               switch(move->sign){
               default:
                    break;
               case MOVE_SIGN_POSITIVE:
                    motion->pos_delta -= distance_over;
                    break;
               case MOVE_SIGN_NEGATIVE:
                    motion->pos_delta += distance_over;
                    break;
               }

               // xf = x0 + vt + 1/2at^2
               // xf - x0 = vt + 1/2at^2
               // dx = vt + 1/2at^2
               // dx / 0.5a = (vt / 0.5a) + t^2
               // complete the square
               // b = (0.5(v/0.5a))
               // c = b^2
               // (t^2 + (vt / 0.5a) + c) - c - (dx / 0.5a) = 0
               // (t + b)^2 = c + dx/0.5a
               // t = sqrt(c + dx/0.5a) - b
               // Anthony told me this is just the quadratic formula, which I should have realized

               F32 b = motion->vel / motion->accel;
               F32 c = b * b;
               F32 dt_consumed = sqrt(c + motion->pos_delta / (0.5f * motion->accel)) - b;
               F32 dt_leftover = dt - dt_consumed;

               // simulate up until the point distance we want
               F32 new_vel = motion->prev_vel;
               motion->pos_delta = calc_position_motion(new_vel, motion->accel, dt_consumed);
               new_vel = calc_velocity_motion(new_vel, motion->accel, dt_consumed);

               // motion->pos_delta = mass_move_component_delta(new_vel, motion->accel, dt_consumed);
               // new_vel += motion->accel * dt_consumed;

               // reverse the accel and simulate for the reset of the dt
               motion->accel = new_accel;
               F32 stop_delta = calc_position_motion(new_vel, motion->accel, dt_leftover);
               new_vel = calc_velocity_motion(new_vel, motion->accel, dt_leftover);

               motion->pos_delta += stop_delta;
               motion->vel = new_vel;

               move->distance = stop_delta;
               move->state = new_push_state;
          }
          break;
     case MOVE_STATE_COASTING:
          if(positive_key_down && move->sign == MOVE_SIGN_POSITIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->accel = -ACCEL;
          }else if(negative_key_down && move->sign == MOVE_SIGN_NEGATIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->accel = ACCEL;
          }
          break;
     case MOVE_STATE_STOPPING:
          if((motion->prev_vel > 0 && motion->vel < 0) || (motion->prev_vel < 0 && motion->vel > 0)){
               move->state = MOVE_STATE_IDLING;
               move->sign = MOVE_SIGN_ZERO;
               move->distance = 0;

               motion->vel = 0;
               motion->accel = 0;
          }
          break;
     }
}

float calc_position_motion(float v, float a, float dt){
     return (v * dt) + (0.5f * a * dt * dt);
}

float calc_velocity_motion(float v, float a, float dt){
     return v + a * dt;
}
