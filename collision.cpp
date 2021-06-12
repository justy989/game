#include "collision.h"
#include "rect.h"
#include "defines.h"
#include "conversion.h"
#include "player.h"
#include "utils.h"

#include <cfloat>

#define OLD

Vec_t collide_circle_with_line(Vec_t circle_center, F32 circle_radius, Vec_t a, Vec_t b, bool* collided){
     LOG("cen: %.10f, %.10f rad: %.10f, a: %.10f, %.10f b: %.10f, %.10f\n",
         circle_center.x, circle_center.y, circle_radius, a.x, a.y, b.x, b.y);

     // use a as the origin, so offset everything relative to a
     Vec_t c = circle_center - a;
     Vec_t l = b - a;
     Vec_t c_on_l = vec_project_onto(c, l);

     // figure out the vector from the closest l to c
     Vec_t proj_to_c = c_on_l - c;
     if(vec_magnitude(proj_to_c) < circle_radius){
          // find edge of circle in that direction
          Vec_t edge_of_circle = vec_normalize(proj_to_c) * circle_radius;
          Vec_t collision_vec = proj_to_c - edge_of_circle;
          if(collision_vec != vec_zero()){
               *collided = true;
               return collision_vec;
          }
     }

     return vec_zero();
}

Vec_t collide_circle_with_line(Position_t circle_center, PositionLength_t circle_radius, Position_t a, Position_t b, bool* collided) {
     LOG("cen: %d, %d - %.10f, %.10f rad: %d, %.10f, a: %d, %d - %.10f, %.10f b: %d, %d - %.10f, %.10f\n",
         circle_center.pixel.x, circle_center.pixel.y, circle_center.decimal.x, circle_center.decimal.y,
         circle_radius.integer, circle_radius.decimal,
         a.pixel.x, a.pixel.y, a.decimal.x, a.decimal.y,
         b.pixel.x, b.pixel.y, b.decimal.x, b.decimal.y);
     Position_t c = circle_center - a;
     Position_t l = b - a;
     Position_t c_on_l;
     bool vertical = false;

     if(a.pixel.x == b.pixel.x && a.decimal.x == b.decimal.x){
          // we are a vertical line
          c_on_l = Position_t{Pixel_t{l.pixel.x, c.pixel.y}, Vec_t{l.decimal.x, c.decimal.y}, 0};
          vertical = true;
     }else if(a.pixel.y == b.pixel.y && a.decimal.y == b.decimal.y){
          // we are a horizontal line
          c_on_l = Position_t{Pixel_t{c.pixel.x, l.pixel.y}, Vec_t{c.decimal.x, l.decimal.y}, 0};
     }else{
          assert(!"The line we are checking collision for should be horizontal or vertical");
          return vec_zero();
     }

     Position_t proj_onto_c = c_on_l - c;

     if(position_length(proj_onto_c) > circle_radius) {
          Position_t edge_of_circle;
          if (vertical) {
               edge_of_circle = Position_t{Pixel_t{circle_radius.integer, 0}, Vec_t{circle_radius.decimal, 0}, 0};
          }else{
               edge_of_circle = Position_t{Pixel_t{0, circle_radius.integer}, Vec_t{0, circle_radius.decimal}, 0};
          }
          // LOG("proj: %d, %d - %.10f, %.10f edge: %d, %d - %.10f, %.10f\n",
          //     proj_onto_c.pixel.x, proj_onto_c.pixel.y,
          //     proj_onto_c.decimal.x, proj_onto_c.decimal.y,
          //     edge_of_circle.pixel.x, edge_of_circle.pixel.y,
          //     edge_of_circle.decimal.x, edge_of_circle.decimal.y);
          Position_t collision = proj_onto_c - edge_of_circle;
          if(collision != position_zero()) {
               *collided = true;
               return pos_to_vec(collision);
          }
     }

     return vec_zero();
}

void position_collide_with_rect(Position_t pos, Position_t rect_pos, F32 rect_width, F32 rect_height, Vec_t* pos_delta, bool* collides){
     LOG("player: %d, %d - %.10f, %.10f rect: %d, %d - %.10f, %.10f, dim: %.10f, %.10f\n",
         pos.pixel.x, pos.pixel.y, pos.decimal.x, pos.decimal.y,
         rect_pos.pixel.x, rect_pos.pixel.y, rect_pos.decimal.x, rect_pos.decimal.y,
         rect_width, rect_height);

     Position_t relative = rect_pos - pos;

#ifdef OLD
     Vec_t bottom_left = pos_to_vec(relative);

     LOG("  bottom left: %.10f, %.10f -> mag: %.10f rwph: %.10f\n",
         bottom_left.x, bottom_left.y, vec_magnitude(bottom_left), rect_width + rect_height);

     if(vec_magnitude(bottom_left) > (rect_width + rect_height)) return;

     Vec_t top_left {bottom_left.x, bottom_left.y + rect_height};
     Vec_t top_right {bottom_left.x + rect_width, bottom_left.y + rect_height};
     Vec_t bottom_right {bottom_left.x + rect_width, bottom_left.y};

     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, top_left, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, top_left, top_right, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, bottom_right, collides);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_right, top_right, collides);
#else
     Position_t bottom_left = relative;
     auto bottom_left_len = position_length(bottom_left);
     LOG("  bottom left: %d, %d - %.10f, %.10f -> len: %d, %.10f len len: %.10f rwph: %.10f\n",
         bottom_left.pixel.x, bottom_left.pixel.y, bottom_left.decimal.x, bottom_left.decimal.y,
         bottom_left_len.integer, bottom_left_len.decimal,
         position_length_length(bottom_left_len), rect_width + rect_height);
     if(position_length_length(position_length(bottom_left)) > (rect_width + rect_height)) return;

     Position_t top_left = bottom_left + Vec_t{0, rect_height};
     Position_t top_right = bottom_left + Vec_t{rect_width, rect_height};
     Position_t bottom_right = bottom_left + Vec_t{rect_width, 0};

     Position_t circle_center = vec_to_pos(*pos_delta);
     PositionLength_t player_radius = position_length(PLAYER_RADIUS);

     // TODO: pass in radius
     *pos_delta += collide_circle_with_line(circle_center, player_radius, bottom_left, top_left, collides);
     *pos_delta += collide_circle_with_line(circle_center, player_radius, top_left, top_right, collides);
     *pos_delta += collide_circle_with_line(circle_center, player_radius, bottom_left, bottom_right, collides);
     *pos_delta += collide_circle_with_line(circle_center, player_radius, bottom_right, top_right, collides);
#endif
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

