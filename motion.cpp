#include "motion.h"
#include "defines.h"
#include "log.h"
#include "utils.h"
#include "conversion.h"

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

          if((motion->prev_vel >= 0 && motion->vel <= 0) || (motion->prev_vel <= 0 && motion->vel >= 0)){
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
