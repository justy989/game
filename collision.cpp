#include "collision.h"
#include "rect.h"
#include "defines.h"
#include "conversion.h"
#include "player.h"
#include "utils.h"

#include <cfloat>

Vec_t collide_circle_with_line(Vec_t circle_center, F32 circle_radius, Vec_t a, Vec_t b, bool* collided){
     // move data we care about to the origin
     Vec_t c = circle_center - a;
     Vec_t l = b - a;
     Vec_t closest_point_on_l_to_c = vec_project_onto(c, l);
     Vec_t collision_to_c = closest_point_on_l_to_c - c;

     if(vec_magnitude(collision_to_c) < circle_radius){
          // find edge of circle in that direction
          Vec_t edge_of_circle = vec_normalize(collision_to_c) * circle_radius;
          Vec_t collision_vec = collision_to_c - edge_of_circle;
          if(collision_vec != vec_zero()){
               *collided = true;
               return collision_vec;
          }
     }

     return vec_zero();
}

void position_collide_with_rect(Position_t pos, Position_t rect_pos, F32 rect_width, F32 rect_height, Vec_t* pos_delta, bool* collides){
     Position_t relative = rect_pos - pos;
     Vec_t bottom_left = pos_to_vec(relative);
     if(vec_magnitude(bottom_left) > (rect_width + rect_height)) return;

     Vec_t top_left {bottom_left.x, bottom_left.y + rect_height};
     Vec_t top_right {bottom_left.x + rect_width, bottom_left.y + rect_height};
     Vec_t bottom_right {bottom_left.x + rect_width, bottom_left.y};

     // TODO: pass in radius
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, top_left, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, top_left, top_right, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, bottom_right, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_right, top_right, collides);
}

void position_slide_against_rect(Position_t pos, Rect_t rect, F32 player_radius, Vec_t* pos_delta, bool* collide_with_coord){
     Position_t relative = pixel_to_pos(Pixel_t{rect.left, rect.bottom}) - pos;
     Vec_t bottom_left = pos_to_vec(relative);
     F32 rect_width = (rect.right - rect.left) * PIXEL_SIZE;
     F32 rect_height = (rect.top - rect.bottom) * PIXEL_SIZE;
     if(vec_magnitude(bottom_left) > (rect_width + rect_height)) return;

     Vec_t top_left {bottom_left.x, bottom_left.y + rect_height};
     Vec_t top_right {bottom_left.x + rect_width, bottom_left.y + rect_height};
     Vec_t bottom_right {bottom_left.x + rect_width, bottom_left.y};

     Pixel_t rect_center{(S16)(rect.left + ((rect.right - rect.left) / 2)),
                         (S16)(rect.bottom + ((rect.top - rect.bottom) / 2))};

     DirectionMask_t mask = pixel_direction_mask_between(rect_center, pos.pixel);

     // TODO: figure out slide when on tile boundaries
     switch((U8)(mask)){
     default:
          break;
     case DIRECTION_MASK_LEFT:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     case DIRECTION_MASK_LEFT | DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_LEFT | DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     }
}

