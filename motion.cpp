#include "motion.h"
#include "defines.h"
#include "position.h"
#include "log.h"

#include <math.h>

MotionComponent_t* motion_x_component(Motion_t* motion){
     return (MotionComponent_t*)(motion);
}

MotionComponent_t* motion_y_component(Motion_t* motion){
     return (MotionComponent_t*)((char*)(motion) + sizeof(float));
}

float calc_accel_component_move(Move_t move, float accel){
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
               return accel;
          case MOVE_SIGN_NEGATIVE:
               return -accel;
          }
          break;
     case MOVE_STATE_STOPPING:
          switch(move.sign){
          default:
             break;
          case MOVE_SIGN_POSITIVE:
               return -accel;
          case MOVE_SIGN_NEGATIVE:
               return accel;
          }
          break;
     }

     return 0.0f;
}

float calc_position_motion(float v, float a, float dt){
     return (v * dt) + (0.5f * a * dt * dt);
}

float calc_velocity_motion(float v, float a, float dt){
     return v + a * dt;
}

void update_motion_free_form(Move_t* move, MotionComponent_t* motion, bool positive_key_down, bool negative_key_down,
                             float dt, float accel, float accel_distance){
     switch(move->state){
     default:
     case MOVE_STATE_IDLING:
          break;
     case MOVE_STATE_STARTING:
          move->distance += motion->pos_delta;

          if(positive_key_down && negative_key_down){
               motion->accel = -motion->accel;
               move->state = MOVE_STATE_STOPPING;
               break;
          }

          if((!positive_key_down && move->sign == MOVE_SIGN_POSITIVE) ||
             (!negative_key_down && move->sign == MOVE_SIGN_NEGATIVE)){
               motion->accel = -motion->accel;
               move->state = MOVE_STATE_STOPPING;
          }else if(fabs(move->distance) > accel_distance){
               F32 new_accel = -motion->accel;
               MoveState_t new_push_state = MOVE_STATE_STOPPING;

               if((positive_key_down && move->sign == MOVE_SIGN_POSITIVE) ||
                  (negative_key_down && move->sign == MOVE_SIGN_NEGATIVE)){
                    new_accel = 0;
                    new_push_state = MOVE_STATE_COASTING;
               }

               float distance_over = fabs(move->distance) - accel_distance; // TODO: multiply by mass

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
          if(!positive_key_down && move->sign == MOVE_SIGN_POSITIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->accel = -accel;
          }else if(!negative_key_down && move->sign == MOVE_SIGN_NEGATIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->accel = accel;
          }else if(positive_key_down && negative_key_down){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               if(move->sign == MOVE_SIGN_POSITIVE){
                    motion->accel = -accel;
               }else if(move->sign == MOVE_SIGN_POSITIVE){
                    motion->accel = accel;
               }
          }
          break;
     case MOVE_STATE_STOPPING:
          if((motion->prev_vel >= 0 && motion->vel <= 0) || (motion->prev_vel <= 0 && motion->vel >= 0)){
               move->state = MOVE_STATE_IDLING;
               move->sign = MOVE_SIGN_ZERO;
               move->distance = 0;

               motion->vel = 0;
               motion->accel = 0;
          }
          break;
     }
}

static S16 closest_grid_center_pixel(S16 grid_width, S16 v){
     S16 lower_index = v / grid_width;
     S16 upper_index = lower_index + 1;

     S16 lower_bound = grid_width * lower_index;
     S16 upper_bound = grid_width * upper_index;

     S16 lower_diff = v - lower_bound;
     S16 upper_diff = upper_bound - v;

     if(lower_diff < upper_diff) return lower_bound;

     return upper_bound;
}

void update_motion_grid_aligned(Move_t* move, MotionComponent_t* motion, bool coast, float dt, float accel,
                                float accel_distance, float pos){
     switch(move->state){
     default:
     case MOVE_STATE_IDLING:
          break;
     case MOVE_STATE_COASTING:
          move->distance += motion->pos_delta;

          if(fabs(move->distance) > accel_distance){
               if(coast){
                    move->distance = fmod(move->distance, accel_distance);
               }else{
                    float distance_over = fabs(move->distance) - accel_distance;

                    switch(move->sign){
                    default:
                         break;
                    case MOVE_SIGN_POSITIVE:
                         motion->pos_delta -= distance_over;
                         motion->accel = -accel;
                         break;
                    case MOVE_SIGN_NEGATIVE:
                         motion->pos_delta += distance_over;
                         motion->accel = accel;
                         break;
                    }

                    // dx = vt
                    // dx / v = t
                    float dt_consumed = motion->pos_delta / motion->vel;

                    // simulate with reverse accel for the reset of the dt
                    float dt_leftover = dt - dt_consumed;
                    float stop_delta = calc_position_motion(motion->vel, motion->accel, dt_leftover);

                    motion->vel += motion->accel * dt_leftover;
                    motion->pos_delta += stop_delta;

                    move->distance = stop_delta;
                    move->state = MOVE_STATE_STOPPING;
               }
          }
          break;
     case MOVE_STATE_STARTING:
     {
          // how far have we moved last frame?
          move->distance += motion->pos_delta;

          if(fabs(move->distance) > accel_distance){
               float new_accel = -motion->accel;
               MoveState_t new_move_state = MOVE_STATE_STOPPING;

               if(coast){
                    new_accel = 0;
                    new_move_state = MOVE_STATE_COASTING;
               }

               float distance_over = fabs(move->distance) - accel_distance;

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

               float b = motion->vel / motion->accel;
               float c = b * b;
               float dt_consumed = sqrt(c + motion->pos_delta / (0.5f * motion->accel)) - b;
               float dt_leftover = dt - dt_consumed;

               // simulate up until the point distance we want
               float new_vel = motion->prev_vel;
               motion->pos_delta = calc_position_motion(new_vel, motion->accel, dt_consumed);
               new_vel += motion->accel * dt_consumed;

               // reverse the accel and simulate for the reset of the dt
               motion->accel = new_accel;
               float stop_delta = calc_position_motion(new_vel, motion->accel, dt_leftover);

               new_vel += motion->accel * dt_leftover;
               motion->pos_delta += stop_delta;
               motion->vel = new_vel;

               move->distance = stop_delta;
               move->state = new_move_state;
          }
     } break;
     case MOVE_STATE_STOPPING:
     {
          move->distance += motion->pos_delta;

          if((motion->prev_vel > 0 && motion->vel < 0) || (motion->prev_vel < 0 && motion->vel > 0)){
               move->state = MOVE_STATE_IDLING;
               move->sign = MOVE_SIGN_ZERO;
               move->distance = 0;

               motion->stop_on_pixel = closest_grid_center_pixel(TILE_SIZE_IN_PIXELS, (S16)(pos / PIXEL_SIZE));
               motion->pos_delta = (motion->stop_on_pixel * PIXEL_SIZE) - pos;
               motion->vel = 0.0;
               motion->accel = 0.0;
          }
     } break;
     }
}
