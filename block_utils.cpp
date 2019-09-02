#include "block_utils.h"
#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"

#include <float.h>
#include <math.h>
#include <string.h>

void add_block_held(BlockHeldResult_t* result, Block_t* block, Rect_t rect){
     if(result->count < MAX_HELD_BLOCKS){
          result->blocks_held[result->count].block = block;
          result->blocks_held[result->count].rect = rect;
          result->count++;
     }
}

void add_interactive_held(InteractiveHeldResult_t* result, Interactive_t* interactive, Rect_t rect){
     if(result->count < MAX_HELD_INTERACTIVES){
          result->interactives_held[result->count].interactive = interactive;
          result->interactives_held[result->count].rect = rect;
          result->count++;
     }
}

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

Block_t* block_against_block_in_list(Position_t pos, Block_t** blocks, S16 block_count, Direction_t direction, Position_t* portal_offsets){
     // intentionally use the original pos without pos_delta
     // TODO: account for block width/height
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* adjacent_block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, adjacent_block->pos.z)) continue;

               auto adjacent_pos = adjacent_block->teleport ?
                                   adjacent_block->teleport_pos + adjacent_block->teleport_pos_delta
                                   : adjacent_block->pos + adjacent_block->pos_delta;
               adjacent_pos += portal_offsets[i];

               auto boundary_check_pos = adjacent_pos + Pixel_t{TILE_SIZE_IN_PIXELS, 0};
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - pos).x);

               auto bottom_boundary_pos = pos + Pixel_t{0, -BLOCK_SOLID_SIZE_IN_PIXELS};
               auto top_boundary_pos = pos + Pixel_t{0, TILE_SIZE_IN_PIXELS};

               // check if they are on the expected boundary
               if(distance_to_boundary <= FLT_EPSILON &&
                  position_y_greater_than(adjacent_pos, bottom_boundary_pos) &&
                  position_y_less_than(adjacent_pos, top_boundary_pos)){
                    return adjacent_block;
               }
          }
          break;
     case DIRECTION_RIGHT:
          for(S16 i = 0; i < block_count; i++){
               Block_t* adjacent_block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, adjacent_block->pos.z)) continue;

               auto adjacent_pos = adjacent_block->teleport ?
                                   adjacent_block->teleport_pos + adjacent_block->teleport_pos_delta
                                   : adjacent_block->pos + adjacent_block->pos_delta;
               adjacent_pos += portal_offsets[i];

               auto boundary_check_pos = adjacent_pos;
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - (pos + Pixel_t{TILE_SIZE_IN_PIXELS, 0})).x);

               auto bottom_boundary_pos = pos + Pixel_t{0, -BLOCK_SOLID_SIZE_IN_PIXELS};
               auto top_boundary_pos = pos + Pixel_t{0, TILE_SIZE_IN_PIXELS};

               // check if they are on the expected boundary
               if(distance_to_boundary <= FLT_EPSILON &&
                  position_y_greater_than(adjacent_pos, bottom_boundary_pos) &&
                  position_y_less_than(adjacent_pos, top_boundary_pos)){
                    return adjacent_block;
               }
          }
          break;
     case DIRECTION_DOWN:
          for(S16 i = 0; i < block_count; i++){
               Block_t* adjacent_block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, adjacent_block->pos.z)) continue;

               auto adjacent_pos = adjacent_block->teleport ?
                                   adjacent_block->teleport_pos + adjacent_block->teleport_pos_delta
                                   : adjacent_block->pos + adjacent_block->pos_delta;
               adjacent_pos += portal_offsets[i];

               auto boundary_check_pos = adjacent_pos + Pixel_t{0, TILE_SIZE_IN_PIXELS};
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - pos).y);

               auto left_boundary_pos = pos + Pixel_t{-BLOCK_SOLID_SIZE_IN_PIXELS, 0};
               auto right_boundary_pos = pos + Pixel_t{TILE_SIZE_IN_PIXELS, 0};

               // check if they are on the expected boundary
               if(distance_to_boundary <= FLT_EPSILON &&
                  position_x_greater_than(adjacent_pos, left_boundary_pos) &&
                  position_x_less_than(adjacent_pos, right_boundary_pos)){
                    return adjacent_block;
               }
          }
          break;
     case DIRECTION_UP:
          for(S16 i = 0; i < block_count; i++){
               Block_t* adjacent_block = blocks[i];
               if(!blocks_at_collidable_height(pos.z, adjacent_block->pos.z)) continue;

               auto adjacent_pos = adjacent_block->teleport ?
                                   adjacent_block->teleport_pos + adjacent_block->teleport_pos_delta
                                   : adjacent_block->pos + adjacent_block->pos_delta;
               adjacent_pos += portal_offsets[i];

               auto boundary_check_pos = adjacent_pos;
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - (pos + Pixel_t{0, TILE_SIZE_IN_PIXELS})).y);

               auto left_boundary_pos = pos + Pixel_t{-BLOCK_SOLID_SIZE_IN_PIXELS, 0};
               auto right_boundary_pos = pos + Pixel_t{TILE_SIZE_IN_PIXELS, 0};

               // check if they are on the expected boundary
               if(distance_to_boundary <= FLT_EPSILON &&
                  position_x_greater_than(adjacent_pos, left_boundary_pos) &&
                  position_x_less_than(adjacent_pos, right_boundary_pos)){
                    return adjacent_block;
               }
          }
          break;
     }

     return nullptr;
}

BlockAgainstOthersResult_t block_against_other_blocks(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                      QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap){
     BlockAgainstOthersResult_t result;

     auto block_center = block_get_center(pos);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center.pixel);

     Position_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          // lol at me misusing this function, but watevs
          if(block_against_block_in_list(pos, blocks + i, 1, direction, portal_offsets)){
               BlockAgainstOther_t against_other;
               against_other.block = blocks[i];
               result.add(against_other);
          }
     }

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
                         Direction_t new_dir = (Direction_t)(d);
                         for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                              auto dst_coord = portal_exits.directions[d].coords[p];
                              if(dst_coord == src_coord) continue;

                              search_portal_destination_for_blocks(block_qt, interactive->portal.face, new_dir, src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              for(S16 b = 0; b < block_count; b++){
                                   // lol at me misusing this function, but watevs
                                   if(block_against_block_in_list(pos, blocks + b, 1, direction, portal_offsets + b)){
                                        BlockAgainstOther_t against_other;
                                        against_other.block = blocks[b];
                                        against_other.rotations_through_portal = portal_rotations_between(interactive->portal.face, new_dir);
                                        result.add(against_other);
                                   }
                              }
                         }
                    }
               }
          }
     }

     return result;
}

Block_t* block_against_another_block(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir){
     auto block_center = block_get_center(pos);
     Rect_t rect = rect_to_check_surrounding_blocks(block_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Position_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
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
     Interactive_t* interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord, block_to_check->pos.z);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP &&
             interactive->popup.lift.ticks - 1 <= block_to_check->pos.z){
          }else{
               return interactive;
          }
     }

     tile_coord = pixel_to_coord(pixel_b);
     interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, tile_coord, block_to_check->pos.z);
     if(interactive) return interactive;

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

struct BlockInsideBlockInListResult_t{
     Block_t* block = nullptr;
     Position_t collided_pos;
};

#define MAX_BLOCK_INSIDE_BLOCK_COUNT 16

struct BlockInsideBlockListResult_t{
     BlockInsideBlockInListResult_t entries[MAX_BLOCK_INSIDE_BLOCK_COUNT];
     S8 count = 0;

     bool add(Block_t* block, Position_t collided_pos){
          if(count >= MAX_BLOCK_INSIDE_BLOCK_COUNT) return false;
          entries[count].block = block;
          entries[count].collided_pos = collided_pos;
          count++;
          return true;
     }
};

BlockInsideBlockListResult_t block_inside_block_list(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta,
                                                     S16 block_to_check_index, bool block_to_check_cloning,
                                                     Block_t** blocks, S16 block_count, ObjectArray_t<Block_t>* blocks_array,
                                                     U8 portal_rotations, Position_t* portal_offsets){
     (void)(portal_rotations);
     BlockInsideBlockListResult_t result;

     auto final_block_to_check_pos = block_to_check_pos + block_to_check_pos_delta;

     Quad_t quad = {0, 0, TILE_SIZE, TILE_SIZE};

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];

          // don't collide with blocks that are cloning with our block to check
          if(block_to_check_cloning){
               Block_t* block_to_check = blocks_array->elements + block_to_check_index;
               if(blocks_are_entangled(block_to_check, block, blocks_array) && block->clone_start.x != 0) continue;
          }

          if(!blocks_at_collidable_height(final_block_to_check_pos.z, block->pos.z)) continue;

          auto final_block_pos = block->pos + block->pos_delta;
          final_block_pos += portal_offsets[i];

          auto pos_diff = final_block_pos - final_block_to_check_pos;
          auto check_vec = pos_to_vec(pos_diff);

          Quad_t quad_to_check = {check_vec.x, check_vec.y, check_vec.x + TILE_SIZE, check_vec.y + TILE_SIZE};

          if(block_to_check_index == (block - blocks_array->elements)){
               // if, after applying the portal offsets it is the same quad to check and it is the same index,
               // ignore it because the block is literally just going through a portal, nbd
               if(quad == quad_to_check) continue;
          }

          if(quad_in_quad_high_range_exclusive(&quad, &quad_to_check)){
               final_block_pos.pixel += HALF_TILE_SIZE_PIXEL;
               result.add(block, final_block_pos);
          }
     }

     return result;
}

Block_t* pixel_inside_block(Pixel_t pixel, S8 z, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     (void)(tilemap);
     (void)(interactive_qt);
     Rect_t search_rect;

     search_rect.left = pixel.x - HALF_TILE_SIZE_IN_PIXELS;
     search_rect.right = pixel.x + HALF_TILE_SIZE_IN_PIXELS;
     search_rect.bottom = pixel.y - HALF_TILE_SIZE_IN_PIXELS;
     search_rect.top = pixel.y + HALF_TILE_SIZE_IN_PIXELS;

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, search_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S8 i = 0; i < block_count; i++){
          if(blocks_at_collidable_height(z, blocks[i]->pos.z) && pixel_in_rect(pixel, block_get_rect(blocks[i]))){
               return blocks[i];
          }
     }

     // TODO: handle portal logic in the same way

     return nullptr;
}

BlockInsideOthersResult_t block_inside_others(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta, S16 block_to_check_index,
                                              bool block_to_check_cloning, QuadTreeNode_t<Block_t>* block_qt,
                                              QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                              ObjectArray_t<Block_t>* block_array){
     BlockInsideOthersResult_t result = {};

     auto block_to_check_center_pixel = block_center_pixel(block_to_check_pos);

     // TODO: need more complicated function to detect this
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center_pixel);
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Position_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     auto inside_list_result = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                       block_to_check_index,
                                                       block_to_check_cloning, blocks, block_count,
                                                       block_array, 0, portal_offsets);
     for(S8 i = 0; i < inside_list_result.count; i++){
          result.add(inside_list_result.entries[i].block, inside_list_result.entries[i].collided_pos, 0, Coord_t{-1, -1}, Coord_t{-1, -1});
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

                              inside_list_result = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                                           block_to_check_index, block_to_check_cloning,
                                                                           blocks, block_count, block_array, portal_rotations,
                                                                           portal_offsets);

                              for(S8 i = 0; i < inside_list_result.count; i++){
                                   if(block_to_check_index == inside_list_result.entries[i].block - block_array->elements &&
                                      direction_in_mask(vec_direction_mask(block_to_check_pos_delta), (Direction_t)(d))){
                                        // if we collide with ourselves through a portal, then ignore certain cases
                                        // that we end up inside, because while they are true, they are not what we
                                        // have decided should happen
                                   }else{
                                        result.add(inside_list_result.entries[i].block, inside_list_result.entries[i].collided_pos,
                                                   portal_rotations, src_coord, dst_coord);
                                   }
                              }
                         }
                    }
               }
          }
     }

     return result;
}

Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap){
     Pixel_t pixel_a {};
     Pixel_t pixel_b {};

     if(!block_adjacent_pixels_to_check(block_to_check->pos, block_to_check->pos_delta, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);

     Tile_t* tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

     tile_coord = pixel_to_coord(pixel_b);

     tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

     return nullptr;
}

Tile_t* block_against_solid_tile(Position_t block_pos, Vec_t pos_delta, Direction_t direction, TileMap_t* tilemap){
     Pixel_t pixel_a {};
     Pixel_t pixel_b {};

     if(!block_adjacent_pixels_to_check(block_pos, pos_delta, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);

     Tile_t* tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

     tile_coord = pixel_to_coord(pixel_b);

     tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

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

InteractiveHeldResult_t block_held_up_by_popup(Position_t block_pos, QuadTreeNode_t<Interactive_t>* interactive_qt, S16 min_area){
     InteractiveHeldResult_t result;
     auto block_rect = block_get_rect(block_pos.pixel);
     Coord_t rect_coords[4];
     get_rect_coords(block_rect, rect_coords);
     for(S8 i = 0; i < 4; i++){
          auto* interactive = quad_tree_interactive_find_at(interactive_qt, rect_coords[i]);
          if(interactive && interactive->type == INTERACTIVE_TYPE_POPUP){
               if(block_pos.z == (interactive->popup.lift.ticks - 1)){
                    auto popup_rect = block_get_rect(coord_to_pixel(rect_coords[i]));
                    auto intserection_area = rect_intersecting_area(block_rect, popup_rect);
                    if(intserection_area >= min_area){
                         add_interactive_held(&result, interactive, popup_rect);
                    }
               }
          }
     }

     return result;
}

static BlockHeldResult_t block_at_height_in_block_rect(Pixel_t block_to_check_pixel, QuadTreeNode_t<Block_t>* block_qt,
                                                       S8 expected_height, QuadTreeNode_t<Interactive_t>* interactive_qt,
                                                       TileMap_t* tilemap, S16 min_area = 0){
     BlockHeldResult_t result;

     // TODO: need more complicated function to detect this
     auto block_to_check_center = block_center_pixel(block_to_check_pixel);
     Rect_t check_rect = block_get_rect(block_to_check_pixel);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          if(block->pos.z != expected_height) continue;
          auto block_pos = block->pos + block->pos_delta;

          if(pixel_in_rect(block_pos.pixel, check_rect) ||
             pixel_in_rect(block_top_left_pixel(block_pos.pixel), check_rect) ||
             pixel_in_rect(block_top_right_pixel(block_pos.pixel), check_rect) ||
             pixel_in_rect(block_bottom_right_pixel(block_pos.pixel), check_rect)){
               auto block_rect = block_get_rect(block_pos.pixel);
               auto intserection_area = rect_intersecting_area(block_rect, check_rect);
               if(intserection_area >= min_area){
                    add_block_held(&result, block, block_rect);
               }
          }
     }

     auto block_to_check_coord = pixel_to_coord(block_to_check_center);

     Coord_t surrounding_coords[SURROUNDING_COORD_COUNT];
     coords_surrounding(surrounding_coords, SURROUNDING_COORD_COUNT, block_to_check_coord);

     // TODO: compress this logic with move_player_through_world()
     for(S8 d = 0; d < SURROUNDING_COORD_COUNT; d++){
          Coord_t check_coord = surrounding_coords[d];
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

                              if(pixel_in_rect(block_pos.pixel, check_rect) ||
                                 pixel_in_rect(block_top_left_pixel(block_pos.pixel), check_rect) ||
                                 pixel_in_rect(block_top_right_pixel(block_pos.pixel), check_rect) ||
                                 pixel_in_rect(block_bottom_right_pixel(block_pos.pixel), check_rect)){
                                   auto block_rect = block_get_rect(block_pos.pixel);
                                   auto intserection_area = rect_intersecting_area(block_rect, check_rect);
                                   if(intserection_area >= min_area){
                                        add_block_held(&result, block, block_rect);
                                   }
                              }
                         }
                    }
               }
          }
     }

     return result;
}

BlockHeldResult_t block_held_up_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                        QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     if(block->teleport){
          auto final_pos = block->teleport_pos + block->teleport_pos_delta;
          final_pos.pixel.x = passes_over_pixel(block->teleport_pos.pixel.x, final_pos.pixel.x);
          final_pos.pixel.y = passes_over_pixel(block->teleport_pos.pixel.y, final_pos.pixel.y);
          return block_at_height_in_block_rect(final_pos.pixel, block_qt,
                                               block->teleport_pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     auto final_pos = block->pos + block->pos_delta;
     final_pos.pixel.x = passes_over_pixel(block->pos.pixel.x, final_pos.pixel.x);
     final_pos.pixel.y = passes_over_pixel(block->pos.pixel.y, final_pos.pixel.y);
     return block_at_height_in_block_rect(final_pos.pixel, block_qt,
                                          block->pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

BlockHeldResult_t block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     if(block->teleport){
          auto final_pos = block->teleport_pos + block->teleport_pos_delta;
          final_pos.pixel.x = passes_over_pixel(block->teleport_pos.pixel.x, final_pos.pixel.x);
          final_pos.pixel.y = passes_over_pixel(block->teleport_pos.pixel.y, final_pos.pixel.y);
          return block_at_height_in_block_rect(final_pos.pixel, block_qt,
                                               block->teleport_pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     auto final_pos = block->pos + block->pos_delta;
     final_pos.pixel.x = passes_over_pixel(block->pos.pixel.x, final_pos.pixel.x);
     final_pos.pixel.y = passes_over_pixel(block->pos.pixel.y, final_pos.pixel.y);
     return block_at_height_in_block_rect(final_pos.pixel, block_qt,
                                          block->pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

BlockHeldResult_t block_held_down_by_another_block(Pixel_t block_pixel, S8 block_z, QuadTreeNode_t<Block_t>* block_qt,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     return block_at_height_in_block_rect(block_pixel, block_qt, block_z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

bool block_on_ice(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                  QuadTreeNode_t<Block_t>* block_qt){
     auto block_pos = pos + pos_delta;

     Pixel_t pixel_to_check = block_get_center(block_pos).pixel;
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

bool block_on_air(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     if(pos.z == 0) return false; // TODO: if we add pits, check for a pit obv

     auto final_pos = pos + pos_delta;
     auto block_center = block_center_pixel(final_pos);
     auto block_result = block_at_height_in_block_rect(final_pos.pixel, block_qt,
                                                       final_pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap);
     for(S16 i = 0; i < block_result.count; i++){
          if(pixel_in_rect(block_center, block_result.blocks_held[i].rect)) return false;
     }

     auto interactive_result = block_held_up_by_popup(final_pos, interactive_qt);
     for(S16 i = 0; i < interactive_result.count; i++){
          if(pixel_in_rect(block_center, interactive_result.interactives_held[i].rect)) return false;
     }

     return true;
}

bool block_on_air(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     return block_on_air(block->pos, block->pos_delta, tilemap, interactive_qt, block_qt);
}

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, S16 block_stop_on_pixel_x, S16 block_stop_on_pixel_y,
                                                                    Move_t block_horizontal_move, Move_t block_vertical_move,
                                                                    S16 block_index, bool block_is_cloning, World_t* world){
     CheckBlockCollisionResult_t result {};

     result.pos_delta = block_pos_delta;
     result.vel = block_vel;
     result.accel = block_accel;

     result.stop_on_pixel_x = block_stop_on_pixel_x;
     result.stop_on_pixel_y = block_stop_on_pixel_y;

     result.horizontal_move = block_horizontal_move;
     result.vertical_move = block_vertical_move;
     result.collided_block_index = -1;

     auto block_inside_result = block_inside_others(block_pos,
                                                    result.pos_delta,
                                                    block_index,
                                                    block_is_cloning,
                                                    world->block_qt,
                                                    world->interactive_qt,
                                                    &world->tilemap,
                                                    &world->blocks);

     if(block_inside_result.count > 0 ){
          S16 collided_with_blocks_on_ice = 0;

          if(block_on_ice(block_pos, block_pos_delta, &world->tilemap, world->interactive_qt, world->block_qt) ||
             block_on_air(block_pos, block_pos_delta, &world->tilemap, world->interactive_qt, world->block_qt)){
               // calculate the momentum if we are on ice for later
               for(S8 i = 0; i < block_inside_result.count; i++){
                    // find the closest pixel in the collision rect to our block
                    auto collided_block_index = get_block_index(world, block_inside_result.entries[i].block);
                    auto collision_center_pixel = block_inside_result.entries[i].collision_pos.pixel;
                    Rect_t collision_rect;

                    collision_rect.left = collision_center_pixel.x - HALF_TILE_SIZE_IN_PIXELS;
                    collision_rect.bottom = collision_center_pixel.y - HALF_TILE_SIZE_IN_PIXELS;
                    collision_rect.right = collision_center_pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                    collision_rect.top = collision_center_pixel.y + HALF_TILE_SIZE_IN_PIXELS;

                    auto block_rect = block_get_rect(block_pos.pixel);

                    auto block_closest_pixel = closest_pixel_in_rect(collision_center_pixel, block_rect);
                    auto closest_pixel = closest_pixel_in_rect(block_closest_pixel, collision_rect);
                    auto direction_to_check_mask = pixel_direction_mask_between(block_closest_pixel, closest_pixel);

                    // check each adjacent pixel against the mask to see if it is inside a block that is not our block
                    // what this is trying to do, is check to see if we pass through another block to get to colliding with this block
                    // it happens in the case of a block traveling diagonally and hitting a block adjacent to another block. see ascii art
                    //     X
                    //    /X
                    //   X

                    if(direction_to_check_mask & DIRECTION_MASK_LEFT){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{1, 0},
                                                                 block_inside_result.entries[i].collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   block_inside_result.entries[i].invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_RIGHT){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{-1, 0},
                                                                 block_inside_result.entries[i].collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   block_inside_result.entries[i].invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_DOWN){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{0, 1},
                                                                 block_inside_result.entries[i].collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   block_inside_result.entries[i].invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_UP){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{0, -1},
                                                                 block_inside_result.entries[i].collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   block_inside_result.entries[i].invalidated = true;
                              }
                         }
                    }
               }

               // for valid collisions calculate how many blocks we collided with
               for(S8 i = 0; i < block_inside_result.count; i++){
                    if(block_inside_result.entries[i].invalidated) continue;

                    if(block_on_ice(block_inside_result.entries[i].block->pos, block_inside_result.entries[i].block->pos_delta, &world->tilemap, world->interactive_qt, world->block_qt) ||
                       block_on_air(block_inside_result.entries[i].block->pos, block_inside_result.entries[i].block->pos_delta, &world->tilemap, world->interactive_qt, world->block_qt)){
                         collided_with_blocks_on_ice++;
                    }
               }
          }

          for(S8 i = 0; i < block_inside_result.count; i++){
               if(block_inside_result.entries[i].invalidated) continue;

               result.collided = true;
               result.collided_block_index = get_block_index(world, block_inside_result.entries[i].block);
               result.collided_pos = block_inside_result.entries[i].collision_pos;
               result.collided_portal_rotations = block_inside_result.entries[i].portal_rotations;

               auto collided_block_center = block_inside_result.entries[i].collision_pos;

               // update collided block center if the block is stopping on a pixel this frame
               // TODO: account for stop_on_pixel through a portal
               if(block_inside_result.entries[i].block->stop_on_pixel_x){
                    auto portal_pixel_offset = (block_inside_result.entries[i].collision_pos - (block_inside_result.entries[i].block->pos + block_inside_result.entries[i].block->pos_delta)).pixel.x - HALF_TILE_SIZE_IN_PIXELS;
                    collided_block_center.pixel.x = block_inside_result.entries[i].block->stop_on_pixel_x + HALF_TILE_SIZE_IN_PIXELS + portal_pixel_offset;
                    collided_block_center.decimal.x = 0;
               }

               if(block_inside_result.entries[i].block->stop_on_pixel_y){
                    auto portal_pixel_offset = (block_inside_result.entries[i].collision_pos - (block_inside_result.entries[i].block->pos + block_inside_result.entries[i].block->pos_delta)).pixel.y - HALF_TILE_SIZE_IN_PIXELS;
                    collided_block_center.pixel.y = block_inside_result.entries[i].block->stop_on_pixel_y + HALF_TILE_SIZE_IN_PIXELS + portal_pixel_offset;
                    collided_block_center.decimal.y = 0;
               }

               auto moved_block_pos = block_get_center(block_pos);
               auto move_direction = move_direction_between(moved_block_pos, block_inside_result.entries[i].collision_pos);
               Direction_t first_direction;
               Direction_t second_direction;
               move_direction_to_directions(move_direction, &first_direction, &second_direction);

               // check if they are on ice before we adjust the position on our block to check
               bool a_on_ice_or_air = block_on_ice(block_pos, result.pos_delta, &world->tilemap, world->interactive_qt, world->block_qt) ||
                                      block_on_air(block_pos, result.pos_delta, &world->tilemap, world->interactive_qt, world->block_qt);
               bool b_on_ice_or_air = block_on_ice(block_inside_result.entries[i].block->pos, block_inside_result.entries[i].block->pos_delta,
                                                   &world->tilemap, world->interactive_qt, world->block_qt) ||
                   block_on_air(block_inside_result.entries[i].block->pos, block_inside_result.entries[i].block->pos_delta, &world->tilemap, world->interactive_qt, world->block_qt);
               bool both_frictionless = a_on_ice_or_air && b_on_ice_or_air;

               S16 block_inside_index = -1;
               if(block_inside_result.entries[i].block){
                   block_inside_index = get_block_index(world, block_inside_result.entries[i].block);
               }

               if(blocks_are_entangled(result.collided_block_index, block_index, &world->blocks)){
                    auto* block = world->blocks.elements + block_index;
                    auto* entangled_block = world->blocks.elements + result.collided_block_index;
                    auto final_block_pos = block->pos + block->pos_delta;
                    final_block_pos.pixel += HALF_TILE_SIZE_PIXEL;
                    auto pos_diff = pos_to_vec(result.collided_pos - final_block_pos);

                    auto pos_dimension_delta = fabs(fabs(pos_diff.x) - fabs(pos_diff.y));

                    // if positions are diagonal to each other and the rotation between them is odd, check if we are moving into each other
                    if(pos_dimension_delta <= FLT_EPSILON && (block->rotation + entangled_block->rotation + result.collided_portal_rotations) % 2 == 1){
                         // just gtfo if this happens, we handle this case outside this function
                         return result;
                    }

                    // if we are not moving towards the entangled block, it's on them to resolve the collision, so get outta here
                    DirectionMask_t pos_diff_dir_mask = vec_direction_mask(pos_diff);
                    auto pos_delta_dir = vec_direction(block->pos_delta);
                    if(!direction_in_mask(pos_diff_dir_mask, pos_delta_dir)){
                         return result;
                    }
               }

               auto collided_block_move_mask = vec_direction_mask(block_inside_result.entries[i].block->pos_delta);

               if(result.collided_portal_rotations){
                    // auto save_mask = collided_block_move_mask;

                    // unsure why this is correct, but we've proved it via example
                    auto total_rotations = DIRECTION_COUNT - result.collided_portal_rotations;

                    // TODO: make direction_mask_rotate_clockwise(mask, rotations) function
                    for(S8 r = 0; r < total_rotations; r++){
                         collided_block_move_mask = direction_mask_rotate_clockwise(collided_block_move_mask);
                    }
               }

               if(block_inside_index != block_index){
                    switch(move_direction){
                    default:
                         break;
                    case MOVE_DIRECTION_LEFT_UP:
                    case MOVE_DIRECTION_LEFT_DOWN:
                    case MOVE_DIRECTION_RIGHT_UP:
                    case MOVE_DIRECTION_RIGHT_DOWN:
                    {
                         bool impact_pos_delta_horizontal = false;
                         bool impact_pos_delta_vertical = false;

                         switch(first_direction){
                         default:
                              break;
                         case DIRECTION_LEFT:
                              if(block_pos_delta.x < 0){
                                   if(!direction_in_mask(collided_block_move_mask, first_direction)){
                                        impact_pos_delta_horizontal = true;
                                        result.stop_on_pixel_x = collided_block_center.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                                   }else{
                                        Position_t final_stop_pos = collided_block_center;
                                        final_stop_pos.pixel.x += HALF_TILE_SIZE_IN_PIXELS;
                                        result.pos_delta.x = pos_to_vec(final_stop_pos - block_pos).x;
                                   }
                              }
                              break;
                         case DIRECTION_RIGHT:
                              if(block_pos_delta.x > 0){
                                   if(!direction_in_mask(collided_block_move_mask, first_direction)){
                                        impact_pos_delta_horizontal = true;
                                        result.stop_on_pixel_x = (collided_block_center.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                                   }else{
                                        Position_t final_stop_pos = collided_block_center;
                                        final_stop_pos.pixel.x -= (HALF_TILE_SIZE_IN_PIXELS + TILE_SIZE_IN_PIXELS );
                                        result.pos_delta.x = pos_to_vec(final_stop_pos - block_pos).x;
                                   }
                              }
                              break;
                         }

                         switch(second_direction){
                         default:
                              break;
                         case DIRECTION_DOWN:
                              if(block_pos_delta.y < 0){
                                   if(!direction_in_mask(collided_block_move_mask, second_direction)){
                                        impact_pos_delta_vertical = true;
                                        result.stop_on_pixel_y = collided_block_center.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
                                   }else{
                                        Position_t final_stop_pos = collided_block_center;
                                        final_stop_pos.pixel.y += HALF_TILE_SIZE_IN_PIXELS;
                                        result.pos_delta.y = pos_to_vec(final_stop_pos - block_pos).y;
                                   }
                              }
                              break;
                         case DIRECTION_UP:
                              if(block_pos_delta.y > 0){
                                   if(!direction_in_mask(collided_block_move_mask, second_direction)){
                                        impact_pos_delta_vertical = true;
                                        result.stop_on_pixel_y = (collided_block_center.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS;
                                   }else{
                                        Position_t final_stop_pos = collided_block_center;
                                        final_stop_pos.pixel.y -= (HALF_TILE_SIZE_IN_PIXELS + TILE_SIZE_IN_PIXELS );
                                        result.pos_delta.y = pos_to_vec(final_stop_pos - block_pos).y;
                                   }
                              }
                              break;
                         }

                         Position_t final_stop_pos = pixel_pos(Pixel_t{result.stop_on_pixel_x, result.stop_on_pixel_y});
                         Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

                         if(impact_pos_delta_horizontal){
                              result.pos_delta.x = pos_delta.x;
                              if(!both_frictionless) result.stop_horizontally();
                         }

                         if(impact_pos_delta_vertical){
                              result.pos_delta.y = pos_delta.y;
                              if(!both_frictionless) result.stop_vertically();
                         }
                    } break;
                    case MOVE_DIRECTION_LEFT:
                         if(block_pos_delta.x < 0){
                              // TODO: compress these cases
                              if(direction_in_mask(collided_block_move_mask, first_direction)){
                                   Position_t final_stop_pos = collided_block_center;
                                   final_stop_pos.pixel.x += HALF_TILE_SIZE_IN_PIXELS;
                                   auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                                   result.pos_delta.x = new_pos_delta.x;
                              }else{
                                   result.stop_on_pixel_x = closest_pixel(collided_block_center.pixel.x + HALF_TILE_SIZE_IN_PIXELS, collided_block_center.decimal.x);

                                   Position_t final_stop_pos = pixel_pos(Pixel_t{result.stop_on_pixel_x, 0});
                                   Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

                                   result.pos_delta.x = pos_delta.x;
                                   if(!both_frictionless) result.stop_horizontally();
                              }
                         }
                         break;
                    case MOVE_DIRECTION_RIGHT:
                         if(block_pos_delta.x > 0){
                              if(direction_in_mask(collided_block_move_mask, first_direction)){
                                   Position_t final_stop_pos = collided_block_center;
                                   final_stop_pos.pixel.x -= (HALF_TILE_SIZE_IN_PIXELS + TILE_SIZE_IN_PIXELS);
                                   auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                                   result.pos_delta.x = new_pos_delta.x;
                              }else{
                                   result.stop_on_pixel_x = closest_pixel((collided_block_center.pixel.x - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS, collided_block_center.decimal.x);

                                   Position_t final_stop_pos = pixel_pos(Pixel_t{result.stop_on_pixel_x, 0});
                                   Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

                                   result.pos_delta.x = pos_delta.x;
                                   if(!both_frictionless) result.stop_horizontally();
                              }
                         }
                         break;
                    case MOVE_DIRECTION_DOWN:
                         if(block_pos_delta.y < 0){
                              if(direction_in_mask(collided_block_move_mask, first_direction)){
                                   Position_t final_stop_pos = collided_block_center;
                                   final_stop_pos.pixel.y += HALF_TILE_SIZE_IN_PIXELS;
                                   auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                                   result.pos_delta.y = new_pos_delta.y;
                              }else{
                                   result.stop_on_pixel_y = closest_pixel(collided_block_center.pixel.y + HALF_TILE_SIZE_IN_PIXELS, collided_block_center.decimal.y);

                                   Position_t final_stop_pos = pixel_pos(Pixel_t{0, result.stop_on_pixel_y});
                                   Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

                                   result.pos_delta.y = pos_delta.y;
                                   if(!both_frictionless) result.stop_vertically();
                              }
                         }
                         break;
                    case MOVE_DIRECTION_UP:
                         if(block_pos_delta.y > 0){
                              if(direction_in_mask(collided_block_move_mask, first_direction)){
                                   Position_t final_stop_pos = collided_block_center;
                                   final_stop_pos.pixel.y -= (HALF_TILE_SIZE_IN_PIXELS + TILE_SIZE_IN_PIXELS);
                                   auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                                   result.pos_delta.y = new_pos_delta.y;
                              }else{
                                   result.stop_on_pixel_y = closest_pixel((collided_block_center.pixel.y - HALF_TILE_SIZE_IN_PIXELS) - TILE_SIZE_IN_PIXELS, collided_block_center.decimal.y);

                                   Position_t final_stop_pos = pixel_pos(Pixel_t{0, result.stop_on_pixel_y});
                                   Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

                                   result.pos_delta.y = pos_delta.y;
                                   if(!both_frictionless) result.stop_vertically();
                              }
                         }
                         break;
                    }
               }else{
                    Coord_t block_coord = block_get_coord(block_pos);
                    Direction_t src_portal_dir = direction_between(block_coord, block_inside_result.entries[i].src_portal_coord);
                    Direction_t dst_portal_dir = direction_between(block_coord, block_inside_result.entries[i].dst_portal_coord);
                    DirectionMask_t move_mask = vec_direction_mask(result.vel);

                    auto resolve_result = resolve_block_colliding_with_itself(src_portal_dir, dst_portal_dir, move_mask, block_pos);

                    if(resolve_result.stop_on_pixel_x >= 0){
                         result.stop_on_pixel_x = resolve_result.stop_on_pixel_x;
                         if(!both_frictionless) result.stop_horizontally();

                         Position_t final_stop_pos = pixel_pos(Pixel_t{result.stop_on_pixel_x, 0});
                         Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);
                         result.pos_delta.x = pos_delta.x;
                    }

                    if(resolve_result.stop_on_pixel_y >= 0){
                         result.stop_on_pixel_y = resolve_result.stop_on_pixel_y;
                         if(!both_frictionless) result.stop_vertically();

                         Position_t final_stop_pos = pixel_pos(Pixel_t{0, result.stop_on_pixel_y});
                         Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);
                         result.pos_delta.y = pos_delta.y;
                    }
               }

               for(S16 p = 0; p < world->players.count; p++){
                    Player_t* player = world->players.elements + p;
                    if(player->pushing_block == block_index && first_direction == player->face){
                         player->push_time = 0.0f;
                    }
               }

               bool should_push = true;

               Block_t* last_block_in_chain = block_inside_result.entries[i].block;
               Direction_t against_direction = first_direction;

               while(true){
                    // TODO: handle multiple against blocks
                    auto against_result = block_against_other_blocks(last_block_in_chain->pos + last_block_in_chain->pos_delta, against_direction, world->block_qt,
                                                                     world->interactive_qt, &world->tilemap);
                    if(against_result.count > 0){
                         last_block_in_chain = against_result.againsts[0].block;
                         against_direction = direction_rotate_clockwise(against_direction, against_result.againsts[0].rotations_through_portal);
                    }else{
                         break;
                    }
               }

               bool c_on_ice_or_air = block_on_ice(last_block_in_chain->pos, last_block_in_chain->pos_delta,
                                                   &world->tilemap, world->interactive_qt, world->block_qt) ||
                                      block_on_air(last_block_in_chain->pos, last_block_in_chain->pos_delta, &world->tilemap, world->interactive_qt, world->block_qt);

               // if the blocks are headed in the same direction but the block is slowing down, slow down with it
               if(!c_on_ice_or_air){
                    switch(first_direction){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         if(block_vel.x < 0 &&
                            block_inside_result.entries[i].block->vel.x < 0 &&
                            block_vel.x < block_inside_result.entries[i].block->vel.x){
                              result.vel.x = block_inside_result.entries[i].block->vel.x;
                              if(result.vel.x == 0) result.stop_horizontally();
                              should_push = false;
                         }
                         break;
                    case DIRECTION_RIGHT:
                         if(block_vel.x > 0 &&
                            block_inside_result.entries[i].block->vel.x > 0 &&
                            block_vel.x > block_inside_result.entries[i].block->vel.x){
                              result.vel.x = block_inside_result.entries[i].block->vel.x;
                              if(result.vel.x == 0) result.stop_horizontally();
                              should_push = false;
                         }
                         break;
                    case DIRECTION_DOWN:
                         if(block_vel.y < 0 &&
                            block_inside_result.entries[i].block->vel.y < 0 &&
                            block_vel.y < block_inside_result.entries[i].block->vel.y){
                              result.vel.y = block_inside_result.entries[i].block->vel.y;
                              if(result.vel.y == 0) result.stop_vertically();
                              should_push = false;
                         }
                         break;
                    case DIRECTION_UP:
                         if(block_vel.y > 0 &&
                            block_inside_result.entries[i].block->vel.y >= 0 &&
                            block_vel.y > block_inside_result.entries[i].block->vel.y){
                              result.vel.y = block_inside_result.entries[i].block->vel.y;
                              if(result.vel.y == 0) result.stop_vertically();
                              should_push = false;
                         }
                         break;
                    }
               }

               if(a_on_ice_or_air && b_on_ice_or_air){
                    if(block_inside_index != block_index){
                         if(!direction_in_mask(vec_direction_mask(block_pos_delta), first_direction)){
                              // although we collided, the other block is colliding into us, so let that block resolve this mess
                              result.collided = false;
                              return result;
                         }
                    }

                    if(should_push){
                         BlockPush_t push;

                         push.pusher_index = block_index;
                         push.pushee_index = get_block_index(world, block_inside_result.entries[i].block);

                         push.direction_mask = direction_mask_add(direction_to_direction_mask(first_direction),
                                                                  direction_to_direction_mask(second_direction));

                         push.portal_rotations = block_inside_result.entries[i].portal_rotations;

                         // LOG("Add push, block %d %s | %s %d pr %d result vel: %f, %f\n",
                         //     block_index, direction_to_string(first_direction), direction_to_string(second_direction), push.pushee_index, push.portal_rotations,
                         //     result.vel.x, result.vel.y);

                         push.collided_with_block_count = collided_with_blocks_on_ice;

                         result.block_pushes.add(&push);
                    }
               }
          }
     }

     return result;
}

BlockCollidesWithItselfResult_t resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                                                    Position_t block_pos){
     BlockCollidesWithItselfResult_t result {};

     // double check that this is a rotated portal
     if(direction_is_horizontal(src_portal_dir) != direction_is_horizontal(dst_portal_dir)){
          if(move_mask & direction_to_direction_mask(src_portal_dir)){
               if(direction_is_horizontal(src_portal_dir)){
                    result.stop_on_pixel_x = closest_pixel(block_pos.pixel.x, block_pos.decimal.x);
               }else{
                    result.stop_on_pixel_y = closest_pixel(block_pos.pixel.y, block_pos.decimal.y);
               }
          }else if(move_mask & direction_to_direction_mask(dst_portal_dir)){
               if(direction_is_horizontal(dst_portal_dir)){
                    result.stop_on_pixel_x = closest_pixel(block_pos.pixel.x, block_pos.decimal.x);
               }else{
                    result.stop_on_pixel_y = closest_pixel(block_pos.pixel.y, block_pos.decimal.y);
               }
          }
     }

     return result;
}

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_qt, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Position_t* portal_offsets){
     U8 rotations_between_portals = portal_rotations_between(dst_portal_face, src_portal_face);
     Coord_t dst_coord = dst_portal_coord + direction_opposite(dst_portal_face);
     Position_t src_portal_center_pos = pixel_to_pos(coord_to_pixel_at_center(src_portal_coord));
     Position_t dst_center_pos = pixel_to_pos(coord_to_pixel_at_center(dst_coord));
     Rect_t rect = rect_surrounding_adjacent_coords(dst_coord);
     quad_tree_find_in(block_qt, rect, blocks, block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S8 o = 0; o < *block_count; o++){
          Position_t block_center = block_get_center(blocks[o]->pos + blocks[o]->pos_delta);
          Position_t dst_offset = block_center - dst_center_pos;
          Position_t src_fake_pos = src_portal_center_pos + position_rotate_quadrants_clockwise(dst_offset, rotations_between_portals);
          portal_offsets[o] = src_fake_pos - block_center;
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

void push_entangled_block(Block_t* block, World_t* world, Direction_t push_dir, bool pushed_by_ice, TransferMomentum_t* instant_momentum){
     if(block->entangle_index < 0) return;

     S16 block_index = block - world->blocks.elements;
     S16 entangle_index = block->entangle_index;
     while(entangle_index != block_index && entangle_index >= 0){
          Block_t* entangled_block = world->blocks.elements + entangle_index;
          bool held_down = block_held_down_by_another_block(entangled_block, world->block_qt, world->interactive_qt, &world->tilemap).held();
          bool on_ice = block_on_ice(entangled_block->pos, entangled_block->pos_delta, &world->tilemap, world->interactive_qt, world->block_qt);
          if(!held_down || on_ice){
               auto rotations_between = direction_rotations_between(static_cast<Direction_t>(entangled_block->rotation), static_cast<Direction_t>(block->rotation));
               Direction_t rotated_dir = direction_rotate_clockwise(push_dir, rotations_between);

               if(instant_momentum){
                    auto rotated_instant_momentum = *instant_momentum;

                    switch(rotated_dir){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         if(rotated_instant_momentum.vel > 0) rotated_instant_momentum.vel = -rotated_instant_momentum.vel;
                         break;
                    case DIRECTION_RIGHT:
                         if(rotated_instant_momentum.vel < 0) rotated_instant_momentum.vel = -rotated_instant_momentum.vel;
                         break;
                    case DIRECTION_DOWN:
                         if(rotated_instant_momentum.vel > 0) rotated_instant_momentum.vel = -rotated_instant_momentum.vel;
                         break;
                    case DIRECTION_UP:
                         if(rotated_instant_momentum.vel < 0) rotated_instant_momentum.vel = -rotated_instant_momentum.vel;
                         break;
                    }

                    auto allowed_result = allowed_to_push(world, entangled_block, rotated_dir, &rotated_instant_momentum);
                    if(allowed_result.push){
                         // LOG("  entangled push %d %s\n", get_block_index(world, entangled_block), direction_to_string(rotated_dir));
                         block_push(entangled_block, rotated_dir, world, pushed_by_ice, allowed_result.mass_ratio, &rotated_instant_momentum, true);
                         // TODO: deal_with_push_result() here and below
                    }
               }else{
                    auto allowed_result = allowed_to_push(world, entangled_block, rotated_dir);
                    if(allowed_result.push){
                         // LOG("  entangled push %d %s\n", get_block_index(world, entangled_block), direction_to_string(rotated_dir));
                         block_push(entangled_block, rotated_dir, world, pushed_by_ice, allowed_result.mass_ratio);
                    }
               }
          }
          entangle_index = entangled_block->entangle_index;
     }
}

void apply_block_change(ObjectArray_t<Block_t>* blocks_array, BlockChange_t* change){
     auto block = blocks_array->elements + change->block_index;

     switch(change->type){
     case BLOCK_CHANGE_TYPE_POS_DELTA_X:
          block->pos_delta.x = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_POS_DELTA_Y:
          block->pos_delta.y = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_VEL_X:
          // LOG("changing block %d vel x from %f to %f\n", change->block_index, block->vel.x, change->decimal);
          block->vel.x = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_VEL_Y:
          block->vel.y = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_ACCEL_X:
          block->accel.x = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_ACCEL_Y:
          block->accel.y = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_STATE:
          block->horizontal_move.state = change->move_state;
          break;
     case BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_SIGN:
          block->horizontal_move.sign = change->move_sign;
          break;
     case BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_TIME_LEFT:
          block->horizontal_move.time_left = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_VERTICAL_MOVE_STATE:
          block->vertical_move.state = change->move_state;
          break;
     case BLOCK_CHANGE_TYPE_VERTICAL_MOVE_SIGN:
          block->vertical_move.sign = change->move_sign;
          break;
     case BLOCK_CHANGE_TYPE_VERTICAL_MOVE_TIME_LEFT:
          block->vertical_move.time_left = change->decimal;
          break;
     case BLOCK_CHANGE_TYPE_STOP_ON_PIXEL_X:
          block->stop_on_pixel_x = change->integer;
          break;
     case BLOCK_CHANGE_TYPE_STOP_ON_PIXEL_Y:
          block->stop_on_pixel_y = change->integer;
          break;
     }
}

struct DealWithPushResult_t
{
     BlockChanges_t changes;
     Block_t* block_receiving_force = nullptr;
     Direction_t final_direction = DIRECTION_COUNT;
};

DealWithPushResult_t deal_with_push_result(Block_t* pusher, Direction_t direction_to_check, S8 portal_rotations,
                                           F32 scale_push_velocity, World_t* world, BlockPushResult_t* push_result){
     DealWithPushResult_t result;

     auto save_direction_to_check = direction_to_check;
     direction_to_check = direction_rotate_counter_clockwise(direction_to_check, portal_rotations);
     S8 total_against_rotations = portal_rotations;

     auto block_receiving_force = pusher;

     // TODO: handle multiple blocks we could be against
     // adjust the force back through the chain
     while(true){
          auto block_receiving_force_final_pos = block_receiving_force->pos + block_receiving_force->pos_delta;
          if(block_receiving_force->teleport) block_receiving_force_final_pos = block_receiving_force->teleport_pos + block_receiving_force->teleport_pos_delta;

          auto adjacent_results = block_against_other_blocks(block_receiving_force_final_pos, direction_to_check, world->block_qt,
                                                             world->interactive_qt, &world->tilemap);
          // ignore if we are against ourselves
          if(adjacent_results.count && adjacent_results.againsts[0].block != block_receiving_force){
               auto new_direction_to_check = direction_rotate_clockwise(direction_to_check, adjacent_results.againsts[0].rotations_through_portal);
               auto vel = direction_is_horizontal(new_direction_to_check) ? adjacent_results.againsts[0].block->vel.x : adjacent_results.againsts[0].block->vel.y;
               if(vel != 0){
                    block_receiving_force = adjacent_results.againsts[0].block;
                    direction_to_check = direction_rotate_clockwise(direction_to_check, adjacent_results.againsts[0].rotations_through_portal);
                    total_against_rotations += adjacent_results.againsts[0].rotations_through_portal;
                    total_against_rotations %= DIRECTION_COUNT;
               }else{
                    break;
               }
          }else{
               break;
          }
     }

     {
          static const bool negate_push_direction_table[DIRECTION_COUNT][DIRECTION_COUNT] = {
               {false, true,  true, false}, // left
               {false, false, true, true}, // up
               {false, true,  true, false}, // right
               {false, false,  true, true}, // down
          };

          if(negate_push_direction_table[save_direction_to_check][total_against_rotations]){
               push_result->velocity = -push_result->velocity;
          }
     }

     result.block_receiving_force = block_receiving_force;
     result.final_direction = direction_to_check;

     S16 block_receiving_force_index = block_receiving_force - world->blocks.elements;

     switch(direction_to_check){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          if(push_result->transferred_momentum_back()){
               F32 new_vel = push_result->velocity * scale_push_velocity;
               F32 x_pos = pos_to_vec(block_receiving_force->pos + block_receiving_force->pos_delta).x;

               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VEL_X, new_vel);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_ACCEL_X, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_STATE, MOVE_STATE_COASTING);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_SIGN, move_sign_from_vel(new_vel));
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_TIME_LEFT, calc_coast_motion_time_left(x_pos, new_vel));
          }else{
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VEL_X, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_ACCEL_X, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_STATE, MOVE_STATE_IDLING);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_SIGN, MOVE_SIGN_ZERO);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_TIME_LEFT, 0.0f);
          }
          break;
     case DIRECTION_UP:
     case DIRECTION_DOWN:
          if(push_result->transferred_momentum_back()){
               F32 new_vel = push_result->velocity * scale_push_velocity;
               F32 y_pos = pos_to_vec(block_receiving_force->pos + block_receiving_force->pos_delta).y;

               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VEL_Y, new_vel);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_ACCEL_Y, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_STATE, MOVE_STATE_COASTING);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_SIGN, move_sign_from_vel(new_vel));
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_TIME_LEFT, calc_coast_motion_time_left(y_pos, new_vel));
          }else{
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VEL_Y, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_ACCEL_Y, 0.0f);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_STATE, MOVE_STATE_IDLING);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_SIGN, MOVE_SIGN_ZERO);
               result.changes.add(block_receiving_force_index, BLOCK_CHANGE_TYPE_VERTICAL_MOVE_TIME_LEFT, 0.0f);
          }
          break;
     }

     return result;
}

BlockCollisionPushResult_t block_collision_push(BlockPush_t* push, World_t* world){
     BlockCollisionPushResult_t result;

     auto* pusher = world->blocks.elements + push->pusher_index;
     auto* pushee = world->blocks.elements + push->pushee_index;

     TransferMomentum_t instant_momentum;
     instant_momentum.mass = get_block_stack_mass(world, pusher);
     instant_momentum.mass = (S16)((F32)(instant_momentum.mass) * (1.0f / (F32)(push->collided_with_block_count)));

     for(S16 d = 0; d < DIRECTION_COUNT; d++){
          Direction_t direction = (Direction_t)(d);
          if(!direction_in_mask(push->direction_mask, direction)) continue;

          Vec_t rotated_pusher_vel = vec_rotate_quadrants_clockwise(pusher->vel, push->portal_rotations);

          Direction_t push_direction = direction_rotate_clockwise(direction, push->portal_rotations);

          F32 current_collision_block_vel = 0;
          F32 current_pos_delta = 0;
          F32 scale_push_velocity = 1.0f;

          switch(push_direction){
          default:
               break;
          case DIRECTION_LEFT:
          case DIRECTION_RIGHT:
               current_collision_block_vel = pushee->vel.x;
               current_pos_delta = pushee->pos_delta.x;
               instant_momentum.vel = rotated_pusher_vel.x;
               break;
          case DIRECTION_UP:
          case DIRECTION_DOWN:
               current_collision_block_vel = pushee->vel.y;
               current_pos_delta = pushee->pos_delta.y;
               instant_momentum.vel = rotated_pusher_vel.y;
               break;
          }

          // If the block has velocity but hasn't moved yet, then it has been collided with another block during this frame.
          // Thus, the collisions happened at the same time.
          if(current_collision_block_vel != 0 && current_pos_delta == 0){
               auto collided_block_mass = get_block_stack_mass(world, pushee);

               // steal the momentum from the collided block to mimic the collisions happening as a simultaneous mass
               auto fake_elastic_addition = elastic_transfer_momentum(collided_block_mass, current_collision_block_vel, instant_momentum.mass, 0);
               scale_push_velocity = instant_momentum.vel / (instant_momentum.vel + fake_elastic_addition.second_final_velocity);
               instant_momentum.vel += fake_elastic_addition.second_final_velocity;

               // kill the collided blocks velocity so that the it transfers all of the momentum
               switch(push_direction){
               default:
                    break;
               case DIRECTION_LEFT:
               case DIRECTION_RIGHT:
                    pushee->vel.x = 0;
                    break;
               case DIRECTION_UP:
               case DIRECTION_DOWN:
                    pushee->vel.y = 0;
                    break;
               }
          }

          // LOG("collision on ice causes block %d pushes block %d %s with mass: %d, vel: %f\n",
          //    push->pusher_index, push->pushee_index, direction_to_string(push_direction), instant_momentum.mass, instant_momentum.vel);

          auto push_pos = pushee->pos;
          auto push_pos_delta = pushee->pos_delta;

          if(pushee->teleport){
               push_pos = pushee->teleport_pos;
               push_pos_delta = pushee->teleport_pos_delta;
          }

          auto push_result = block_push(pushee, push_pos, push_pos_delta, push_direction, world, true, 1.0f, &instant_momentum);

          auto direction_to_check = direction_opposite(push_direction);

          auto deal_with_push_result_result = deal_with_push_result(pusher, direction_to_check, push->portal_rotations,
                                                                    scale_push_velocity, world, &push_result);

          if(push_result.pushed || push_result.force_flowed_through){
               push_entangled_block(pushee, world, push_direction, true, &instant_momentum);
          }

          if(push_result.transferred_momentum_back()){
               result.add_block_pushed(get_block_index(world, deal_with_push_result_result.block_receiving_force), direction_opposite(deal_with_push_result_result.final_direction));
               push_entangled_block(deal_with_push_result_result.block_receiving_force, world, direction_opposite(deal_with_push_result_result.final_direction), true, &instant_momentum);
          }

          result.changes.merge(&deal_with_push_result_result.changes);
     }

     return result;
}
