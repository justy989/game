#include "block_utils.h"
#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"

#include <string.h>

Pixel_t g_collided_with_pixel = {};

bool block_adjacent_pixels_to_check(Block_t* block_to_check, Direction_t direction, Pixel_t* a, Pixel_t* b){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.x--;
          *a = pixel;

          // top corner
          pixel.y += BLOCK_SOLID_SIZE_IN_PIXELS;
          *b = pixel;
          return true;
     };
     case DIRECTION_RIGHT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.x += TILE_SIZE_IN_PIXELS;
          *a = pixel;

          // check top corner
          pixel.y += BLOCK_SOLID_SIZE_IN_PIXELS;
          *b = pixel;
          return true;
     };
     case DIRECTION_DOWN:
     {
          // check left corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.y--;
          *a = pixel;

          // check right corner
          pixel.x += BLOCK_SOLID_SIZE_IN_PIXELS;
          *b = pixel;
          return true;
     };
     case DIRECTION_UP:
     {
          // check left corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.y += TILE_SIZE_IN_PIXELS;
          *a = pixel;

          // check right corner
          pixel.x += BLOCK_SOLID_SIZE_IN_PIXELS;
          *b = pixel;
          return true;
     };
     }

     return false;
}

Block_t* block_against_block_in_list(Block_t* block_to_check, Block_t** blocks, S16 block_count, Direction_t direction, Pixel_t* offsets){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(block_to_check, block)) continue;

               Pixel_t pixel_to_check = block->pos.pixel + offsets[i];
               if((pixel_to_check.x + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.x &&
                  pixel_to_check.y >= block_to_check->pos.pixel.y &&
                  pixel_to_check.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_RIGHT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(block_to_check, block)) continue;

               Pixel_t pixel_to_check = block->pos.pixel + offsets[i];
               if(pixel_to_check.x == (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS) &&
                  pixel_to_check.y >= block_to_check->pos.pixel.y &&
                  pixel_to_check.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_DOWN:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(block_to_check, block)) continue;

               Pixel_t pixel_to_check = block->pos.pixel + offsets[i];
               if((pixel_to_check.y + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.y &&
                  pixel_to_check.x >= block_to_check->pos.pixel.x &&
                  pixel_to_check.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_UP:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(block_to_check, block)) continue;

               Pixel_t pixel_to_check = block->pos.pixel + offsets[i];
               if(pixel_to_check.y == (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS) &&
                  pixel_to_check.x >= block_to_check->pos.pixel.x &&
                  pixel_to_check.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     }

     return nullptr;
}

Block_t* block_against_another_block(Block_t* block_to_check, Direction_t direction, QuadTreeNode_t<Block_t>* block_quad_tree,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir){
     Rect_t rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     Block_t* collided_block = block_against_block_in_list(block_to_check, blocks, block_count, direction, portal_offsets);
     if(collided_block){
          *push_dir = direction;
          return collided_block;
     }

     // check adjacent portals
     auto block_coord = block_get_coord(block_to_check);
     Coord_t min = block_coord - Coord_t{1, 1};
     Coord_t max = block_coord + Coord_t{1, 1};

     for(S16 y = min.y; y <= max.y; ++y){
          for(S16 x = min.x; x <= max.x; ++x){
               Coord_t src_coord = {x, y};

               Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, src_coord);
               if(is_active_portal(interactive)){
                    auto portal_exits = find_portal_exits(src_coord, tilemap, interactive_quad_tree);
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                              auto dst_coord = portal_exits.directions[d].coords[p];
                              if(dst_coord == src_coord) continue;

                              search_portal_destination_for_blocks(block_quad_tree, interactive->portal.face, (Direction_t)(d), src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              collided_block = block_against_block_in_list(block_to_check, blocks, block_count, direction, portal_offsets);
                              if(collided_block){
                                   U8 rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));
                                   *push_dir = direction_rotate_clockwise(direction, rotations);
                                   return collided_block;
                              }
                         }
                    }
               }
          }
     }

     return nullptr;
}

Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Interactive_t* interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord);
     if(interactive) return interactive;

     tile_coord = pixel_to_coord(pixel_b);
     interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord);
     if(interactive) return interactive;

     return nullptr;
}

Block_t* block_inside_block_list(Block_t* block_to_check, Block_t** blocks, S16 block_count, ObjectArray_t<Block_t>* blocks_array, Position_t* collided_with, Pixel_t* portal_offsets){
     auto check_pos = block_to_check->pos + block_to_check->pos_delta;
     Rect_t rect = {check_pos.pixel.x, check_pos.pixel.y,
                    (S16)(check_pos.pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS),
                    (S16)(check_pos.pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS)};

     Block_t* entangled_block = nullptr;
     if(block_to_check->entangle_index >= 0){
          entangled_block = blocks_array->elements + block_to_check->entangle_index;
     }

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];

          // don't collide with blocks that are cloning
          if(block == entangled_block && block_to_check->clone_start.x > 0) continue;
          if(block == block_to_check && portal_offsets[i].x == 0 && portal_offsets[i].y == 0) continue;

          auto block_pos = block->pos + block->pos_delta;

          Pixel_t pixel_to_check = block_pos.pixel + portal_offsets[i];

          if(pixel_in_rect(pixel_to_check, rect) ||
             pixel_in_rect(block_top_left_pixel(pixel_to_check), rect) ||
             pixel_in_rect(block_top_right_pixel(pixel_to_check), rect) ||
             pixel_in_rect(block_bottom_right_pixel(pixel_to_check), rect)){
               *collided_with = block_get_center(block);
               collided_with->pixel += portal_offsets[i];
               g_collided_with_pixel = collided_with->pixel;
               return block;
          }
     }

     return nullptr;
}

BlockInsideResult_t block_inside_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree,
                                               QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                               ObjectArray_t<Block_t>* block_array){
     BlockInsideResult_t result = {};

     // TODO: need more complicated function to detect this
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     Block_t* collided_block = block_inside_block_list(block_to_check, blocks, block_count, block_array, &result.collision_pos, portal_offsets);
     if(collided_block){
          result.block = collided_block;
          return result;
     }

     // find portals around the block to check
     auto block_coord = block_get_coord(block_to_check);
     Coord_t min = block_coord - Coord_t{1, 1};
     Coord_t max = block_coord + Coord_t{1, 1};

     for(S16 y = min.y; y <= max.y; ++y){
          for(S16 x = min.x; x <= max.x; ++x){
               Coord_t src_coord = {x, y};

               Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, src_coord);
               if(is_active_portal(interactive)){
                    auto portal_exits = find_portal_exits(src_coord, tilemap, interactive_quad_tree);
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                              auto dst_coord = portal_exits.directions[d].coords[p];
                              if(dst_coord == src_coord) continue;

                              search_portal_destination_for_blocks(block_quad_tree, interactive->portal.face, (Direction_t)(d), src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              collided_block = block_inside_block_list(block_to_check, blocks, block_count, block_array, &result.collision_pos, portal_offsets);
                              if(collided_block){
                                   result.block = collided_block;
                                   result.portal_rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));
                                   result.src_portal_coord = src_coord;
                                   result.dst_portal_coord = dst_coord;
                                   return result;
                              }
                         }
                    }
               }
          }
     }

     return result;
}

Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap,
                                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Pixel_t pixel_a {};
     Pixel_t pixel_b {};

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t skip_coord[DIRECTION_COUNT];
     find_portal_adjacents_to_skip_collision_check(block_get_coord(block_to_check), interactive_quad_tree, skip_coord);

     Coord_t tile_coord = pixel_to_coord(pixel_a);

     bool skip = false;
     for (auto d : skip_coord) {
          if(d == tile_coord){
               skip = true;
               break;
          }
     }

     if(!skip){
          Tile_t* tile = tilemap_get_tile(tilemap, tile_coord);
          if(tile && tile->id) return tile;
     }

     tile_coord = pixel_to_coord(pixel_b);

     skip = false;
     for (auto d : skip_coord) {
          if(d == tile_coord){
               skip = true;
               break;
          }
     }

     if(!skip){
          Tile_t* tile = tilemap_get_tile(tilemap, tile_coord);
          if(tile && tile->id) return tile;
     }

     return nullptr;
}

Block_t* block_held_up_by_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS),
                    (S16)(block_to_check->pos.pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS)};
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     S8 held_at_height = block_to_check->pos.z - HEIGHT_INTERVAL;
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          if(block == block_to_check || block->pos.z != held_at_height) continue;

          if(pixel_in_rect(block->pos.pixel, rect) ||
             pixel_in_rect(block_top_left_pixel(block->pos.pixel), rect) ||
             pixel_in_rect(block_top_right_pixel(block->pos.pixel), rect) ||
             pixel_in_rect(block_bottom_right_pixel(block->pos.pixel), rect)){
               return block;
          }
     }

     return nullptr;
}

bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     if(block->pos.z == 0){
          Pixel_t block_pixel_check = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

          // if the block is moving, to the right or up, then check the pixel adjacent to the center in the direction we are moving
          // this will have to change in the future anyway once blocks are no longer square
          if(block->horizontal_move.state > MOVE_STATE_IDLING &&
             block->horizontal_move.sign == MOVE_SIGN_POSITIVE){
               block_pixel_check.x++;
          }

          if(block->vertical_move.state > MOVE_STATE_IDLING &&
             block->vertical_move.sign == MOVE_SIGN_POSITIVE){
               block_pixel_check.y++;
          }

          Coord_t coord = pixel_to_coord(block_pixel_check);
          Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
          if(interactive){
               if(interactive->type == INTERACTIVE_TYPE_POPUP){
                    if(interactive->popup.lift.ticks == 1 && interactive->popup.iced){
                         return true;
                    }
               }
          }

          return tilemap_is_iced(tilemap, coord);
     }

     // TODO: check for blocks below
     return false;
}

void check_block_collision_with_other_blocks(Block_t* block_to_check, World_t* world){
     for(BlockInsideResult_t block_inside_result = block_inside_another_block(block_to_check, world->block_qt, world->interactive_qt, &world->tilemap, &world->blocks);
         block_inside_result.block && blocks_at_collidable_height(block_to_check, block_inside_result.block);
         block_inside_result = block_inside_another_block(block_to_check, world->block_qt, world->interactive_qt, &world->tilemap, &world->blocks)){

          auto block_center_pixel = block_to_check->pos.pixel + HALF_TILE_SIZE_PIXEL;
          auto quadrant = relative_quadrant(block_center_pixel, block_inside_result.collision_pos.pixel);

          // check if they are on ice before we adjust the position on our block to check
          bool a_on_ice = block_on_ice(block_to_check, &world->tilemap, world->interactive_qt);
          bool b_on_ice = block_on_ice(block_inside_result.block, &world->tilemap, world->interactive_qt);

          Vec_t save_vel = block_to_check->vel;

          if(block_inside_result.block == block_to_check){
               block_to_check->pos = coord_to_pos(block_get_coord(block_to_check));
          }else{
               switch(quadrant){
               default:
                    break;
               case DIRECTION_LEFT:
               {
                    // TODO: compress these cases
                    block_to_check->stop_on_pixel_x = block_inside_result.collision_pos.pixel.x + HALF_TILE_SIZE_IN_PIXELS;

                    Position_t dest_pos;
                    dest_pos.pixel.x = block_to_check->stop_on_pixel_x;
                    dest_pos.decimal.x = 0;
                    dest_pos.z = block_to_check->pos.z;

                    Position_t new_pos_delta = dest_pos - block_to_check->pos;
                    block_to_check->pos_delta.x = pos_y_unit(new_pos_delta);

                    block_to_check->vel.x = 0.0f;
                    block_to_check->accel.x = 0.0f;
                    block_to_check->horizontal_move.state = MOVE_STATE_IDLING;
               } break;
               case DIRECTION_RIGHT:
               {
                    block_to_check->stop_on_pixel_x = (block_inside_result.collision_pos.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;

                    Position_t dest_pos;
                    dest_pos.pixel.x = block_to_check->stop_on_pixel_x;
                    dest_pos.decimal.x = 0;
                    dest_pos.z = block_to_check->pos.z;

                    Position_t new_pos_delta = dest_pos - block_to_check->pos;
                    block_to_check->pos_delta.x = pos_y_unit(new_pos_delta);

                    block_to_check->vel.x = 0.0f;
                    block_to_check->accel.x = 0.0f;
                    block_to_check->horizontal_move.state = MOVE_STATE_IDLING;
               } break;
               case DIRECTION_DOWN:
               {
                    block_to_check->stop_on_pixel_y = block_inside_result.collision_pos.pixel.y + HALF_TILE_SIZE_IN_PIXELS;

                    Position_t dest_pos;
                    dest_pos.pixel.y = block_to_check->stop_on_pixel_y;
                    dest_pos.decimal.y = 0;
                    dest_pos.z = block_to_check->pos.z;

                    Position_t new_pos_delta = dest_pos - block_to_check->pos;
                    block_to_check->pos_delta.y = pos_y_unit(new_pos_delta);

                    block_to_check->vel.y = 0.0f;
                    block_to_check->accel.y = 0.0f;
                    block_to_check->vertical_move.state = MOVE_STATE_IDLING;
               } break;
               case DIRECTION_UP:
               {
                    block_to_check->stop_on_pixel_y = (block_inside_result.collision_pos.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;

                    Position_t dest_pos;
                    dest_pos.pixel.y = block_to_check->stop_on_pixel_y;
                    dest_pos.decimal.y = 0;
                    dest_pos.z = block_to_check->pos.z;

                    Position_t new_pos_delta = dest_pos - block_to_check->pos;
                    block_to_check->pos_delta.y = pos_y_unit(new_pos_delta);

                    block_to_check->vel.y = 0.0f;
                    block_to_check->accel.y = 0.0f;
                    block_to_check->vertical_move.state = MOVE_STATE_IDLING;
               } break;
               }
          }

          for(S16 i = 0; i < world->players.count; i++){
               Player_t* player = world->players.elements + i;
               if(player->pushing_block == (S16)(block_to_check - world->blocks.elements) && quadrant == player->face){
                    player->push_time = 0.0f;
               }
          }

          if(a_on_ice && b_on_ice){
               bool push = true;
               Direction_t push_dir = DIRECTION_COUNT;;

               if(block_inside_result.block == block_to_check){
                    Coord_t block_coord = block_get_coord(block_to_check);
                    Direction_t src_portal_dir = direction_between(block_coord, block_inside_result.src_portal_coord);
                    Direction_t dst_portal_dir = direction_between(block_coord, block_inside_result.dst_portal_coord);
                    DirectionMask_t move_mask = vec_direction_mask(block_to_check->vel);

                    resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_to_check,
                                                        DIRECTION_LEFT, DIRECTION_UP, &push_dir);

                    resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_to_check,
                                                        DIRECTION_LEFT, DIRECTION_DOWN, &push_dir);

                    resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_to_check,
                                                        DIRECTION_RIGHT, DIRECTION_UP, &push_dir);

                    resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_to_check,
                                                        DIRECTION_RIGHT, DIRECTION_DOWN, &push_dir);
               }else{
                    push_dir = direction_rotate_clockwise(quadrant, block_inside_result.portal_rotations);

                    switch(push_dir){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         if(block_inside_result.block->accel.x > 0){
                              block_inside_result.block->accel.x = 0.0f;
                              block_inside_result.block->vel.x = 0.0f;
                              push = false;
                         }
                         break;
                    case DIRECTION_RIGHT:
                         if(block_inside_result.block->accel.x < 0){
                              block_inside_result.block->accel.x = 0.0f;
                              block_inside_result.block->vel.x = 0.0f;
                              push = false;
                         }
                         break;
                    case DIRECTION_DOWN:
                         if(block_inside_result.block->accel.y > 0){
                              block_inside_result.block->accel.y = 0.0f;
                              block_inside_result.block->vel.y = 0.0f;
                              push = false;
                         }
                         break;
                    case DIRECTION_UP:
                         if(block_inside_result.block->accel.y < 0){
                              block_inside_result.block->accel.y = 0.0f;
                              block_inside_result.block->vel.y = 0.0f;
                              push = false;
                         }
                         break;
                    }
               }

               if(push){
                    F32 instant_vel = direction_is_horizontal(push_dir) ? save_vel.x : save_vel.y;
                    block_push(block_inside_result.block, push_dir, world, true, instant_vel);
               }
          }

          // TODO: there is no way this is the right way to do this
          if(block_inside_result.block == block_to_check) break;
     }
}


void resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                         Block_t* block, Direction_t check_horizontal, Direction_t check_vertical, Direction_t* push_dir){
     if(directions_meet_expectations(src_portal_dir, dst_portal_dir, check_horizontal, check_vertical)){
          if(move_mask & direction_to_direction_mask(check_vertical)){
               *push_dir = direction_opposite(check_horizontal);
               block->vel.y = 0;
               block->accel.y = 0;
          }else if(move_mask & direction_to_direction_mask(check_horizontal)){
               *push_dir = direction_opposite(check_vertical);
               block->vel.x = 0;
               block->accel.x = 0;
          }
     }
}

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_quad_tree, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Pixel_t* offsets){
     U8 rotations_between_portals = portal_rotations_between(dst_portal_face, src_portal_face);
     Coord_t dst_coord = dst_portal_coord + direction_opposite(dst_portal_face);
     Pixel_t src_portal_center_pixel = coord_to_pixel_at_center(src_portal_coord);
     Pixel_t dst_center_pixel = coord_to_pixel_at_center(dst_coord);
     Rect_t rect = rect_surrounding_adjacent_coords(dst_coord);
     quad_tree_find_in(block_quad_tree, rect, blocks, block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S8 o = 0; o < *block_count; o++){
          Pixel_t offset = block_center_pixel(blocks[o]) - dst_center_pixel;
          Pixel_t src_fake_pixel = src_portal_center_pixel + pixel_rotate_quadrants_clockwise(offset, rotations_between_portals);
          offsets[o] = src_fake_pixel - block_center_pixel(blocks[o]);
     }
}

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt){
     auto block_coord = block_get_coord(block);
     auto block_rect = block_get_rect(block);
     auto min = block_coord - Coord_t{1, 1};
     auto max = block_coord + Coord_t{1, 1};

     for(auto y = min.y; y <= max.y; y++){
          for(auto x = min.x; x <= max.x; x++){
               Interactive_t* interactive = quad_tree_find_at(interactive_qt, x, y);
               if(!is_active_portal(interactive)) continue;

               auto portal_line = get_portal_line(interactive);

               if(axis_line_intersects_rect(portal_line, block_rect)){
                    return interactive;
               }
          }
     }

     return nullptr;
}
