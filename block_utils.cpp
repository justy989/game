#include "block_utils.h"
#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"

#include <string.h>
#include <math.h>

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, Direction_t direction, Pixel_t* a, Pixel_t* b){
     auto block_to_check_pos = pos + pos_delta;

     // TODO: account for width/height
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check_pos.pixel;
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
          Pixel_t pixel = block_to_check_pos.pixel;
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
          Pixel_t pixel = block_to_check_pos.pixel;
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
          Pixel_t pixel = block_to_check_pos.pixel;
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

Block_t* block_against_block_in_list(Position_t pos, Block_t** blocks, S16 block_count, Direction_t direction, Pixel_t* offsets){
     // TODO: account for block width/height
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, block->pos.z)) continue;

               auto block_pos = block->pos + block->pos_delta;

               Pixel_t pixel_to_check = block_pos.pixel + offsets[i];
               if((pixel_to_check.x + TILE_SIZE_IN_PIXELS) == pos.pixel.x &&
                  pixel_to_check.y >= (pos.pixel.y - BLOCK_SOLID_SIZE_IN_PIXELS) &&
                  pixel_to_check.y < (pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_RIGHT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, block->pos.z)) continue;

               auto block_pos = block->pos + block->pos_delta;

               Pixel_t pixel_to_check = block_pos.pixel + offsets[i];
               if(pixel_to_check.x == (pos.pixel.x + TILE_SIZE_IN_PIXELS) &&
                  pixel_to_check.y >= (pos.pixel.y - BLOCK_SOLID_SIZE_IN_PIXELS) &&
                  pixel_to_check.y < (pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_DOWN:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, block->pos.z)) continue;

               auto block_pos = block->pos + block->pos_delta;

               Pixel_t pixel_to_check = block_pos.pixel + offsets[i];
               if((pixel_to_check.y + TILE_SIZE_IN_PIXELS) == pos.pixel.y &&
                  pixel_to_check.x >= (pos.pixel.x - BLOCK_SOLID_SIZE_IN_PIXELS) &&
                  pixel_to_check.x < (pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_UP:
          for(S16 i = 0; i < block_count; i++){
               Block_t* block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, block->pos.z)) continue;

               auto block_pos = block->pos + block->pos_delta;

               Pixel_t pixel_to_check = block_pos.pixel + offsets[i];
               if(pixel_to_check.y == (pos.pixel.y + TILE_SIZE_IN_PIXELS) &&
                  pixel_to_check.x >= (pos.pixel.x - BLOCK_SOLID_SIZE_IN_PIXELS) &&
                  pixel_to_check.x < (pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     }

     return nullptr;
}

Block_t* block_against_another_block(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir){
     auto block_center = block_get_center(pos);
     Rect_t rect = rect_to_check_surrounding_blocks(block_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     Block_t* collided_block = block_against_block_in_list(pos, blocks, block_count, direction, portal_offsets);
     if(collided_block){
          *push_dir = direction;
          return collided_block;
     }

     // check adjacent portals
     auto block_coord = pos_to_coord(block_center);
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

                              search_portal_destination_for_blocks(block_qt, interactive->portal.face, (Direction_t)(d), src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              collided_block = block_against_block_in_list(pos, blocks, block_count, direction, portal_offsets);
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

Pixel_t get_check_pixel_against_centroid(Pixel_t block_pixel, Direction_t direction, S8 rotations_between)
{
     Pixel_t check_pixel = {-1, -1};

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y + TILE_SIZE_IN_PIXELS;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y - 1;
          }
          break;
     case DIRECTION_UP:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x + TILE_SIZE_IN_PIXELS;
               check_pixel.y = block_pixel.y + TILE_SIZE_IN_PIXELS;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y + TILE_SIZE_IN_PIXELS;
          }
          break;
     case DIRECTION_RIGHT:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x + TILE_SIZE_IN_PIXELS;
               check_pixel.y = block_pixel.y - 1;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x + TILE_SIZE_IN_PIXELS;
               check_pixel.y = block_pixel.y + TILE_SIZE_IN_PIXELS;
          }
          break;
     case DIRECTION_DOWN:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y - 1;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x + TILE_SIZE_IN_PIXELS;
               check_pixel.y = block_pixel.y - 1;
          }
          break;
     }

     return check_pixel;
}

Block_t* check_portal_for_centroid_with_block(PortalExit_t* portal_exits, Coord_t portal_coord, Direction_t portal_direction,
                                              Direction_t block_move_dir, Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                              ObjectArray_t<Block_t>* blocks_array){
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

     auto portal_coord_center_pixel = coord_to_pixel_at_center(portal_coord);

     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          auto portal_exit = portal_exits->directions[d];

          for(S8 p = 0; p < portal_exit.count; p++){
               if(portal_exit.coords[p] == portal_coord) continue;

               auto portal_dst_coord = portal_exit.coords[p] + direction_opposite((Direction_t)(d));
               auto portal_dst_center_pixel = coord_to_pixel_at_center(portal_dst_coord);

               auto rect = rect_surrounding_adjacent_coords(portal_exit.coords[p]);

               quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

               for(S16 i = 0; i < block_count; i++){
                    Block_t* check_block = blocks[i];
                    if(!blocks_are_entangled(block, check_block, blocks_array)) continue;

                    auto check_block_center = block_get_center(check_block);
                    auto check_block_portal_offset = check_block_center.pixel - portal_dst_center_pixel;

                    auto portal_rotations = portal_rotations_between(portal_direction, (Direction_t)(d));

                    S8 rotations_between = blocks_rotations_between(block, check_block);
                    rotations_between += portal_rotations;
                    rotations_between %= DIRECTION_COUNT;

                    check_block_portal_offset = pixel_rotate_quadrants_counter_clockwise(check_block_portal_offset, portal_rotations);

                    auto check_block_pixel = portal_coord_center_pixel + check_block_portal_offset - HALF_TILE_SIZE_PIXEL;
                    auto check_block_rect = block_get_rect(check_block_pixel);
                    auto check_pixel = get_check_pixel_against_centroid(block->pos.pixel, block_move_dir, rotations_between);

                    if(pixel_in_rect(check_pixel, check_block_rect)) return check_block;
               }
          }
     }

     return nullptr;
}

Block_t* rotated_entangled_blocks_against_centroid(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                   ObjectArray_t<Block_t>* blocks_array,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap){
     auto block_center = block_get_center(block);
     Rect_t rect = rect_to_check_surrounding_blocks(block_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     // TODO: compress this with below code that deals with portals
     for(S16 i = 0; i < block_count; i++){
          Block_t* check_block = blocks[i];
          if(!blocks_are_entangled(block, check_block, blocks_array)) continue;

          auto check_block_rect = block_get_rect(check_block);
          S8 rotations_between = blocks_rotations_between(block, check_block);

          auto check_pixel = get_check_pixel_against_centroid(block->pos.pixel, direction, rotations_between);

          if(pixel_in_rect(check_pixel, check_block_rect)) return check_block;
     }

     // check through portals
     auto portal_coord = block_get_coord(block) + direction;

     PortalExit_t portal_exits = find_portal_exits(portal_coord, tilemap, interactive_qt);
     auto* check_block = check_portal_for_centroid_with_block(&portal_exits, portal_coord, direction, direction, block, block_qt, blocks_array);
     if(check_block) return check_block;

     // check adjacent coord for portals from different directions
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          if(d == direction_opposite(direction)) continue;

          auto adj_portal_coord = portal_coord + (Direction_t)(d);

          portal_exits = find_portal_exits(adj_portal_coord, tilemap, interactive_qt);
          check_block = check_portal_for_centroid_with_block(&portal_exits, adj_portal_coord, (Direction_t)(d), direction, block, block_qt, blocks_array);
          if(check_block) return check_block;
     }

     return nullptr;
}

Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check->pos, block_to_check->pos_delta, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Interactive_t* interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP &&
             interactive->popup.lift.ticks - 1 <= block_to_check->pos.z){
          }else{
               return interactive;
          }
     }

     tile_coord = pixel_to_coord(pixel_b);
     interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP &&
             interactive->popup.lift.ticks - 1 <= block_to_check->pos.z){
          }else{
               return interactive;
          }
     }

     return nullptr;
}

bool blocks_are_entangled(Block_t* a, Block_t* b, ObjectArray_t<Block_t>* blocks_array)
{
     S16 a_index = a - blocks_array->elements;
     S16 entangle_index = a->entangle_index;
     while(entangle_index != a_index && entangle_index >= 0){
          Block_t* entangled_block = blocks_array->elements + entangle_index;
          if(entangled_block == b) return true;
          entangle_index = entangled_block->entangle_index;
     }
     return false;
}

bool blocks_are_entangled(S16 a_index, S16 b_index, ObjectArray_t<Block_t>* blocks_array){
     if(a_index >= blocks_array->count || b_index >= blocks_array->count) return false;

     Block_t* a = blocks_array->elements + a_index;
     Block_t* b = blocks_array->elements + b_index;
     return blocks_are_entangled(a, b, blocks_array);
}

Block_t* block_inside_block_list(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta,
                                 S16 block_to_check_index, bool block_to_check_cloning,
                                 Block_t** blocks, S16 block_count, ObjectArray_t<Block_t>* blocks_array,
                                 Position_t* collided_with, U8 portal_rotations, Pixel_t* portal_offsets){
     auto final_block_to_check_pos = block_to_check_pos + block_to_check_pos_delta;

     Quad_t quad = {0, 0, BLOCK_SOLID_SIZE, BLOCK_SOLID_SIZE};

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];

          // don't collide with blocks that are cloning with our block to check
          if(block_to_check_cloning){
               Block_t* block_to_check = blocks_array->elements + block_to_check_index;
               if(blocks_are_entangled(block_to_check, block, blocks_array) && block->clone_start.x != 0) continue;
          }

          if(block_to_check_index == (block - blocks_array->elements)) continue;
          if(!blocks_at_collidable_height(final_block_to_check_pos.z, block->pos.z)) continue;

          auto final_block_pos = block->pos;
          final_block_pos.pixel += portal_offsets[i];

          // account for portal rotations in decimal portals of position
          final_block_pos.decimal = vec_rotate_quadrants_counter_clockwise(final_block_pos.decimal, portal_rotations);
          final_block_pos += vec_rotate_quadrants_counter_clockwise(block->pos_delta, portal_rotations);

          auto pos_diff = final_block_pos - final_block_to_check_pos;
          auto check_vec = pos_to_vec(pos_diff);

          Quad_t quad_to_check = {check_vec.x, check_vec.y, check_vec.x + BLOCK_SOLID_SIZE, check_vec.y + BLOCK_SOLID_SIZE};

          if(quad_in_quad(&quad, &quad_to_check)){
               *collided_with = final_block_pos;
               collided_with->pixel += HALF_TILE_SIZE_PIXEL;
               return block;
          }
     }

     return nullptr;
}

BlockInsideResult_t block_inside_another_block(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta, S16 block_to_check_index,
                                               bool block_to_check_cloning, QuadTreeNode_t<Block_t>* block_qt,
                                               QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                               ObjectArray_t<Block_t>* block_array){
     BlockInsideResult_t result = {};

     auto block_to_check_center_pixel = block_center_pixel(block_to_check_pos);

     // TODO: need more complicated function to detect this
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center_pixel);
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     Block_t* collided_block = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                       block_to_check_index,
                                                       block_to_check_cloning, blocks, block_count,
                                                       block_array, &result.collision_pos, 0, portal_offsets);
     if(collided_block){
          result.block = collided_block;
          return result;
     }

     // find portals around the block to check
     auto block_coord = pixel_to_coord(block_to_check_center_pixel);
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

                              U8 portal_rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));

                              search_portal_destination_for_blocks(block_qt, interactive->portal.face, (Direction_t)(d), src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              collided_block = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                                       block_to_check_index,
                                                                       block_to_check_cloning, blocks, block_count,
                                                                       block_array, &result.collision_pos,
                                                                       portal_rotations, portal_offsets);
                              if(collided_block){
                                   result.block = collided_block;
                                   result.portal_rotations = portal_rotations;
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

     if(!block_adjacent_pixels_to_check(block_to_check->pos, block_to_check->pos_delta, direction, &pixel_a, &pixel_b)){
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

Player_t* block_against_player(Block_t* block, Direction_t direction, ObjectArray_t<Player_t>* players){
     auto block_rect = block_get_rect(block);

     // extend the block rect to give the player some room
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          block_rect.left -= HALF_TILE_SIZE_IN_PIXELS;
          break;
     case DIRECTION_UP:
          block_rect.top += HALF_TILE_SIZE_IN_PIXELS;
          break;
     case DIRECTION_RIGHT:
          block_rect.right += HALF_TILE_SIZE_IN_PIXELS;
          break;
     case DIRECTION_DOWN:
          block_rect.bottom -= HALF_TILE_SIZE_IN_PIXELS;
          break;
     }

     for(S16 i = 0; i < players->count; i++){
          auto* player = players->elements + i;
          if(!block_in_height_range_of_player(block, player->pos)) continue;
          if(pixel_in_rect(player->pos.pixel, block_rect)){
               return player;
          }
     }

     return nullptr;
}

static Block_t* block_at_height_in_block_rect(Position_t block_to_check_pos, QuadTreeNode_t<Block_t>* block_qt,
                                              S8 expected_height, QuadTreeNode_t<Interactive_t>* interactive_qt,
                                              TileMap_t* tilemap, S16 min_area = 0){
     // TODO: need more complicated function to detect this
     auto block_to_check_center = block_get_center(block_to_check_pos);
     Rect_t rect = block_get_rect(block_to_check_pos.pixel);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          if(block->pos.z != expected_height) continue;
          auto block_pos = block->pos + block->pos_delta;

          if(pixel_in_rect(block_pos.pixel, rect) ||
             pixel_in_rect(block_top_left_pixel(block_pos.pixel), rect) ||
             pixel_in_rect(block_top_right_pixel(block_pos.pixel), rect) ||
             pixel_in_rect(block_bottom_right_pixel(block_pos.pixel), rect)){
               auto intserection_area = rect_intersecting_area(block_get_rect(block_pos.pixel), rect);
               if(intserection_area >= min_area){
                    return block;
               }
          }
     }

     auto block_to_check_coord = pos_to_coord(block_to_check_center);

     // TODO: compress this logic with move_player_through_world()
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          Coord_t check_coord = block_to_check_coord + (Direction_t)(d);
          auto portal_src_pixel = coord_to_pixel_at_center(check_coord);
          auto interactive = quad_tree_interactive_find_at(interactive_qt, check_coord);

          if(is_active_portal(interactive)){
               PortalExit_t portal_exits = find_portal_exits(check_coord, tilemap, interactive_qt);

               for(S8 pd = 0; pd < DIRECTION_COUNT; pd++){
                    auto portal_exit = portal_exits.directions + pd;

                    for(S8 p = 0; p < portal_exit->count; p++){
                         auto portal_dst_coord = portal_exit->coords[p];
                         if(portal_dst_coord == check_coord) continue;

                         portal_dst_coord += direction_opposite((Direction_t)(pd));

                         auto check_portal_rect = rect_surrounding_adjacent_coords(portal_dst_coord);
                         quad_tree_find_in(block_qt, check_portal_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                         auto portal_dst_center_pixel = coord_to_pixel_at_center(portal_dst_coord);

                         for(S8 b = 0; b < block_count; b++){
                              Block_t* block = blocks[b];

                              if(block->pos.z != expected_height) continue;

                              auto portal_rotations = direction_rotations_between(interactive->portal.face, direction_opposite((Direction_t)(pd)));

                              auto block_portal_dst_offset = block->pos + block->pos_delta;
                              block_portal_dst_offset.pixel += HALF_TILE_SIZE_PIXEL;
                              block_portal_dst_offset.pixel -= portal_dst_center_pixel;

                              Position_t block_pos;
                              block_pos.pixel = portal_src_pixel;
                              block_pos.pixel += pixel_rotate_quadrants_clockwise(block_portal_dst_offset.pixel, portal_rotations);
                              block_pos.pixel -= HALF_TILE_SIZE_PIXEL;
                              block_pos.decimal = vec_rotate_quadrants_clockwise(block_portal_dst_offset.decimal, portal_rotations);
                              canonicalize(&block_pos);

                              if(pixel_in_rect(block_pos.pixel, rect) ||
                                 pixel_in_rect(block_top_left_pixel(block_pos.pixel), rect) ||
                                 pixel_in_rect(block_top_right_pixel(block_pos.pixel), rect) ||
                                 pixel_in_rect(block_bottom_right_pixel(block_pos.pixel), rect)){
                                   auto intserection_area = rect_intersecting_area(block_get_rect(block_pos.pixel), rect);
                                   if(intserection_area >= min_area){
                                        return block;
                                   }
                              }
                         }
                    }
               }
          }
     }

     return nullptr;
}

Block_t* block_held_up_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                        QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     if(block->teleport){
          return block_at_height_in_block_rect(block->teleport_pos + block->teleport_pos_delta, block_qt,
                                               block->teleport_pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     return block_at_height_in_block_rect(block->pos + block->pos_delta, block_qt,
                                          block->pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

Block_t* block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                          QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     if(block->teleport){
          return block_at_height_in_block_rect(block->teleport_pos + block->teleport_pos_delta, block_qt,
                                               block->teleport_pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     return block_at_height_in_block_rect(block->pos + block->pos_delta, block_qt,
                                          block->pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

bool block_on_ice(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                  QuadTreeNode_t<Block_t>* block_qt){
     auto block_pos = pos + pos_delta;

     Pixel_t pixel_to_check = block_pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
     Coord_t coord_to_check = pixel_to_coord(pixel_to_check);

     if(pos.z == 0){
          if(tilemap_is_iced(tilemap, coord_to_check)) return true;
     }

     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_qt, coord_to_check);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP){
               if(interactive->popup.lift.ticks == (pos.z + 1)){
                    if(interactive->popup.iced){
                        return true;
                    }
               }
          }
     }

     auto rect_to_check = rect_surrounding_adjacent_coords(coord_to_check);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect_to_check, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          auto block = blocks[i];
          auto block_rect = block_get_rect(block);

          if(block->pos.z + HEIGHT_INTERVAL != pos.z) continue;
          if(!pixel_in_rect(pixel_to_check, block_rect)) continue;

          if(block->element == ELEMENT_ICE || block->element == ELEMENT_ONLY_ICED){
              return true;
          }
     }

     return false;
}

bool block_on_air(Position_t pos, Vec_t pos_delta, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     auto block_pos = pos + pos_delta;

     // auto block_rect = block_get_rect(block_pos.pixel);
     auto block_center = block_get_center(block_pos);
     auto coord_to_check = block_get_coord(block_pos);

     if(pos.z == 0) return false; // TODO: if we add pits, check for a pit obv

     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_qt, coord_to_check);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP){
               if(interactive->popup.lift.ticks == (pos.z + 1)){
                    return false;
                    // auto coord_rect = rect_surrounding_coord(coord_to_check);
                    // if(rect_in_rect(block_rect, coord_rect)) return false;
               }
          }
     }

     auto rect_to_check = rect_surrounding_adjacent_coords(coord_to_check);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect_to_check, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          auto block = blocks[i];
          auto check_block_rect = block_get_rect(block);

          if(block->pos.z + HEIGHT_INTERVAL != pos.z) continue;
          if(pixel_in_rect(block_center.pixel, check_block_rect)) return false;
          // if(rect_in_rect(block_rect, check_block_rect)) return false;
     }

     return true;
}

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, S16 block_stop_on_pixel_x, S16 block_stop_on_pixel_y,
                                                                    Move_t block_horizontal_move, Move_t block_vertical_move, S16 block_index,
                                                                    bool block_is_cloning, World_t* world){
     CheckBlockCollisionResult_t result {};

     result.pos_delta = block_pos_delta;
     result.vel = block_vel;
     result.accel = block_accel;

     result.stop_on_pixel_x = block_stop_on_pixel_x;
     result.stop_on_pixel_y = block_stop_on_pixel_y;

     result.horizontal_move = block_horizontal_move;
     result.vertical_move = block_vertical_move;
     result.collided_block_index = -1;

     static const S8 max_attempts = 16;
     S8 attempts = 0;
     for(BlockInsideResult_t block_inside_result = block_inside_another_block(block_pos,
                                                                              result.pos_delta,
                                                                              block_index,
                                                                              block_is_cloning,
                                                                              world->block_qt,
                                                                              world->interactive_qt,
                                                                              &world->tilemap,
                                                                              &world->blocks);
         block_inside_result.block && attempts < max_attempts;
         block_inside_result = block_inside_another_block(block_pos,
                                                          result.pos_delta,
                                                          block_index,
                                                          block_is_cloning,
                                                          world->block_qt,
                                                          world->interactive_qt,
                                                          &world->tilemap,
                                                          &world->blocks), attempts++){
          result.collided = true;
          result.collided_block_index = get_block_index(world, block_inside_result.block);
          result.collided_pos = block_inside_result.collision_pos;
          result.collided_portal_rotations = block_inside_result.portal_rotations;

          auto collided_block_center = block_inside_result.collision_pos;

          auto moved_block_pos = block_get_center(block_pos);
          auto move_direction = move_direction_between(moved_block_pos, block_inside_result.collision_pos);
          Direction_t first_direction;
          Direction_t second_direction;
          move_direction_to_directions(move_direction, &first_direction, &second_direction);

          // check if they are on ice before we adjust the position on our block to check
          bool a_on_ice = block_on_ice(block_pos, result.pos_delta, &world->tilemap, world->interactive_qt, world->block_qt);
          bool b_on_ice = block_on_ice(block_inside_result.block->pos, block_inside_result.block->pos_delta,
                                       &world->tilemap, world->interactive_qt, world->block_qt);

          Vec_t save_vel = result.vel;

          S16 block_inside_index = 0;
          if(block_inside_result.block){
              block_inside_index = get_block_index(world, block_inside_result.block);
          }

          if(blocks_are_entangled(result.collided_block_index, block_index, &world->blocks)){
               auto* block = world->blocks.elements + block_index;
               auto* entangled_block = world->blocks.elements + result.collided_block_index;
               auto pos_diff = pos_to_vec(entangled_block->pos - block->pos);

               // if positions are diagonal to each other and the rotation between them is odd, check if we are moving into each other
               if(fabs(pos_diff.x) == fabs(pos_diff.y) && (block->rotation + entangled_block->rotation) % 2 == 1){
                    // just gtfo if this happens, we handle this case outside this function
                    break;
               }

               // if we are not moving towards the entangled block, it's on them to resolve the collision, so get outta here
               DirectionMask_t pos_diff_dir_mask = vec_direction_mask(pos_diff);
               auto pos_delta_dir = vec_direction(block->pos_delta);
               if(!direction_in_mask(pos_diff_dir_mask, pos_delta_dir)) break;
          }

          if(block_inside_index != block_index){
               auto* block = world->blocks.elements + block_index;
               auto collided_block_move_mask = vec_direction_mask(block_inside_result.block->vel);

               switch(move_direction){
               default:
                    break;
               case MOVE_DIRECTION_LEFT_UP:
               case MOVE_DIRECTION_LEFT_DOWN:
               case MOVE_DIRECTION_RIGHT_UP:
               case MOVE_DIRECTION_RIGHT_DOWN:
               {
                    switch(first_direction){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         result.stop_on_pixel_x = collided_block_center.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                         break;
                    case DIRECTION_RIGHT:
                         result.stop_on_pixel_x = (collided_block_center.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                         break;
                    }

                    switch(second_direction){
                    default:
                         break;
                    case DIRECTION_DOWN:
                         result.stop_on_pixel_y = collided_block_center.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
                         break;
                    case DIRECTION_UP:
                         result.stop_on_pixel_y = (collided_block_center.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                         break;
                    }

                    result.pos_delta.x = 0;
                    result.vel.x = 0.0f;
                    result.accel.x = 0.0f;
                    result.horizontal_move.state = MOVE_STATE_IDLING;

                    result.pos_delta.y = 0;
                    result.vel.y = 0.0f;
                    result.accel.y = 0.0f;
                    result.vertical_move.state = MOVE_STATE_IDLING;
               } break;
               case MOVE_DIRECTION_LEFT:
               {
                    // TODO: compress these cases
                    if(direction_in_mask(collided_block_move_mask, first_direction)){
                         // if the block is moving in the direction we collided, just move right up against it
                         // TODO: set the decimal portion so we are right up against the block
                         // TODO: this case probably means if they are both on ice they want to start moving as a
                         //       group at a speed in the middle of both of their original speeds?
                         block->pos.pixel.x = collided_block_center.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.x = 0;
                         block->vel.x = block_inside_result.block->vel.x;
                         block->pos_delta.x = 0;
                    }else{
                         result.stop_on_pixel_x = collided_block_center.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                         result.pos_delta.x = 0;
                         result.vel.x = 0.0f;
                         result.accel.x = 0.0f;
                         result.horizontal_move.state = MOVE_STATE_IDLING;
                    }
               } break;
               case MOVE_DIRECTION_RIGHT:
               {
                    if(direction_in_mask(collided_block_move_mask, first_direction)){
                         block->pos.pixel.x = (collided_block_center.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.x = 0;
                         block->vel.x = block_inside_result.block->vel.x;
                         block->pos_delta.x = 0;
                    }else{
                         result.stop_on_pixel_x = (collided_block_center.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;

                         result.pos_delta.x = 0;
                         result.vel.x = 0.0f;
                         result.accel.x = 0.0f;
                         result.horizontal_move.state = MOVE_STATE_IDLING;
                    }
               } break;
               case MOVE_DIRECTION_DOWN:
               {
                    if(direction_in_mask(collided_block_move_mask, first_direction)){
                         block->pos.pixel.y = collided_block_center.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.y = 0;
                         block->vel.y = block_inside_result.block->vel.y;
                         block->pos_delta.y = 0;
                    }else{
                         result.stop_on_pixel_y = collided_block_center.pixel.y + HALF_TILE_SIZE_IN_PIXELS;

                         result.pos_delta.y = 0;
                         result.vel.y = 0.0f;
                         result.accel.y = 0.0f;
                         result.vertical_move.state = MOVE_STATE_IDLING;
                    }
               } break;
               case MOVE_DIRECTION_UP:
               {
                    if(direction_in_mask(collided_block_move_mask, first_direction)){
                         block->pos.pixel.y = (collided_block_center.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.y = 0;
                         block->vel.y = block_inside_result.block->vel.y;
                         block->pos_delta.y = 0;
                    }else{
                         result.stop_on_pixel_y = (collided_block_center.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;

                         result.pos_delta.y = 0;
                         result.vel.y = 0.0f;
                         result.accel.y = 0.0f;
                         result.vertical_move.state = MOVE_STATE_IDLING;
                    }
               } break;
               }
          }

          for(S16 i = 0; i < world->players.count; i++){
               Player_t* player = world->players.elements + i;
               if(player->pushing_block == block_index && first_direction == player->face){
                    player->push_time = 0.0f;
               }
          }

          if(a_on_ice && b_on_ice){
               bool push = true;

               if(block_inside_index == block_index){
                    Coord_t block_coord = block_get_coord(block_pos);
                    Direction_t src_portal_dir = direction_between(block_coord, block_inside_result.src_portal_coord);
                    Direction_t dst_portal_dir = direction_between(block_coord, block_inside_result.dst_portal_coord);
                    DirectionMask_t move_mask = vec_direction_mask(result.vel);

                    auto resolve_result = resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_vel,
                                                                              block_accel, DIRECTION_LEFT, DIRECTION_UP);
                    if(resolve_result.push_dir != DIRECTION_COUNT){
                         first_direction = resolve_result.push_dir;
                         resolve_result.vel = resolve_result.vel;
                         resolve_result.accel = resolve_result.accel;
                    }

                    resolve_result = resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_vel,
                                                                         block_accel, DIRECTION_LEFT, DIRECTION_DOWN);

                    if(resolve_result.push_dir != DIRECTION_COUNT){
                         first_direction = resolve_result.push_dir;
                         resolve_result.vel = resolve_result.vel;
                         resolve_result.accel = resolve_result.accel;
                    }

                    resolve_result = resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_vel,
                                                                         block_accel, DIRECTION_RIGHT, DIRECTION_UP);

                    if(resolve_result.push_dir != DIRECTION_COUNT){
                         first_direction = resolve_result.push_dir;
                         resolve_result.vel = resolve_result.vel;
                         resolve_result.accel = resolve_result.accel;
                    }

                    resolve_result = resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_vel,
                                                                         block_accel, DIRECTION_RIGHT, DIRECTION_DOWN);

                    if(resolve_result.push_dir != DIRECTION_COUNT){
                         first_direction = resolve_result.push_dir;
                         resolve_result.vel = resolve_result.vel;
                         resolve_result.accel = resolve_result.accel;
                    }
               }else{
                    if(!direction_in_mask(vec_direction_mask(block_pos_delta), first_direction)){
                         // although we collided, the other block is colliding into us, so let that block resolve this mess
                         result.collided = false;
                         break;
                    }

                    first_direction = direction_rotate_clockwise(first_direction, block_inside_result.portal_rotations);
                    if(second_direction != DIRECTION_COUNT){
                         second_direction = direction_rotate_clockwise(second_direction, block_inside_result.portal_rotations);
                    }

                    // blocks heading towards each other will stop
                    switch(first_direction){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         if(block_inside_result.block->vel.x > 0){
                              block_inside_result.block->stop_on_pixel_x = closest_pixel(block_inside_result.block->pos.pixel.x, block_inside_result.block->pos.decimal.x);
                              block_inside_result.block->accel.x = 0.0f;
                              block_inside_result.block->vel.x = 0.0f;
                              block_inside_result.block->horizontal_move.state = MOVE_STATE_IDLING;
                              block_inside_result.block->horizontal_move.sign = MOVE_SIGN_ZERO;
                              push = false;
                         }
                         break;
                    case DIRECTION_RIGHT:
                         if(block_inside_result.block->vel.x < 0){
                              block_inside_result.block->stop_on_pixel_x = closest_pixel(block_inside_result.block->pos.pixel.x, block_inside_result.block->pos.decimal.x);
                              block_inside_result.block->accel.x = 0.0f;
                              block_inside_result.block->vel.x = 0.0f;
                              block_inside_result.block->horizontal_move.state = MOVE_STATE_IDLING;
                              block_inside_result.block->horizontal_move.sign = MOVE_SIGN_ZERO;
                              push = false;
                         }
                         break;
                    case DIRECTION_DOWN:
                         if(block_inside_result.block->vel.y > 0){
                              block_inside_result.block->stop_on_pixel_y = closest_pixel(block_inside_result.block->pos.pixel.y, block_inside_result.block->pos.decimal.y);
                              block_inside_result.block->accel.y = 0.0f;
                              block_inside_result.block->vel.y = 0.0f;
                              block_inside_result.block->vertical_move.state = MOVE_STATE_IDLING;
                              block_inside_result.block->vertical_move.sign = MOVE_SIGN_ZERO;
                              push = false;
                         }
                         break;
                    case DIRECTION_UP:
                         if(block_inside_result.block->vel.y < 0){
                              block_inside_result.block->stop_on_pixel_y = closest_pixel(block_inside_result.block->pos.pixel.y, block_inside_result.block->pos.decimal.y);
                              block_inside_result.block->accel.y = 0.0f;
                              block_inside_result.block->vel.y = 0.0f;
                              block_inside_result.block->vertical_move.state = MOVE_STATE_IDLING;
                              block_inside_result.block->vertical_move.sign = MOVE_SIGN_ZERO;
                              push = false;
                         }
                         break;
                    }
               }

               if(push){
                    F32 instant_vel = 0;

                    // take into account direction and portal rotations before setting the instant vel
                    if(direction_is_horizontal(first_direction)){
                         if((block_inside_result.portal_rotations % 2)){
                              instant_vel = save_vel.y;
                         }else{
                              instant_vel = save_vel.x;
                         }
                    }else{
                         if((block_inside_result.portal_rotations % 2)){
                              instant_vel = save_vel.x;
                         }else{
                              instant_vel = save_vel.y;
                         }
                    }

                    if(block_push(block_inside_result.block, first_direction, world, true, instant_vel)){
                         push_entangled_block(block_inside_result.block, world, first_direction, true, instant_vel);
                         if(blocks_are_entangled(block_inside_result.block - world->blocks.elements, block_index, &world->blocks)){
                              Block_t* block = world->blocks.elements + block_index;
                              auto rotations_between = direction_rotations_between(static_cast<Direction_t>(block_inside_result.block->rotation), static_cast<Direction_t>(block->rotation));
                              if(rotations_between % 2 == 0){
                                   if(direction_is_horizontal(first_direction)){
                                        result.vel.x = block->vel.x;
                                        result.horizontal_move = block->horizontal_move;
                                   }else{
                                        result.vel.y = block->vel.y;
                                        result.vertical_move = block->vertical_move;
                                   }
                              }else{
                                   if(direction_is_horizontal(first_direction)){
                                        result.vel.y = block->vel.y;
                                        result.vertical_move = block->vertical_move;
                                   }else{
                                        result.vel.x = block->vel.x;
                                        result.horizontal_move = block->horizontal_move;
                                   }
                              }
                         }
                    }

                    if(second_direction != DIRECTION_COUNT){
                         instant_vel = direction_is_horizontal(second_direction) ? save_vel.x : save_vel.y;
                         if(block_push(block_inside_result.block, second_direction, world, true, instant_vel)){
                              push_entangled_block(block_inside_result.block, world, second_direction, true, instant_vel);
                              if(blocks_are_entangled(block_inside_result.block - world->blocks.elements, block_index, &world->blocks)){
                                   Block_t* block = world->blocks.elements + block_index;
                                   auto rotations_between = direction_rotations_between(static_cast<Direction_t>(block_inside_result.block->rotation), static_cast<Direction_t>(block->rotation));
                                   if(rotations_between % 2 == 0){
                                        if(direction_is_horizontal(second_direction)){
                                             result.vel.x = block->vel.x;
                                             result.horizontal_move = block->horizontal_move;
                                        }else{
                                             result.vel.y = block->vel.y;
                                             result.vertical_move = block->vertical_move;
                                        }
                                   }else{
                                        if(direction_is_horizontal(second_direction)){
                                             result.vel.y = block->vel.y;
                                             result.vertical_move = block->vertical_move;
                                        }else{
                                             result.vel.x = block->vel.x;
                                             result.horizontal_move = block->horizontal_move;
                                        }
                                   }
                              }
                         }
                    }
               }
          }

          // TODO: there is no way this is the right way to do this
          if(block_inside_index == block_index) break;
     }

     return result;
}

BlockCollidesWithItselfResult_t resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                                                    Vec_t block_vel, Vec_t block_accel, Direction_t check_horizontal, Direction_t check_vertical){
     BlockCollidesWithItselfResult_t result {};
     result.push_dir = DIRECTION_COUNT;
     result.vel = block_vel;
     result.accel = block_accel;

     if(directions_meet_expectations(src_portal_dir, dst_portal_dir, check_horizontal, check_vertical)){
          if(move_mask & direction_to_direction_mask(check_vertical)){
               result.push_dir = direction_opposite(check_horizontal);
               result.vel.y = 0;
               result.accel.y = 0;
          }else if(move_mask & direction_to_direction_mask(check_horizontal)){
               result.push_dir = direction_opposite(check_vertical);
               result.vel.x = 0;
               result.accel.x = 0;
          }
     }

     return result;
}

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_qt, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Pixel_t* offsets){
     U8 rotations_between_portals = portal_rotations_between(dst_portal_face, src_portal_face);
     Coord_t dst_coord = dst_portal_coord + direction_opposite(dst_portal_face);
     Pixel_t src_portal_center_pixel = coord_to_pixel_at_center(src_portal_coord);
     Pixel_t dst_center_pixel = coord_to_pixel_at_center(dst_coord);
     Rect_t rect = rect_surrounding_adjacent_coords(dst_coord);
     quad_tree_find_in(block_qt, rect, blocks, block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S8 o = 0; o < *block_count; o++){
          Pixel_t dst_offset = block_center_pixel(blocks[o]) - dst_center_pixel;
          Pixel_t src_fake_pixel = src_portal_center_pixel + pixel_rotate_quadrants_clockwise(dst_offset, rotations_between_portals);
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

void push_entangled_block(Block_t* block, World_t* world, Direction_t push_dir, bool pushed_by_ice, F32 instant_vel){
     if(block->entangle_index < 0) return;

     S16 block_index = block - world->blocks.elements;
     S16 entangle_index = block->entangle_index;
     while(entangle_index != block_index && entangle_index >= 0){
          Block_t* entangled_block = world->blocks.elements + entangle_index;
          auto rotations_between = direction_rotations_between(static_cast<Direction_t>(entangled_block->rotation), static_cast<Direction_t>(block->rotation));
          Direction_t rotated_dir = direction_rotate_clockwise(push_dir, rotations_between);
          block_push(entangled_block, rotated_dir, world, pushed_by_ice, instant_vel);
          entangle_index = entangled_block->entangle_index;
     }
}
