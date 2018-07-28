#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"

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

Direction_t vec_direction(Vec_t vec){
     if(vec.x != 0) assert(vec.y == 0);
     if(vec.y != 0) assert(vec.x == 0);

     return direction_from_single_mask(vec_direction_mask(vec));
}

U8 portal_rotations_between(Direction_t a, Direction_t b){
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
    return Rect_t{(S16)(center.x - DOUBLE_TILE_SIZE_IN_PIXELS),
                   (S16)(center.y - DOUBLE_TILE_SIZE_IN_PIXELS),
                   (S16)(center.x + DOUBLE_TILE_SIZE_IN_PIXELS),
                   (S16)(center.y + DOUBLE_TILE_SIZE_IN_PIXELS)};
}

Interactive_t* quad_tree_interactive_find_at(QuadTreeNode_t<Interactive_t>* root, Coord_t coord){
     return quad_tree_find_at(root, coord.x, coord.y);
}

Interactive_t* quad_tree_interactive_solid_at(QuadTreeNode_t<Interactive_t>* root, TileMap_t* tilemap, Coord_t coord){
     Interactive_t* interactive = quad_tree_find_at(root, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)){
               return interactive;
          }else if(is_active_portal(interactive)){
               if(!portal_has_destination(coord, tilemap, root)) return interactive;
          }
     }

     return nullptr;
}

// NOTE: skip_coord needs to be DIRECTION_COUNT size
void find_portal_adjacents_to_skip_collision_check(Coord_t coord, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                                   Coord_t* skip_coord){
     for(S8 i = 0; i < DIRECTION_COUNT; i++) skip_coord[i] = {-1, -1};

     // figure out which coords we can skip collision checking on, because they have portal exits
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(is_active_portal(interactive)){
          skip_coord[interactive->portal.face] = coord + interactive->portal.face;
     }
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
     Position_t relative_loc {pixel, 0, {0.0f, 0.0f}};
     return pos_to_vec(relative_loc);
}

Vec_t mass_move(Vec_t* vel, Vec_t accel, F32 dt){
     Vec_t pos_dt = (accel * dt * dt * 0.5f) + (*vel * dt);
     *vel += accel * dt;
     *vel *= MASS_DRAG;
     return pos_dt;
}

bool vec_in_quad(const Quad_t* q, Vec_t v){
     return (v.x >= q->left && v.x <= q->right &&
             v.y >= q->bottom && v.y <= q->top);
}
