#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"
#include "block.h"

Direction_t direction_between(Coord_t a, Coord_t b){
     if(a == b) return DIRECTION_COUNT;

     Coord_t diff = a - b;

     if(abs(diff.x) > abs(diff.y)){
          if(diff.x > 0){
               return DIRECTION_LEFT;
          }else{
               return DIRECTION_RIGHT;
          }
     }

     if(diff.y > 0){
          return DIRECTION_DOWN;
     }

     return DIRECTION_UP;
}

bool directions_meet_expectations(Direction_t a, Direction_t b, Direction_t first_expectation, Direction_t second_expectation){
     return (a == first_expectation && b == second_expectation) ||
            (a == second_expectation && b == first_expectation);
}

DirectionMask_t vec_direction_mask(Vec_t vec){
     DirectionMask_t mask = DIRECTION_MASK_NONE;

     if(vec.x > 0){
          mask = direction_mask_add(mask, DIRECTION_MASK_RIGHT);
     }else if(vec.x < 0){
          mask = direction_mask_add(mask, DIRECTION_MASK_LEFT);
     }

     if(vec.y > 0){
          mask = direction_mask_add(mask, DIRECTION_MASK_UP);
     }else if(vec.y < 0){
          mask = direction_mask_add(mask, DIRECTION_MASK_DOWN);
     }

     return mask;
}

DirectionMask_t pixel_direction_mask_between(Pixel_t a, Pixel_t b){
     DirectionMask_t mask = DIRECTION_MASK_NONE;
     S16 x_diff = b.x - a.x;
     S16 y_diff = b.y - a.y;

     if(x_diff > 0) mask = direction_mask_add(mask, DIRECTION_MASK_RIGHT);
     if(x_diff < 0) mask = direction_mask_add(mask, DIRECTION_MASK_LEFT);
     if(y_diff > 0) mask = direction_mask_add(mask, DIRECTION_MASK_UP);
     if(y_diff < 0) mask = direction_mask_add(mask, DIRECTION_MASK_DOWN);

     return mask;
}

Direction_t vec_direction(Vec_t vec){
     return direction_from_single_mask(vec_direction_mask(vec));
}

U8 portal_rotations_between(Direction_t a, Direction_t b){
     if(a >= DIRECTION_COUNT || b >= DIRECTION_COUNT) return DIRECTION_COUNT;
     if(a == b) return 2;
     if(a == direction_opposite(b)) return 0;
     return direction_rotations_between(a, b);
}

Vec_t vec_rotate_quadrants_clockwise(Vec_t vec, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = vec.x;
          vec.x = vec.y;
          vec.y = -tmp;
     }

     return vec;
}

Pixel_t pixel_rotate_quadrants_clockwise(Pixel_t pixel, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = pixel.x;
          pixel.x = pixel.y;
          pixel.y = -tmp;
     }

     return pixel;
}

Position_t position_rotate_quadrants_clockwise(Position_t pos, S8 rotations_between){
     pos.decimal = vec_rotate_quadrants_clockwise(pos.decimal, rotations_between);
     pos.pixel = pixel_rotate_quadrants_clockwise(pos.pixel, rotations_between);
     canonicalize(&pos);
     return pos;
}

Vec_t vec_rotate_quadrants_counter_clockwise(Vec_t vec, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = vec.x;
          vec.x = -vec.y;
          vec.y = tmp;
     }

     return vec;
}

Pixel_t pixel_rotate_quadrants_counter_clockwise(Pixel_t pixel, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = pixel.x;
          pixel.x = -pixel.y;
          pixel.y = tmp;
     }

     return pixel;
}

Position_t position_rotate_quadrants_counter_clockwise(Position_t pos, S8 rotations_between){
     pos.decimal = vec_rotate_quadrants_counter_clockwise(pos.decimal, rotations_between);
     pos.pixel = pixel_rotate_quadrants_counter_clockwise(pos.pixel, rotations_between);
     canonicalize(&pos);
     return pos;
}

Vec_t rotate_vec_between_dirs_clockwise(Direction_t a, Direction_t b, Vec_t vec){
     U8 rotations_between = portal_rotations_between(a, b);
     return vec_rotate_quadrants_clockwise(vec, rotations_between);
}

Vec_t direction_to_vec(Direction_t d){
     switch(d){
     default:
          assert(!"wat");
          break;
     case DIRECTION_LEFT:
          return Vec_t{-1, 0};
     case DIRECTION_RIGHT:
          return Vec_t{1, 0};
     case DIRECTION_UP:
          return Vec_t{0, 1};
     case DIRECTION_DOWN:
          return Vec_t{0, -1};
     }

     return Vec_t{0, 0};
}

Rect_t rect_surrounding_coord(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);
     return Rect_t{pixel.x,
                   pixel.y,
                   (S16)(pixel.x + TILE_SIZE_IN_PIXELS),
                   (S16)(pixel.y + TILE_SIZE_IN_PIXELS)};
}

Rect_t rect_surrounding_adjacent_coords(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);
     return Rect_t {(S16)(pixel.x - TILE_SIZE_IN_PIXELS),
                    (S16)(pixel.y - TILE_SIZE_IN_PIXELS),
                    (S16)(pixel.x + DOUBLE_TILE_SIZE_IN_PIXELS),
                    (S16)(pixel.y + DOUBLE_TILE_SIZE_IN_PIXELS)};
}

Rect_t rect_to_check_surrounding_blocks(Pixel_t center){
    return Rect_t{(S16)(center.x - (TILE_SIZE_IN_PIXELS + 1)),
                   (S16)(center.y - (TILE_SIZE_IN_PIXELS + 1)),
                   (S16)(center.x + (TILE_SIZE_IN_PIXELS + 1)),
                   (S16)(center.y + (TILE_SIZE_IN_PIXELS + 1))};
}

Interactive_t* quad_tree_interactive_find_at(QuadTreeNode_t<Interactive_t>* root, Coord_t coord){
     return quad_tree_find_at(root, coord.x, coord.y);
}

Interactive_t* quad_tree_interactive_solid_at(QuadTreeNode_t<Interactive_t>* root, TileMap_t* tilemap, Coord_t coord, S8 check_height){
     Interactive_t* interactive = quad_tree_find_at(root, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)){
                if(interactive->type == INTERACTIVE_TYPE_POPUP && (interactive->popup.lift.ticks - 1) <= check_height){
                     // pass
                }else{
                     return interactive;
                }
          }else if(is_active_portal(interactive)){
               if(!portal_has_destination(coord, tilemap, root)) return interactive;
               if(check_height >= PORTAL_MAX_HEIGHT) return interactive;
          }
     }

     return nullptr;
}

bool portal_has_destination(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     bool result = false;
     // search all portal exits for a portal they can go through
     PortalExit_t portal_exits = find_portal_exits(coord, tilemap, interactive_quad_tree);
     for(S8 d = 0; d < DIRECTION_COUNT && !result; d++){
          for(S8 p = 0; p < portal_exits.directions[d].count; p++){
               if(portal_exits.directions[d].coords[p] == coord) continue;

               Coord_t portal_dest = portal_exits.directions[d].coords[p];
               Interactive_t* portal_dest_interactive = quad_tree_find_at(interactive_quad_tree, portal_dest.x, portal_dest.y);
               if(is_active_portal(portal_dest_interactive)){
                    result = true;
                    break;
               }
          }
     }

     return result;
}

Interactive_t* player_is_teleporting(const Player_t* player, QuadTreeNode_t<Interactive_t>* interactive_qt){
     auto player_coord = pos_to_coord(player->pos);
     auto min = player_coord - Coord_t{1, 1};
     auto max = player_coord + Coord_t{1, 1};

     for(int y = min.y; y <= max.y; y++){
          for(int x = min.x; x <= max.x; x++){
               Interactive_t* interactive = quad_tree_find_at(interactive_qt, x, y);
               if(!is_active_portal(interactive)) continue;

               auto portal_line = get_portal_line(interactive);

               if(axis_line_intersects_circle(portal_line, player->pos.pixel, PLAYER_RADIUS_IN_SUB_PIXELS)){
                    return interactive;
               }
          }
     }
     return nullptr;
}

S16 range_passes_tile_boundary(S16 a, S16 b, S16 ignore){
     if(a == b) return 0;
     if(a > b){
          if((b % TILE_SIZE_IN_PIXELS) == 0) return 0;
          SWAP(a, b);
     }

     for(S16 i = a; i <= b; i++){
          if((i % TILE_SIZE_IN_PIXELS) == 0 && i != ignore){
               return i;
          }
     }

     return 0;
}

Pixel_t mouse_select_pixel(Vec_t mouse_screen){
     return {(S16)(mouse_screen.x * (F32)(ROOM_PIXEL_SIZE)), (S16)(mouse_screen.y * (F32)(ROOM_PIXEL_SIZE))};
}

Pixel_t mouse_select_world_pixel(Vec_t mouse_screen, Position_t camera){
     return mouse_select_pixel(mouse_screen) + (camera.pixel - Pixel_t{ROOM_PIXEL_SIZE / 2, ROOM_PIXEL_SIZE / 2});
}

Coord_t mouse_select_coord(Vec_t mouse_screen){
     return {(S16)(mouse_screen.x * (F32)(ROOM_TILE_SIZE)), (S16)(mouse_screen.y * (F32)(ROOM_TILE_SIZE))};
}

Coord_t mouse_select_world(Vec_t mouse_screen, Position_t camera){
     return mouse_select_coord(mouse_screen) + (pos_to_coord(camera) - Coord_t{ROOM_TILE_SIZE / 2, ROOM_TILE_SIZE / 2});
}

Vec_t coord_to_screen_position(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);
     Position_t relative_loc {pixel, {0.0f, 0.0f}, 0};
     return pos_to_vec(relative_loc);
}

bool vec_in_quad(const Quad_t* q, Vec_t v){
     return (v.x >= q->left && v.x <= q->right &&
             v.y >= q->bottom && v.y <= q->top);
}

S16 closest_grid_center_pixel(S16 grid_width, S16 v){
     S16 lower_index = v / grid_width;
     S16 upper_index = lower_index + 1;

     S16 lower_bound = grid_width * lower_index;
     S16 upper_bound = grid_width * upper_index;

     S16 lower_diff = v - lower_bound;
     S16 upper_diff = upper_bound - v;

     if(lower_diff < upper_diff) return lower_bound;

     return upper_bound;
}

S16 closest_pixel(S16 pixel, F32 decimal){
     if(decimal > (PIXEL_SIZE / 2.0)){
          return pixel + 1;
     }

     return pixel;
}

S16 passes_over_grid_pixel(S16 pixel_a, S16 pixel_b){
     if(pixel_a == pixel_b){
          if((pixel_a % TILE_SIZE_IN_PIXELS) == 0) return pixel_a;
          return -1;
     }

     S16 start = pixel_a;
     S16 end = pixel_b;

     if(pixel_a > pixel_b){
          start = pixel_b;
          end = pixel_a;
     }

     for(S16 i = start; i <= end; i++){
          if((i % TILE_SIZE_IN_PIXELS) == 0) return i;
     }

     return -1;
}

S16 passes_over_pixel(S16 pixel_a, S16 pixel_b){
     if(pixel_a == pixel_b) return pixel_a;
     if(pixel_a > pixel_b) return pixel_a;
     return pixel_b;
}

void get_rect_coords(Rect_t rect, Coord_t* coords){
     coords[0] = pixel_to_coord(Pixel_t{rect.left, rect.bottom});
     coords[1] = pixel_to_coord(Pixel_t{rect.left, rect.top});
     coords[2] = pixel_to_coord(Pixel_t{rect.right, rect.bottom});
     coords[3] = pixel_to_coord(Pixel_t{rect.right, rect.top});
}

Pixel_t closest_pixel_in_rect(Pixel_t pixel, Rect_t rect){
     Pixel_t result;

     if(pixel.x < rect.left){
          result.x = rect.left;
     }else if(pixel.x > rect.right){
          result.x = rect.right;
     }else{
          result.x = pixel.x;
     }

     if(pixel.y < rect.bottom){
          result.y = rect.bottom;
     }else if(pixel.y > rect.top){
          result.y = rect.top;
     }else{
          result.y = pixel.y;
     }

     return result;
}

bool direction_in_vec(Vec_t vec, Direction_t direction){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          return vec.x < 0;
     case DIRECTION_RIGHT:
          return vec.x > 0;
     case DIRECTION_DOWN:
          return vec.y < 0;
     case DIRECTION_UP:
          return vec.y > 0;
     }

     return false;
}

F32 get_block_normal_pushed_velocity(BlockCut_t cut, S16 mass, F32 force){
     S16 width = block_get_width_in_pixels(cut);
     S16 height = block_get_height_in_pixels(cut);

     // the way in which mass can be different is if blocks are stacked on each other
     F32 block_mass_ratio = (F32)(width * height) / (F32)mass;

     S16 half_lowest_dim = (width > height) ? height / 2 : width / 2;
     F32 block_accel_distance = half_lowest_dim * PIXEL_SIZE;

     // accurately accumulate floating point error in the same way simulating does lol
     F32 time_left = BLOCK_ACCEL_TIME;
     F32 accel_time = BLOCK_ACCEL_TIME;

     if(width == HALF_TILE_SIZE_IN_PIXELS || height == HALF_TILE_SIZE_IN_PIXELS){
         time_left *= SMALL_BLOCK_ACCEL_MULTIPLIER;
         accel_time *= SMALL_BLOCK_ACCEL_MULTIPLIER;
     }

     F32 result = 0;
     while(time_left >= 0){
          if(time_left > FRAME_TIME){
               result += block_mass_ratio * FRAME_TIME * BLOCK_ACCEL(block_accel_distance, accel_time) * force;
          }else{
               result += block_mass_ratio * time_left * BLOCK_ACCEL(block_accel_distance, accel_time) * force;
               break;
          }

          time_left -= FRAME_TIME;
     }
     return result;
}

F32 rotate_vec_to_see_if_negates(F32 value, bool x, S8 rotations){
     Vec_t v = vec_zero();
     if(x){
          v.x = value;
     }else{
          v.y = value;
     }

     Vec_t rotated = vec_rotate_quadrants_counter_clockwise(v, rotations);

     if(rotated.x != 0) return rotated.x;
     return rotated.y;
}
