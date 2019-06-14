#include "motion.h"
#include "defines.h"
#include "log.h"
#include "utils.h"
#include "conversion.h"

#include <float.h>
#include <math.h>
#include <string.h>

MotionComponent_t motion_x_component(Motion_t* motion){
     MotionComponent_t comp;
     comp.ref = (MotionComponentRef_t*)(motion);
     comp.is_x = true;
     return comp;
}

MotionComponent_t motion_y_component(Motion_t* motion){
     MotionComponent_t comp;
     comp.ref = (MotionComponentRef_t*)((char*)(motion) + sizeof(float));
     comp.is_x = false;
     return comp;
}

Motion_t copy_motion_from_component(MotionComponent_t* motion){
     Motion_t result;
     memset(&result, 0, sizeof(result));

     if(motion->is_x){
          result.pos_delta.x = motion->ref->pos_delta;
          result.prev_vel.x = motion->ref->prev_vel;
          result.vel.x = motion->ref->vel;
          result.accel.x = motion->ref->accel;
          result.coast_vel.x = motion->ref->coast_vel;
     }else{
          result.pos_delta.y = motion->ref->pos_delta;
          result.prev_vel.y = motion->ref->prev_vel;
          result.vel.y = motion->ref->vel;
          result.accel.y = motion->ref->accel;
          result.coast_vel.y = motion->ref->coast_vel;
     }

     return result;
}

DecelToStopResult_t calc_decel_to_stop(float initial_pos, float final_pos, float initial_velocity){
     DecelToStopResult_t result;
     // pf = pi + 0.5(vf + vi)t
     // (pf - pi) = 0.5(vf + vi)t
     // (pf - pi) / 0.5(vf + vi) = t
     result.time = (final_pos - initial_pos) / (0.5 * initial_velocity);

     // vf = at + vi
     // (vf - vi) / t = a
     result.accel = -initial_velocity / result.time;

     return result;
}

float calc_accel_from_stop(float distance, float time){
     // pf = pi + vft + 1/2at^2
     // d = pf - pi
     // d = vft + 1/2at^2
     // vft = 0
     // d / 1/2t^2 = a
     return distance / (0.5f * time * time);
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
          move->distance += motion->ref->pos_delta;

          if(positive_key_down && negative_key_down){
               motion->ref->accel = -motion->ref->accel;
               move->state = MOVE_STATE_STOPPING;
               break;
          }

          if((!positive_key_down && move->sign == MOVE_SIGN_POSITIVE) ||
             (!negative_key_down && move->sign == MOVE_SIGN_NEGATIVE)){
               motion->ref->accel = -motion->ref->accel;
               move->state = MOVE_STATE_STOPPING;
          }else if(fabs(move->distance) > accel_distance){
               F32 new_accel = -motion->ref->accel;
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
                    motion->ref->pos_delta -= distance_over;
                    break;
               case MOVE_SIGN_NEGATIVE:
                    motion->ref->pos_delta += distance_over;
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

               F32 b = motion->ref->vel / motion->ref->accel;
               F32 c = b * b;
               F32 dt_consumed = sqrt(c + motion->ref->pos_delta / (0.5f * motion->ref->accel)) - b;
               F32 dt_leftover = dt - dt_consumed;

               // simulate up until the point distance we want
               F32 new_vel = motion->ref->prev_vel;
               motion->ref->pos_delta = calc_position_motion(new_vel, motion->ref->accel, dt_consumed);
               new_vel = calc_velocity_motion(new_vel, motion->ref->accel, dt_consumed);

               // motion->ref->pos_delta = mass_move_component_delta(new_vel, motion->ref->accel, dt_consumed);
               // new_vel += motion->ref->accel * dt_consumed;

               // reverse the accel and simulate for the reset of the dt
               motion->ref->accel = new_accel;
               F32 stop_delta = calc_position_motion(new_vel, motion->ref->accel, dt_leftover);
               new_vel = calc_velocity_motion(new_vel, motion->ref->accel, dt_leftover);

               motion->ref->pos_delta += stop_delta;
               motion->ref->vel = new_vel;

               move->distance = stop_delta;
               move->state = new_push_state;
          }
          break;
     case MOVE_STATE_COASTING:
          if(!positive_key_down && move->sign == MOVE_SIGN_POSITIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->ref->accel = -accel;
          }else if(!negative_key_down && move->sign == MOVE_SIGN_NEGATIVE){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               motion->ref->accel = accel;
          }else if(positive_key_down && negative_key_down){
               move->state = MOVE_STATE_STOPPING;
               move->distance = 0;

               if(move->sign == MOVE_SIGN_POSITIVE){
                    motion->ref->accel = -accel;
               }else if(move->sign == MOVE_SIGN_POSITIVE){
                    motion->ref->accel = accel;
               }
          }
          break;
     case MOVE_STATE_STOPPING:
          if((motion->ref->prev_vel >= 0 && motion->ref->vel <= 0) || (motion->ref->prev_vel <= 0 && motion->ref->vel >= 0)){
               move->state = MOVE_STATE_IDLING;
               move->sign = MOVE_SIGN_ZERO;
               move->distance = 0;

               motion->ref->vel = 0;
               motion->ref->accel = 0;
          }
          break;
     }
}

float begin_stopping_grid_aligned_motion(MotionComponent_t* motion, float pos){
     float final_pos = pos + motion->ref->pos_delta;

     bool positive = motion->ref->vel >= 0;
     float goal = closest_grid_center_pixel(TILE_SIZE_IN_PIXELS, (S16)(final_pos / PIXEL_SIZE)) * PIXEL_SIZE;

     if(positive){
          if(goal < final_pos){
               goal += TILE_SIZE;
          }

          // TODO: if we are coasting too close to our target, go to the next target
          // if(fabs(goal - final_pos) < (0.25f * TILE_SIZE)){
          //      goal += TILE_SIZE;
          // }
     }else{
          if(goal > final_pos){
               goal -= TILE_SIZE;
          }

          // TODO: if we are coasting too close to our target, go to the next target
          // if(fabs(goal - final_pos) < (0.25f * TILE_SIZE)){
          //      goal -= TILE_SIZE;
          // }
     }

     auto decel_result = calc_decel_to_stop(final_pos, goal, motion->ref->vel);
     return decel_result.accel;
}

static F32 find_next_grid_center(F32 pos, F32 vel){
     // find the next grid center
     S16 current_pixel = (pos / PIXEL_SIZE);
     S16 offset = current_pixel % TILE_SIZE_IN_PIXELS;
     S16 goal_pixel = current_pixel - offset;

     // if we are moving to the right, get the next grid center, unless we are exactly on the start of the grid
     // then just use it as is
     if(vel > 0 && fabs(pos - (F32)(goal_pixel * PIXEL_SIZE)) > FLT_EPSILON) goal_pixel += TILE_SIZE_IN_PIXELS;

     goal_pixel += HALF_TILE_SIZE_IN_PIXELS;

     return (F32)(goal_pixel) * PIXEL_SIZE;
}

float calc_coast_motion_time_left(MotionComponent_t* motion, float pos){
     // pf = pi + vt
     // t = (pf - pi) / v
     return (find_next_grid_center(pos, motion->ref->vel) - pos) / motion->ref->vel;
}

void update_motion_grid_aligned(Move_t* move, MotionComponent_t* motion, bool coast, float dt, float pos){
     switch(move->state){
     default:
     case MOVE_STATE_IDLING:
          break;
     case MOVE_STATE_COASTING:
          if(!coast){
               motion->ref->accel = begin_stopping_grid_aligned_motion(motion, pos);
               move->state = MOVE_STATE_STOPPING;
          }
          break;
     case MOVE_STATE_STARTING:
     {
          float save_time_left = move->time_left;

          move->time_left -= dt;

          if(move->time_left <= 0){
               float dt_leftover = dt - save_time_left;

               // simulate until time 0 with the current accel
               Motion_t sim_move = copy_motion_from_component(motion);
               MotionComponent_t sim_motion;

               if(motion->is_x){
                    sim_motion = motion_x_component(&sim_move);
               }else{
                    sim_motion = motion_y_component(&sim_move);
               }

               sim_motion.ref->vel = sim_motion.ref->prev_vel;
               sim_motion.ref->pos_delta = calc_position_motion(sim_motion.ref->vel, sim_motion.ref->accel, save_time_left);
               sim_motion.ref->vel = calc_velocity_motion(sim_motion.ref->vel, sim_motion.ref->accel, save_time_left);

               // calculate the new acceleration that will stop us or leave us coasting
               float new_accel = 0;
               MoveState_t new_move_state = MOVE_STATE_COASTING;

               if(coast){
                    if(fabs(sim_motion.ref->coast_vel) > fabs(sim_motion.ref->vel)){
                         new_move_state = MOVE_STATE_STARTING;

                         // vf = vi + at
                         // at = vf - vi
                         // t = (vf - vi) / a
                         move->time_left = (sim_motion.ref->coast_vel - sim_motion.ref->vel) / sim_motion.ref->accel;
                         new_accel = sim_motion.ref->accel;
                    }
               }else{
                    new_accel = begin_stopping_grid_aligned_motion(&sim_motion, pos);
                    new_move_state = MOVE_STATE_STOPPING;
               }

               // simulate until the end of dt with the new accel
               move->state = new_move_state;

               sim_motion.ref->accel = new_accel;
               // sim_motion.ref->pos_delta += mass_move_component_delta(sim_motion.ref->vel, sim_motion.ref->accel, dt_leftover);
               sim_motion.ref->pos_delta += calc_position_motion(sim_motion.ref->vel, sim_motion.ref->accel, dt_leftover);
               sim_motion.ref->vel = calc_velocity_motion(sim_motion.ref->vel, sim_motion.ref->accel, dt_leftover);

               motion->ref->pos_delta = sim_motion.ref->pos_delta;
               motion->ref->vel = sim_motion.ref->vel;
               motion->ref->accel = sim_motion.ref->accel;
          }
     } break;
     case MOVE_STATE_STOPPING:
     {
          if((motion->ref->prev_vel >= 0 && motion->ref->vel <= 0) || (motion->ref->prev_vel <= 0 && motion->ref->vel >= 0)){
               move->state = MOVE_STATE_IDLING;
               move->sign = MOVE_SIGN_ZERO;

               motion->ref->stop_on_pixel = closest_grid_center_pixel(TILE_SIZE_IN_PIXELS, (S16)(pos / PIXEL_SIZE));
               motion->ref->pos_delta = (motion->ref->stop_on_pixel * PIXEL_SIZE) - pos;
               motion->ref->vel = 0.0;
               motion->ref->accel = 0.0;
          }
     } break;
     }
}

float calc_distance_from_derivatives(float v, float a){
     float t = v / a;
     return v * t + 0.5f * a * (t * t);
}

bool operator==(const Move_t& a, const Move_t& b){
     return a.state == b.state &&
            a.sign == b.sign &&
            a.distance == b.distance;
}

bool operator!=(const Move_t& a, const Move_t& b){
     return a.state != b.state ||
            a.sign != b.sign ||
            a.distance != b.distance;
}

const char* move_state_to_string(MoveState_t state){
     switch(state){
     case MOVE_STATE_IDLING:
          return "MOVE_STATE_IDLING";
     case MOVE_STATE_STARTING:
          return "MOVE_STATE_STARTING";
     case MOVE_STATE_COASTING:
          return "MOVE_STATE_COASTING";
     case MOVE_STATE_STOPPING:
          return "MOVE_STATE_STOPPING";
     default:
          break;
     }

     return "MOVE_STATE_UNKNOWN";
}

const char* move_sign_to_string(MoveSign_t sign){
     switch(sign){
     case MOVE_SIGN_ZERO:
          return "MOVE_SIGN_ZERO";
     case MOVE_SIGN_POSITIVE:
          return "MOVE_SIGN_POSITIVE";
     case MOVE_SIGN_NEGATIVE:
          return "MOVE_SIGN_NEGATIVE";
     default:
          break;
     }

     return "MOVE_SIGN_UNKNOWN";
}

void reset_move(Move_t* move){
     move->state = MOVE_STATE_IDLING;
     move->sign = MOVE_SIGN_ZERO;
     move->distance = 0;
}

void move_flip_sign(Move_t* move){
     // intentional no-op for MOVE_SIGN_ZERO

     if(move->sign == MOVE_SIGN_POSITIVE){
          move->sign = MOVE_SIGN_NEGATIVE;
          move->distance = -move->distance;
     }else if(move->sign == MOVE_SIGN_NEGATIVE){
          move->sign = MOVE_SIGN_POSITIVE;
          move->distance = -move->distance;
     }
}

MoveSign_t move_sign_from_vel(F32 vel){
     if(vel > 0){
          return MOVE_SIGN_POSITIVE;
     }else if(vel < 0){
          return MOVE_SIGN_NEGATIVE;
     }

     return MOVE_SIGN_ZERO;
}

bool grid_motion_moving_in_direction(GridMotion_t* grid_motion, Direction_t direction){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          if(grid_motion->horizontal_move.state > MOVE_STATE_IDLING &&
             grid_motion->horizontal_move.sign == MOVE_SIGN_NEGATIVE){
               return true;
          }
          break;
     case DIRECTION_RIGHT:
          if(grid_motion->horizontal_move.state > MOVE_STATE_IDLING &&
             grid_motion->horizontal_move.sign == MOVE_SIGN_POSITIVE){
               return true;
          }
          break;
     case DIRECTION_UP:
          if(grid_motion->vertical_move.state > MOVE_STATE_IDLING &&
             grid_motion->vertical_move.sign == MOVE_SIGN_POSITIVE){
               return true;
          }
          break;
     case DIRECTION_DOWN:
          if(grid_motion->vertical_move.state > MOVE_STATE_IDLING &&
             grid_motion->vertical_move.sign == MOVE_SIGN_NEGATIVE){
               return true;
          }
          break;
     }

     return false;
}

MoveDirection_t move_direction_between(Position_t a, Position_t b){
     auto diff = pos_to_vec(b - a);
     auto abs_diff_x = fabs(diff.x);
     auto abs_diff_y = fabs(diff.y);
     auto xy_diff = fabs(abs_diff_x - abs_diff_y);

     if(xy_diff < PIXEL_SIZE){
          if(diff.x < 0){
               if(diff.y < 0){
                    return MOVE_DIRECTION_LEFT_DOWN;
               }

               return MOVE_DIRECTION_LEFT_UP;
          }

          if(diff.y < 0){
               return MOVE_DIRECTION_RIGHT_DOWN;
          }

          return MOVE_DIRECTION_RIGHT_UP;
     }else if(abs_diff_x > abs_diff_y){
          if(diff.x > 0){
               return MOVE_DIRECTION_RIGHT;
          }

          return MOVE_DIRECTION_LEFT;
     }

     if(diff.y > 0){
          return MOVE_DIRECTION_UP;
     }

     return MOVE_DIRECTION_DOWN;
}

void move_direction_to_directions(MoveDirection_t move_direction, Direction_t* a, Direction_t* b){
     switch(move_direction){
     default:
          *a = DIRECTION_COUNT;
          *b = DIRECTION_COUNT;
          break;
     case MOVE_DIRECTION_LEFT:
          *a = DIRECTION_LEFT;
          *b = DIRECTION_COUNT;
          break;
     case MOVE_DIRECTION_UP:
          *a = DIRECTION_UP;
          *b = DIRECTION_COUNT;
          break;
     case MOVE_DIRECTION_RIGHT:
          *a = DIRECTION_RIGHT;
          *b = DIRECTION_COUNT;
          break;
     case MOVE_DIRECTION_DOWN:
          *a = DIRECTION_DOWN;
          *b = DIRECTION_COUNT;
          break;
     case MOVE_DIRECTION_LEFT_UP:
          *a = DIRECTION_LEFT;
          *b = DIRECTION_UP;
          break;
     case MOVE_DIRECTION_RIGHT_UP:
          *a = DIRECTION_RIGHT;
          *b = DIRECTION_UP;
          break;
     case MOVE_DIRECTION_LEFT_DOWN:
          *a = DIRECTION_LEFT;
          *b = DIRECTION_DOWN;
          break;
     case MOVE_DIRECTION_RIGHT_DOWN:
          *a = DIRECTION_RIGHT;
          *b = DIRECTION_DOWN;
          break;
     }
}

const char* move_direction_to_string(MoveDirection_t move_direction){
     switch(move_direction){
     case MOVE_DIRECTION_LEFT:
          return "MOVE_DIRECTION_LEFT";
     case MOVE_DIRECTION_UP:
          return "MOVE_DIRECTION_UP";
     case MOVE_DIRECTION_RIGHT:
          return "MOVE_DIRECTION_RIGHT";
     case MOVE_DIRECTION_DOWN:
          return "MOVE_DIRECTION_DOWN";
     case MOVE_DIRECTION_LEFT_UP:
          return "MOVE_DIRECTION_LEFT_UP";
     case MOVE_DIRECTION_RIGHT_UP:
          return "MOVE_DIRECTION_RIGHT_UP";
     case MOVE_DIRECTION_LEFT_DOWN:
          return "MOVE_DIRECTION_LEFT_DOWN";
     case MOVE_DIRECTION_RIGHT_DOWN:
          return "MOVE_DIRECTION_RIGHT_DOWN";
     case MOVE_DIRECTION_COUNT:
          return "MOVE_DIRECTION_COUNT";
     default:
          break;
     }

     return "MOVE_DIRECTION_COUNT";
}
