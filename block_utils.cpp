#include "block_utils.h"
#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"
#include "tags.h"

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

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, BlockCut_t cut, Direction_t direction, Pixel_t* a, Pixel_t* b){
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
          pixel.y += block_get_height_in_pixels(cut) - 1;
          *b = pixel;
          return true;
     };
     case DIRECTION_RIGHT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check_pos.pixel;
          pixel.x += block_get_width_in_pixels(cut) + 1;
          *a = pixel;

          // check top corner
          pixel.y += block_get_height_in_pixels(cut) - 1;
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
          pixel.x += block_get_width_in_pixels(cut) - 1;
          *b = pixel;
          return true;
     };
     case DIRECTION_UP:
     {
          // check left corner
          Pixel_t pixel = block_to_check_pos.pixel;
          pixel.y += block_get_height_in_pixels(cut) + 1;
          *a = pixel;

          // check right corner
          pixel.x += block_get_width_in_pixels(cut) - 1;
          *b = pixel;
          return true;
     };
     }

     return false;
}

bool block_against_block(Position_t pos, BlockCut_t cut, Block_t* check_block, BlockCut_t check_cut, Direction_t direction, Position_t portal_offset){
     // intentionally use the original pos without pos_delta
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          {
               if(!blocks_at_collidable_height(pos.z, check_block->pos.z)) break;

               auto adjacent_pos = block_get_final_position(check_block) + portal_offset;
               auto boundary_check_pos = adjacent_pos + Pixel_t{block_get_width_in_pixels(check_cut), 0};
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - pos).x);

               auto bottom_boundary_pos = pos + Pixel_t{0, (S16)(-(block_get_height_in_pixels(check_cut) - 1))};
               auto top_boundary_pos = pos + Pixel_t{0, block_get_width_in_pixels(cut)};

               // check if they are on the expected boundary
               if(distance_to_boundary <= DISTANCE_EPSILON &&
                  position_y_greater_than(adjacent_pos, bottom_boundary_pos) &&
                  position_y_less_than(adjacent_pos, top_boundary_pos)){
                    return true;
               }
          }
          break;
     case DIRECTION_RIGHT:
          {
               if(!blocks_at_collidable_height(pos.z, check_block->pos.z)) break;

               auto adjacent_pos = block_get_final_position(check_block) + portal_offset;
               auto boundary_check_pos = adjacent_pos;
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - (pos + Pixel_t{block_get_width_in_pixels(cut), 0})).x);

               auto bottom_boundary_pos = pos + Pixel_t{0, (S16)(-(block_get_height_in_pixels(check_cut) - 1))};
               auto top_boundary_pos = pos + Pixel_t{0, block_get_height_in_pixels(cut)};

               // check if they are on the expected boundary
               if(distance_to_boundary <= DISTANCE_EPSILON &&
                  position_y_greater_than(adjacent_pos, bottom_boundary_pos) &&
                  position_y_less_than(adjacent_pos, top_boundary_pos)){
                    return true;
               }
          }
          break;
     case DIRECTION_DOWN:
          {
               if(!blocks_at_collidable_height(pos.z, check_block->pos.z)) break;

               auto adjacent_pos = block_get_final_position(check_block) + portal_offset;
               auto boundary_check_pos = adjacent_pos + Pixel_t{0, block_get_height_in_pixels(cut)};
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - pos).y);

               auto left_boundary_pos = pos + Pixel_t{(S16)(-(block_get_width_in_pixels(check_cut) - 1)), 0};
               auto right_boundary_pos = pos + Pixel_t{block_get_width_in_pixels(cut), 0};

               // check if they are on the expected boundary
               if(distance_to_boundary <= DISTANCE_EPSILON &&
                  position_x_greater_than(adjacent_pos, left_boundary_pos) &&
                  position_x_less_than(adjacent_pos, right_boundary_pos)){
                    return true;
               }
          }
          break;
     case DIRECTION_UP:
          {
               if(!blocks_at_collidable_height(pos.z, check_block->pos.z)) break;

               auto adjacent_pos = block_get_final_position(check_block) + portal_offset;
               auto boundary_check_pos = adjacent_pos;
               auto distance_to_boundary = fabs(pos_to_vec(boundary_check_pos - (pos + Pixel_t{0, block_get_height_in_pixels(cut)})).y);

               auto left_boundary_pos = pos + Pixel_t{(S16)(-(block_get_width_in_pixels(check_cut) - 1)), 0};
               auto right_boundary_pos = pos + Pixel_t{block_get_width_in_pixels(cut), 0};

               // check if they are on the expected boundary
               if(distance_to_boundary <= DISTANCE_EPSILON &&
                  position_x_greater_than(adjacent_pos, left_boundary_pos) &&
                  position_x_less_than(adjacent_pos, right_boundary_pos)){
                    return true;
               }
          }
          break;
     }

     return false;
}

Block_t* block_against_block_in_list(Position_t pos, BlockCut_t cut, Block_t** blocks, S16 block_count, Direction_t direction, Position_t* portal_offsets){
     for(S16 i = 0; i < block_count; i++){
          if(block_against_block(pos, cut, blocks[i], block_get_cut(blocks[i]), direction, portal_offsets[i])){
              return blocks[i];
          }
     }

     return nullptr;
}

BlockAgainstOthersResult_t block_against_other_blocks(Position_t pos, BlockCut_t cut, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                      QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, bool require_portal_on){
     BlockAgainstOthersResult_t result;

     auto block_center = block_get_center(pos, cut);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center.pixel);

     Position_t portal_offsets[MAX_BLOCKS_FOUND_THROUGH_PORTALS];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     S16 block_count = 0;
     Block_t* blocks[MAX_BLOCKS_FOUND_THROUGH_PORTALS];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, MAX_BLOCKS_FOUND_THROUGH_PORTALS);

     for(S16 i = 0; i < block_count; i++){
          // lol at me misusing this function, but watevs
          if(block_against_block_in_list(pos, cut, blocks + i, 1, direction, portal_offsets)){
               BlockAgainstOther_t against_other {};
               against_other.block = blocks[i];
               result.insert(&against_other);
          }
     }

     auto found_blocks = find_blocks_through_portals(pos_to_coord(block_center), tilemap, interactive_qt, block_qt, require_portal_on);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;
         blocks[i] = found_block->block;
         portal_offsets[i] = found_block->position - block_get_final_position(found_block->block);

         if(block_against_block(pos, cut, blocks[i], found_block->rotated_cut, direction, portal_offsets[i])){
              BlockAgainstOther_t against_other {};
              against_other.block = blocks[i];
              against_other.rotations_through_portal = found_block->rotations_between_portals;
              against_other.through_portal = true;
              result.insert(&against_other);
         }
     }

     return result;
}

BlockAgainstOther_t block_diagonally_against_block(Position_t pos, BlockCut_t cut, DirectionMask_t directions, TileMap_t* tilemap,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     BlockAgainstOther_t result {};
     Pixel_t pixel_to_check;
     BlockCorner_t corner_to_check;

     if(directions == (DIRECTION_MASK_LEFT | DIRECTION_MASK_DOWN)){
          pixel_to_check = pos.pixel + Pixel_t{-1, -1};
          corner_to_check = BLOCK_CORNER_TOP_RIGHT;
     }else if(directions == (DIRECTION_MASK_LEFT | DIRECTION_MASK_UP)){
          pixel_to_check = block_top_left_pixel(pos.pixel, cut) + Pixel_t{-1, 1};
          corner_to_check = BLOCK_CORNER_BOTTOM_RIGHT;
     }else if(directions == (DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN)){
          pixel_to_check = block_bottom_right_pixel(pos.pixel, cut) + Pixel_t{1, -1};
          corner_to_check = BLOCK_CORNER_TOP_LEFT;
     }else if(directions == (DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP)){
          pixel_to_check = block_top_right_pixel(pos.pixel, cut) + Pixel_t{1, 1};
          corner_to_check = BLOCK_CORNER_BOTTOM_LEFT;
     }else{
          return result;
     }

     auto block_center = block_get_center(pos, cut);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center.pixel);

     Position_t portal_offsets[MAX_BLOCKS_FOUND_THROUGH_PORTALS];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     S16 block_count = 0;
     Block_t* blocks[MAX_BLOCKS_FOUND_THROUGH_PORTALS];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, MAX_BLOCKS_FOUND_THROUGH_PORTALS);

     // TODO: this function is at a pixel level, but should be more granular
     // adjacent blockers mean, if any blocks are adjacent, then we can't actually have a diagonal collision because
     // we would have collided with those adjacent blocks first.
     // bool adjacent_blocker = false;
     for(S16 i = 0; i < block_count; i++){
          Pixel_t corner_pixel = block_get_corner_pixel(blocks[i], corner_to_check);
          if(corner_pixel == pixel_to_check){
               result.block = blocks[i];
               return result;
          }
     }

     auto found_blocks = find_blocks_through_portals(pos_to_coord(block_center), tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
          auto* found_block = found_blocks.objects + i;
          BlockCut_t found_block_cut = block_get_cut(found_block->block);
          BlockCut_t rotated_cut = block_cut_rotate_clockwise(found_block_cut, found_block->portal_rotations);
          Pixel_t corner_pixel = block_get_corner_pixel(found_block->position.pixel, rotated_cut, corner_to_check);
          if(corner_pixel == pixel_to_check){
               result.block = found_block->block;
               result.rotations_through_portal = found_block->portal_rotations;
               result.through_portal = true;
               return result;
          }

     }

     return result;
}

Block_t* block_against_another_block(Position_t pos, BlockCut_t cut, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, Direction_t* push_dir){
     auto block_center = block_get_center(pos, cut);
     Rect_t rect = rect_to_check_surrounding_blocks(block_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Position_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     Block_t* collided_block = block_against_block_in_list(pos, cut, blocks, block_count, direction, portal_offsets);
     if(collided_block){
          *push_dir = direction;
          return collided_block;
     }

     auto found_blocks = find_blocks_through_portals(pos_to_coord(block_center), tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;
         blocks[i] = found_block->block;
         auto block_pos = block_get_final_position(found_block->block);
         portal_offsets[i] = found_block->position - block_pos;

         if(block_against_block(pos, cut, blocks[i], found_block->rotated_cut, direction, portal_offsets[i])){
              *push_dir = direction_rotate_clockwise(direction, found_block->rotations_between_portals);
              return collided_block;
         }
     }

     return nullptr;
}

Pixel_t get_check_pixel_against_centroid(Pixel_t block_pixel, BlockCut_t cut, Direction_t direction, S8 rotations_between){
     Pixel_t check_pixel = {-1, -1};

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y + block_get_height_in_pixels(cut);
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y - 1;
          }
          break;
     case DIRECTION_UP:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x + block_get_width_in_pixels(cut);
               check_pixel.y = block_pixel.y + block_get_height_in_pixels(cut);
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y + block_get_height_in_pixels(cut);
          }
          break;
     case DIRECTION_RIGHT:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x + block_get_width_in_pixels(cut);
               check_pixel.y = block_pixel.y - 1;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x + block_get_width_in_pixels(cut);
               check_pixel.y = block_pixel.y + block_get_height_in_pixels(cut);
          }
          break;
     case DIRECTION_DOWN:
          if(rotations_between == 1){
               check_pixel.x = block_pixel.x - 1;
               check_pixel.y = block_pixel.y - 1;
          }else if(rotations_between == 3){
               check_pixel.x = block_pixel.x + block_get_width_in_pixels(cut);
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

                    auto check_block_pixel = portal_coord_center_pixel + check_block_portal_offset - block_center_pixel_offset(check_block->cut);
                    auto check_block_rect = block_get_inclusive_rect(check_block_pixel, check_block->cut);
                    auto check_pixel = get_check_pixel_against_centroid(block->pos.pixel, block->cut, block_move_dir, rotations_between);

                    if(pixel_in_rect(check_pixel, check_block_rect)) return check_block;
               }
          }
     }

     return nullptr;
}

Block_t* rotated_entangled_blocks_against_centroid(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                   ObjectArray_t<Block_t>* blocks_array,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap){
     if(block->entangle_index < 0) return NULL;

     auto block_center = block_get_center(block);
     Rect_t rect = rect_to_check_surrounding_blocks(block_center.pixel);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     // TODO: compress this with below code that deals with portals
     for(S16 i = 0; i < block_count; i++){
          Block_t* check_block = blocks[i];
          if(!blocks_are_entangled(block, check_block, blocks_array)) continue;

          auto check_block_rect = block_get_inclusive_rect(check_block);
          S8 rotations_between = blocks_rotations_between(block, check_block);

          auto check_pixel = get_check_pixel_against_centroid(block->pos.pixel, block->cut, direction, rotations_between);

          if(pixel_in_rect(check_pixel, check_block_rect)){
               add_global_tag(TAG_ENTANGLED_CENTROID_COLLISION);
               return check_block;
          }
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
          if(check_block){
               add_global_tag(TAG_ENTANGLED_CENTROID_COLLISION);
               return check_block;
          }
     }

     return nullptr;
}

Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     Position_t block_to_check_pos = block_get_position(block_to_check);
     Vec_t block_to_check_pos_delta = block_get_pos_delta(block_to_check);
     BlockCut_t block_to_check_cut = block_get_cut(block_to_check);

     if(!block_adjacent_pixels_to_check(block_to_check_pos, block_to_check_pos_delta, block_to_check_cut,
                                        direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     // TODO: compress
     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Interactive_t* interactive = quad_tree_interactive_solid_at(interactive_qt, tilemap, tile_coord, block_to_check->pos.z);
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_POPUP &&
             interactive->popup.lift.ticks - 1 <= block_to_check->pos.z){
          }else{
               return interactive;
          }
     }

     PortalExit_t portal_exits = find_portal_exits(tile_coord, tilemap, interactive_qt);
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          Direction_t current_portal_dir = (Direction_t)(d);
          auto portal_exit = portal_exits.directions + d;

          for(S8 p = 0; p < portal_exit->count; p++){
               auto portal_dst_coord = portal_exit->coords[p];
               if(portal_dst_coord == tile_coord) continue;

               Coord_t portal_dst_output_coord = portal_dst_coord + direction_opposite(current_portal_dir);

               interactive = quad_tree_interactive_solid_at(interactive_qt, tilemap, portal_dst_output_coord, block_to_check->pos.z);
               if(interactive) return interactive;
          }
     }

     tile_coord = pixel_to_coord(pixel_b);
     interactive = quad_tree_interactive_solid_at(interactive_qt, tilemap, tile_coord, block_to_check->pos.z);
     if(interactive) return interactive;

     portal_exits = find_portal_exits(tile_coord, tilemap, interactive_qt);
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          Direction_t current_portal_dir = (Direction_t)(d);
          auto portal_exit = portal_exits.directions + d;

          for(S8 p = 0; p < portal_exit->count; p++){
               auto portal_dst_coord = portal_exit->coords[p];
               if(portal_dst_coord == tile_coord) continue;

               Coord_t portal_dst_output_coord = portal_dst_coord + direction_opposite(current_portal_dir);

               interactive = quad_tree_interactive_solid_at(interactive_qt, tilemap, portal_dst_output_coord, block_to_check->pos.z);
               if(interactive) return interactive;
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

struct BlockInsideBlockInListResult_t{
     Block_t* block = nullptr;
     Position_t collided_pos;
     Vec_t collided_pos_delta;
     Vec_t collided_overlap;
     S16 entry_index = -1;
};

#define MAX_BLOCK_INSIDE_BLOCK_COUNT 16

struct BlockInsideBlockListResult_t{
     BlockInsideBlockInListResult_t entries[MAX_BLOCK_INSIDE_BLOCK_COUNT];
     S8 count = 0;

     bool add(Block_t* block, Position_t collided_pos, Vec_t collided_pos_delta, Vec_t collided_overlap, S16 entry_index){
          if(count >= MAX_BLOCK_INSIDE_BLOCK_COUNT) return false;
          entries[count].block = block;
          entries[count].collided_pos = collided_pos;
          entries[count].collided_pos_delta = collided_pos_delta;
          entries[count].collided_overlap = collided_overlap;
          entries[count].entry_index = entry_index;
          count++;
          return true;
     }
};

BlockInsideBlockListResult_t block_inside_block_list(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta,
                                                     BlockCut_t cut, S16 block_to_check_index, bool block_to_check_cloning,
                                                     Block_t** blocks, S16 block_count, ObjectArray_t<Block_t>* blocks_array,
                                                     BlockCut_t* cuts, Position_t* portal_offsets){
     BlockInsideBlockListResult_t result;

     auto final_block_to_check_pos = block_to_check_pos + block_to_check_pos_delta;

     S16 block_width = block_get_width_in_pixels(cut);
     S16 block_height = block_get_height_in_pixels(cut);

     Quad_t quad = {0, 0, (block_width * PIXEL_SIZE), (block_height * PIXEL_SIZE)};

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];

          // don't collide with blocks that are cloning with our block to check
          if(block_to_check_cloning){
               Block_t* block_to_check = blocks_array->elements + block_to_check_index;
               if(blocks_are_entangled(block_to_check, block, blocks_array) && block->clone_start.x != 0) continue;
          }

          if(!blocks_at_collidable_height(final_block_to_check_pos.z, block->pos.z)) continue;

          auto block_pos = block_get_position(block);
          auto block_pos_delta = block_get_pos_delta(block);
          auto final_block_pos = block_pos + block_pos_delta + portal_offsets[i];

          auto pos_diff = final_block_pos - final_block_to_check_pos;
          auto check_vec = pos_to_vec(pos_diff);

          block_width = block_get_width_in_pixels(cuts[i]);
          block_height = block_get_height_in_pixels(cuts[i]);

          Quad_t quad_to_check = {check_vec.x, check_vec.y, check_vec.x + (block_width * PIXEL_SIZE), check_vec.y + (block_height * PIXEL_SIZE)};

          if(block_to_check_index == (block - blocks_array->elements)){
               // if, after applying the portal offsets it is the same quad to check and it is the same index,
               // ignore it because the block is literally just going through a portal, nbd
               if(quad == quad_to_check) continue;
          }

          auto first_result = quad_in_quad_high_range_exclusive(&quad, &quad_to_check);
          auto second_result = quad_in_quad_high_range_exclusive(&quad_to_check, &quad);
          if(first_result.inside){
               auto block_cut = block_get_cut(block);
               final_block_pos.pixel += block_center_pixel_offset(block_cut);
               result.add(block, final_block_pos, block_pos_delta, Vec_t{first_result.horizontal_overlap, first_result.vertical_overlap}, i);
          }else if(second_result.inside){
               auto block_cut = block_get_cut(block);
               final_block_pos.pixel += block_center_pixel_offset(block_cut);
               result.add(block, final_block_pos, block_pos_delta, Vec_t{second_result.horizontal_overlap, second_result.vertical_overlap}, i);
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
          if(blocks_at_collidable_height(z, blocks[i]->pos.z) && pixel_in_rect(pixel, block_get_inclusive_rect(blocks[i]))){
               return blocks[i];
          }
     }

     // TODO: handle portal logic in the same way

     return nullptr;
}

BlockInsideOthersResult_t block_inside_others(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta,
                                              BlockCut_t cut, S16 block_to_check_index,
                                              bool block_to_check_cloning, QuadTreeNode_t<Block_t>* block_qt,
                                              QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap,
                                              ObjectArray_t<Block_t>* block_array){
     BlockInsideOthersResult_t result = {};

     auto block_to_check_center_pixel = block_center_pixel(block_to_check_pos, cut);

     // TODO: need more complicated function to detect this
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center_pixel);
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Position_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     memset(portal_offsets, 0, sizeof(portal_offsets));

     BlockCut_t cuts[BLOCK_QUAD_TREE_MAX_QUERY];
     for(S16 i = 0; i < block_count; i++) cuts[i] = block_get_cut(blocks[i]);

     auto inside_list_result = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                       cut, block_to_check_index,
                                                       block_to_check_cloning, blocks, block_count,
                                                       block_array, cuts, portal_offsets);
     for(S8 i = 0; i < inside_list_result.count; i++){
          BlockInsideBlockResult_t entry {};
          entry.init(inside_list_result.entries[i].block, inside_list_result.entries[i].collided_pos,
                     inside_list_result.entries[i].collided_pos_delta, inside_list_result.entries[i].collided_overlap);
          result.insert(&entry);
     }

     auto block_coord = pixel_to_coord(block_to_check_center_pixel);

     auto found_blocks = find_blocks_through_portals(block_coord, tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;
         blocks[i] = found_block->block;
         auto block_pos = block_get_final_position(found_block->block);
         portal_offsets[i] = found_block->position - block_pos;
         cuts[i] = found_block->rotated_cut;
     }

     inside_list_result = block_inside_block_list(block_to_check_pos, block_to_check_pos_delta,
                                                  cut, block_to_check_index, block_to_check_cloning,
                                                  blocks, found_blocks.count, block_array, cuts, portal_offsets);

     for(S8 i = 0; i < inside_list_result.count; i++){
         BlockThroughPortal_t* associated_found_block = found_blocks.objects + inside_list_result.entries[i].entry_index;

         if(block_to_check_index == (inside_list_result.entries[i].block - block_array->elements) &&
            direction_in_mask(vec_direction_mask(block_to_check_pos_delta), associated_found_block->dst_portal_dir)){
              // if we collide with ourselves through a portal, then ignore certain cases
              // that we end up inside, because while they are true, they are not what we
              // have decided should happen
         }else{
             U8 portal_rotations = portal_rotations_between(associated_found_block->src_portal_dir, associated_found_block->dst_portal_dir);
             Vec_t collided_pos_delta = vec_rotate_quadrants_clockwise(inside_list_result.entries[i].collided_pos_delta, portal_rotations);

             BlockInsideBlockResult_t entry {};
             entry.init_portal(inside_list_result.entries[i].block, inside_list_result.entries[i].collided_pos,
                               collided_pos_delta, inside_list_result.entries[i].collided_overlap,
                               portal_rotations, associated_found_block->src_portal, associated_found_block->dst_portal);
             result.insert(&entry);
         }
     }

     return result;
}

Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap){
     return block_against_solid_tile(block_get_position(block_to_check), block_get_pos_delta(block_to_check),
                                     block_get_cut(block_to_check), direction, tilemap);
}

Tile_t* block_against_solid_tile(Position_t block_pos, Vec_t pos_delta, BlockCut_t cut, Direction_t direction, TileMap_t* tilemap){
     Pixel_t pixel_a {};
     Pixel_t pixel_b {};

     if(!block_adjacent_pixels_to_check(block_pos, pos_delta, cut, direction, &pixel_a, &pixel_b)){
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

bool block_diagonally_against_solid(Position_t block_pos, Vec_t pos_delta, BlockCut_t cut, Direction_t horizontal_direction,
                                    Direction_t vertical_direction, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt){
     Pixel_t pixel_to_check {};
     Position_t final_pos = block_pos + pos_delta;

     if(horizontal_direction == DIRECTION_LEFT){
          if(vertical_direction == DIRECTION_DOWN){
               pixel_to_check = final_pos.pixel + Pixel_t{-1, -1};
          }else if(vertical_direction == DIRECTION_UP){
               pixel_to_check = block_top_left_pixel(final_pos.pixel, cut) + Pixel_t{-1, 1};
          }
     }else if(horizontal_direction == DIRECTION_RIGHT){
          if(vertical_direction == DIRECTION_DOWN){
               pixel_to_check = block_bottom_right_pixel(final_pos.pixel, cut) + Pixel_t{1, -1};
          }else if(vertical_direction == DIRECTION_UP){
               pixel_to_check = block_top_right_pixel(final_pos.pixel, cut) + Pixel_t{1, 1};
          }
     }

     Coord_t coord_to_check = pixel_to_coord(pixel_to_check);

     if(tilemap_is_solid(tilemap, coord_to_check)){
          return true;
     }

     if(quad_tree_interactive_solid_at(interactive_qt, tilemap, coord_to_check, final_pos.z)){
          return true;
     }

     return false;
}

Player_t* block_against_player(Block_t* block, Direction_t direction, ObjectArray_t<Player_t>* players){
     auto block_rect = block_get_inclusive_rect(block);

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

InteractiveHeldResult_t block_held_up_by_popup(Position_t block_pos, BlockCut_t cut, QuadTreeNode_t<Interactive_t>* interactive_qt, S16 min_area){
     InteractiveHeldResult_t result;
     auto block_rect = block_get_inclusive_rect(block_pos.pixel, cut);
     Coord_t rect_coords[4];
     get_rect_coords(block_rect, rect_coords);
     for(S8 i = 0; i < 4; i++){
          auto* interactive = quad_tree_interactive_find_at(interactive_qt, rect_coords[i]);
          if(interactive && interactive->type == INTERACTIVE_TYPE_POPUP){
               if(block_pos.z == (interactive->popup.lift.ticks - 1)){
                    // TODO: again, not kewl using block_get_inclusive_rect() for this, utils has stuff for this
                    auto popup_rect = block_get_inclusive_rect(coord_to_pixel(rect_coords[i]), BLOCK_CUT_WHOLE);
                    auto intserection_area = rect_intersecting_area(block_rect, popup_rect);
                    if(intserection_area >= min_area){
                         add_interactive_held(&result, interactive, popup_rect);
                    }
               }
          }
     }

     return result;
}

static BlockHeldResult_t block_at_height_in_block_rect(Pixel_t block_to_check_pixel, BlockCut_t cut, QuadTreeNode_t<Block_t>* block_qt,
                                                       S8 expected_height, QuadTreeNode_t<Interactive_t>* interactive_qt,
                                                       TileMap_t* tilemap, S16 min_area = 0, bool include_pos_delta = true){
     BlockHeldResult_t result;

     // TODO: need more complicated function to detect this
     auto block_to_check_center = block_center_pixel(block_to_check_pixel, cut);
     Rect_t check_rect = block_get_inclusive_rect(block_to_check_pixel, cut);
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_to_check_center);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          if(block->pos.z != expected_height) continue;
          auto block_pos = block->pos;
          if(include_pos_delta) block_pos += block->pos_delta;

          BlockCut_t block_cut = block_get_cut(block);

          if(pixel_in_rect(block_pos.pixel, check_rect) ||
             pixel_in_rect(block_top_left_pixel(block_pos.pixel, block_cut), check_rect) ||
             pixel_in_rect(block_top_right_pixel(block_pos.pixel, block_cut), check_rect) ||
             pixel_in_rect(block_bottom_right_pixel(block_pos.pixel, block_cut), check_rect)){
               auto block_rect = block_get_inclusive_rect(block_pos.pixel, block_cut);
               auto intserection_area = rect_intersecting_area(block_rect, check_rect);
               if(intserection_area >= min_area){
                    add_block_held(&result, block, block_rect);
               }
          }
     }

     auto block_to_check_coord = pixel_to_coord(block_to_check_center);

     auto found_blocks = find_blocks_through_portals(block_to_check_coord, tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;

         if(found_block->position.z != expected_height) continue;

         if(pixel_in_rect(found_block->position.pixel, check_rect) ||
            pixel_in_rect(block_top_left_pixel(found_block->position.pixel, found_block->rotated_cut), check_rect) ||
            pixel_in_rect(block_top_right_pixel(found_block->position.pixel, found_block->rotated_cut), check_rect) ||
            pixel_in_rect(block_bottom_right_pixel(found_block->position.pixel, found_block->rotated_cut), check_rect)){
              auto block_rect = block_get_inclusive_rect(found_block->position.pixel, found_block->rotated_cut);
              auto intserection_area = rect_intersecting_area(block_rect, check_rect);
              if(intserection_area >= min_area){
                   add_block_held(&result, found_block->block, block_rect);
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
          return block_at_height_in_block_rect(final_pos.pixel, block->teleport_cut, block_qt,
                                               block->teleport_pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     auto final_pos = block->pos + block->pos_delta;
     final_pos.pixel.x = passes_over_pixel(block->pos.pixel.x, final_pos.pixel.x);
     final_pos.pixel.y = passes_over_pixel(block->pos.pixel.y, final_pos.pixel.y);
     return block_at_height_in_block_rect(final_pos.pixel, block->cut, block_qt,
                                          block->pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

BlockHeldResult_t block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area){
     if(block->teleport){
          auto final_pos = block->teleport_pos + block->teleport_pos_delta;
          final_pos.pixel.x = passes_over_pixel(block->teleport_pos.pixel.x, final_pos.pixel.x);
          final_pos.pixel.y = passes_over_pixel(block->teleport_pos.pixel.y, final_pos.pixel.y);
          return block_at_height_in_block_rect(final_pos.pixel, block->teleport_cut, block_qt,
                                               block->teleport_pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
     }

     auto final_pos = block->pos + block->pos_delta;
     final_pos.pixel.x = passes_over_pixel(block->pos.pixel.x, final_pos.pixel.x);
     final_pos.pixel.y = passes_over_pixel(block->pos.pixel.y, final_pos.pixel.y);
     return block_at_height_in_block_rect(final_pos.pixel, block->cut, block_qt,
                                          block->pos.z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area);
}

BlockHeldResult_t block_held_down_by_another_block(Pixel_t block_pixel, S8 block_z, BlockCut_t cut, QuadTreeNode_t<Block_t>* block_qt,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap,
                                                   S16 min_area, bool include_pos_delta){
     return block_at_height_in_block_rect(block_pixel, cut, block_qt, block_z + HEIGHT_INTERVAL, interactive_qt, tilemap, min_area, include_pos_delta);
}

bool block_on_ice(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                  QuadTreeNode_t<Block_t>* block_qt){
     auto block_pos = pos + pos_delta;

     Pixel_t pixel_to_check = block_get_center(block_pos, cut).pixel;
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
          }else if(interactive->type == INTERACTIVE_TYPE_PIT && interactive->pit.iced){
               return pos.z == -HEIGHT_INTERVAL;
          }
     }

     auto rect_to_check = rect_surrounding_adjacent_coords(coord_to_check);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, rect_to_check, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          auto block = blocks[i];
          auto block_rect = block_get_inclusive_rect(block);

          if(block->pos.z + HEIGHT_INTERVAL != pos.z) continue;
          if(!pixel_in_rect(pixel_to_check, block_rect)) continue;

          if(block->element == ELEMENT_ICE || block->element == ELEMENT_ONLY_ICED){
              return true;
          }
     }

     return false;
}

bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     return block_on_ice(block_get_position(block), block_get_pos_delta(block), block_get_cut(block), tilemap, interactive_qt, block_qt);
}

static bool block_held_up_otherwise_on_air(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     auto final_pos = pos + pos_delta;
     auto block_center = block_center_pixel(final_pos, cut);
     auto block_result = block_at_height_in_block_rect(final_pos.pixel, cut, block_qt,
                                                       final_pos.z - HEIGHT_INTERVAL, interactive_qt, tilemap);
     for(S16 i = 0; i < block_result.count; i++){
          if(pixel_in_rect(block_center, block_result.blocks_held[i].rect)) return false;
     }

     auto interactive_result = block_held_up_by_popup(final_pos, cut, interactive_qt);
     for(S16 i = 0; i < interactive_result.count; i++){
          if(pixel_in_rect(block_center, interactive_result.interactives_held[i].rect)) return false;
     }

     return true;
}

bool block_on_air(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     if(pos.z <= 0){
          if(pos.z <= -HEIGHT_INTERVAL) return false;

          Pixel_t pixel_to_check = block_get_center(pos, cut).pixel;
          Coord_t coord_to_check = pixel_to_coord(pixel_to_check);

          Interactive_t* interactive = quad_tree_interactive_find_at(interactive_qt, coord_to_check);
          if(interactive && interactive->type == INTERACTIVE_TYPE_PIT){
               return block_held_up_otherwise_on_air(pos, pos_delta, cut, tilemap, interactive_qt, block_qt);
          }

          if(pos.z == 0) return false;
     }

     return block_held_up_otherwise_on_air(pos, pos_delta, cut, tilemap, interactive_qt, block_qt);
}

bool block_on_air(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     return block_on_air(block_get_position(block), block_get_pos_delta(block), block_get_cut(block), tilemap, interactive_qt, block_qt);
}

void handle_block_on_block_action_horizontal(Position_t block_pos, Vec_t block_pos_delta, Direction_t direction, Position_t collided_block_center, DirectionMask_t collided_block_move_mask,
                                             bool inside_block_on_frictionless, bool both_frictionless, Pixel_t collision_offset,
                                             BlockInsideBlockResult_t* inside_block_entry, CheckBlockCollisionResult_t* result){
     if(direction_in_mask(collided_block_move_mask, direction) ||
        direction_in_mask(collided_block_move_mask, direction_opposite(direction))){
          if(direction_in_mask(collided_block_move_mask, direction)){
               Position_t final_stop_pos = collided_block_center + collision_offset;
               auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
               result->pos_delta.x = new_pos_delta.x;

              if(!inside_block_on_frictionless){
                  Vec_t collided_block_vel = vec_rotate_quadrants_counter_clockwise(inside_block_entry->block->vel, inside_block_entry->portal_rotations);
                  result->vel.x = collided_block_vel.x;
                  if(result->vel.x == 0) result->stop_horizontally();
              }
          }else{
             F32 total_pos_delta = fabs(block_pos_delta.x) + fabs(inside_block_entry->collision_pos_delta.x);
             F32 inside_block_overlap_ratio = block_pos_delta.x / total_pos_delta;

             Position_t final_stop_pos = block_pos + block_pos_delta - Vec_t{inside_block_overlap_ratio * inside_block_entry->collision_overlap.x, 0};

             if(!inside_block_on_frictionless){
                  result->stop_horizontally();
                  result->stop_on_pixel_x = closest_pixel(final_stop_pos.pixel.x, final_stop_pos.decimal.x);
                  final_stop_pos.pixel.x = result->stop_on_pixel_x;
                  final_stop_pos.decimal.x = 0;
                  auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                  result->pos_delta.x = new_pos_delta.x;
             }else{
                  auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                  result->pos_delta.x = new_pos_delta.x;
             }
          }
     }else{
          Position_t final_stop_pos;
          if(!inside_block_on_frictionless && collided_block_center.decimal.x == 0){
              // if the block is grid aligned let's keep it that way
              final_stop_pos = pixel_pos(collided_block_center.pixel + collision_offset);
              result->stop_on_pixel_x = final_stop_pos.pixel.x;
          }else{
              final_stop_pos = collided_block_center + collision_offset;
          }

          Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

          result->pos_delta.x = pos_delta.x;
          if(!both_frictionless) result->stop_horizontally();
     }
}

void handle_block_on_block_action_vertical(Position_t block_pos, Vec_t block_pos_delta, Direction_t direction, Position_t collided_block_center, DirectionMask_t collided_block_move_mask,
                                           bool inside_block_on_frictionless, bool both_frictionless, Pixel_t collision_offset,
                                           BlockInsideBlockResult_t* inside_block_entry, CheckBlockCollisionResult_t* result){
     if(direction_in_mask(collided_block_move_mask, direction) ||
        direction_in_mask(collided_block_move_mask, direction_opposite(direction))){

          if(direction_in_mask(collided_block_move_mask, direction)){
              Position_t final_stop_pos = collided_block_center + collision_offset;
              auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
              result->pos_delta.y = new_pos_delta.y;

              if(!inside_block_on_frictionless){
                  Vec_t collided_block_vel = vec_rotate_quadrants_counter_clockwise(inside_block_entry->block->vel, inside_block_entry->portal_rotations);
                  result->vel.y = collided_block_vel.y;
                  if(result->vel.y == 0) result->stop_vertically();
              }
          }else{
             F32 total_pos_delta = fabs(block_pos_delta.y) + fabs(inside_block_entry->collision_pos_delta.y);
             F32 inside_block_overlap_ratio = block_pos_delta.y / total_pos_delta;

             Position_t final_stop_pos = block_pos + block_pos_delta - Vec_t{0, inside_block_overlap_ratio * inside_block_entry->collision_overlap.y};

             if(!inside_block_on_frictionless){
                  result->stop_vertically();
                  result->stop_on_pixel_y = closest_pixel(final_stop_pos.pixel.y, final_stop_pos.decimal.y);
                  final_stop_pos.pixel.y = result->stop_on_pixel_y;
                  final_stop_pos.decimal.y = 0;
                  auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                  result->pos_delta.y = new_pos_delta.y;
             }else{
                  auto new_pos_delta = pos_to_vec(final_stop_pos - block_pos);
                  result->pos_delta.y = new_pos_delta.y;
             }
          }
     }else{
          Position_t final_stop_pos;
          if(!inside_block_on_frictionless && collided_block_center.decimal.y == 0){
              // if the block is grid aligned let's keep it that way
              final_stop_pos = pixel_pos(collided_block_center.pixel + collision_offset);
              result->stop_on_pixel_y = final_stop_pos.pixel.y;
          }else{
              final_stop_pos = collided_block_center + collision_offset;
          }

          Vec_t pos_delta = pos_to_vec(final_stop_pos - block_pos);

          result->pos_delta.y = pos_delta.y;
          if(!both_frictionless) result->stop_vertically();
     }
}

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, BlockCut_t cut, S16 block_stop_on_pixel_x,
                                                                    S16 block_stop_on_pixel_y, Move_t block_horizontal_move,
                                                                    Move_t block_vertical_move, bool stopped_by_player_horizontal,
                                                                    bool stopped_by_player_vertical, S16 block_index,
                                                                    bool block_is_cloning, World_t* world){
     CheckBlockCollisionResult_t result {};

     result.block_index = block_index;
     result.same_as_next = false;
     result.pos_delta = block_pos_delta;
     result.original_vel = block_vel;
     result.vel = block_vel;
     result.accel = block_accel;

     result.stop_on_pixel_x = block_stop_on_pixel_x;
     result.stop_on_pixel_y = block_stop_on_pixel_y;

     result.stopped_by_player_horizontal = stopped_by_player_horizontal;
     result.stopped_by_player_vertical = stopped_by_player_vertical;

     result.horizontal_move = block_horizontal_move;
     result.vertical_move = block_vertical_move;
     result.collided_block_index = -1;

     auto block_inside_result = block_inside_others(block_pos,
                                                    result.pos_delta,
                                                    cut,
                                                    block_index,
                                                    block_is_cloning,
                                                    world->block_qt,
                                                    world->interactive_qt,
                                                    &world->tilemap,
                                                    &world->blocks);

     if(block_inside_result.count > 0 ){
          S16 collided_with_blocks_on_ice = 0;

          // TODO: this code might go away with our restructure
          if(block_on_frictionless(block_pos, block_pos_delta, cut, &world->tilemap, world->interactive_qt, world->block_qt)){
               // calculate the momentum if we are on ice for later
               for(S8 i = 0; i < block_inside_result.count; i++){
                    BlockInsideBlockResult_t* inside_entry = block_inside_result.objects + i;

                    // find the closest pixel in the collision rect to our block
                    auto collided_block_index = get_block_index(world, inside_entry->block);
                    auto collision_center_pixel = inside_entry->collision_pos.pixel;
                    auto collided_block_cut = block_get_cut(inside_entry->block);
                    Rect_t collision_rect;

                    auto center_offset = block_center_pixel_offset(collided_block_cut);

                    collision_rect.left = collision_center_pixel.x - center_offset.x;
                    collision_rect.bottom = collision_center_pixel.y - center_offset.y;
                    collision_rect.right = collision_center_pixel.x + center_offset.x;
                    collision_rect.top = collision_center_pixel.y + center_offset.y;

                    auto block_rect = block_get_inclusive_rect(block_pos.pixel, cut);

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
                                                                 inside_entry->collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   inside_entry->invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_RIGHT){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{-1, 0},
                                                                 inside_entry->collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   inside_entry->invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_DOWN){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{0, 1},
                                                                 inside_entry->collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   inside_entry->invalidated = true;
                              }
                         }
                    }

                    if(direction_to_check_mask & DIRECTION_MASK_UP){
                         auto* inside_block = pixel_inside_block(closest_pixel + Pixel_t{0, -1},
                                                                 inside_entry->collision_pos.z,
                                                                 &world->tilemap, world->interactive_qt, world->block_qt);
                         if(inside_block){
                              auto inside_block_index = get_block_index(world, inside_block);
                              if(inside_block_index != collided_block_index && inside_block_index != block_index){
                                   inside_entry->invalidated = true;
                              }
                         }
                    }
               }

               // for valid collisions calculate how many blocks we collided with
               for(S8 i = 0; i < block_inside_result.count; i++){
                    BlockInsideBlockResult_t* inside_entry = block_inside_result.objects + i;
                    if(inside_entry->invalidated) continue;

                    auto inside_block_pos = block_get_position(inside_entry->block);
                    auto inside_block_pos_delta = block_get_pos_delta(inside_entry->block);
                    auto inside_block_cut = block_get_cut(inside_entry->block);

                    if(block_on_frictionless(inside_block_pos, inside_block_pos_delta,
                                             inside_block_cut, &world->tilemap, world->interactive_qt, world->block_qt)){
                         collided_with_blocks_on_ice++;
                    }
               }
          }

          result.collided_distance = FLT_MAX;

          // if(block_inside_result.count > 0){
          //      LOG("block %d inside %d blocks\n", block_index, block_inside_result.count);
          // }

          for(S8 i = 0; i < block_inside_result.count; i++){
               BlockInsideBlockResult_t* inside_entry = block_inside_result.objects + i;
               if(inside_entry->invalidated) continue;

               auto moved_block_pos = block_get_center(block_pos, cut);
               auto move_direction = move_direction_between(moved_block_pos, inside_entry->collision_pos);
               Direction_t first_direction;
               Direction_t second_direction;
               move_direction_to_directions(move_direction, &first_direction, &second_direction);

               auto collided_dir_mask = direction_mask_add(direction_to_direction_mask(first_direction),
                                                             direction_to_direction_mask(second_direction));


               BlockCollidedWithBlock_t collided_with {};
               collided_with.block_index = get_block_index(world, inside_entry->block);
               collided_with.direction_mask = collided_dir_mask;
               collided_with.portal_rotations = inside_entry->portal_rotations;
               result.collided_with_blocks.insert(&collided_with);

               F32 collision_distance = vec_magnitude(pos_to_vec(inside_entry->collision_pos - block_pos));
               if(collision_distance > result.collided_distance) continue;

               S16 block_inside_index = -1;
               if(inside_entry->block){
                   block_inside_index = get_block_index(world, inside_entry->block);
               }

               Position_t inside_block_pos = block_get_position(inside_entry->block);
               Vec_t inside_block_pos_delta = block_get_pos_delta(inside_entry->block);
               BlockCut_t inside_block_cut = block_get_cut(inside_entry->block);

               // check if they are on a frictionless surface before
               bool a_on_frictionless = block_on_frictionless(block_pos, result.pos_delta, cut, &world->tilemap, world->interactive_qt, world->block_qt);
               bool b_on_frictionless = block_on_frictionless(inside_block_pos, inside_block_pos_delta,
                                                              inside_block_cut, &world->tilemap, world->interactive_qt, world->block_qt);
               bool both_frictionless = a_on_frictionless && b_on_frictionless;

               // TODO: handle multiple block indices
               result.collided = true;
               result.collided_distance = collision_distance;
               result.collided_block_index = get_block_index(world, inside_entry->block);
               result.collided_pos = inside_entry->collision_pos;
               result.collided_portal_rotations = inside_entry->portal_rotations;

               auto collided_block_center = inside_entry->collision_pos;
               Block_t* collided_block = inside_entry->block;
               auto collided_block_pos = block_get_position(collided_block);
               auto collided_block_pos_delta = block_get_pos_delta(collided_block);
               auto collided_block_cut = block_get_cut(collided_block);

               // update collided block center if the block is stopping on a pixel this frame
               // TODO: account for stop_on_pixel through a portal
               if(inside_entry->block->stop_on_pixel_x){
                    auto offset_helper = (inside_entry->collision_pos - (collided_block_pos + collided_block_pos_delta)).pixel.x;
                    auto portal_pixel_offset = offset_helper - block_center_pixel_offset(collided_block_cut).x;
                    collided_block_center.pixel.x = collided_block->stop_on_pixel_x + block_center_pixel_offset(collided_block_cut).x + portal_pixel_offset;
                    collided_block_center.decimal.x = 0;
               }

               if(inside_entry->block->stop_on_pixel_y){
                    auto offset_helper = (inside_entry->collision_pos - (collided_block_pos + collided_block_pos_delta)).pixel.y;
                    auto portal_pixel_offset = offset_helper - block_center_pixel_offset(collided_block_cut).y;
                    collided_block_center.pixel.y = collided_block->stop_on_pixel_y + block_center_pixel_offset(collided_block_cut).y + portal_pixel_offset;
                    collided_block_center.decimal.y = 0;
               }

               result.collided_dir_mask = collided_dir_mask;

               auto collided_block_move_mask = vec_direction_mask(inside_block_pos_delta);

               if(blocks_are_entangled(result.collided_block_index, block_index, &world->blocks)){
                    auto* block = world->blocks.elements + block_index;
                    auto* entangled_block = world->blocks.elements + result.collided_block_index;
                    auto final_block_pos = block_get_final_position(block);
                    auto block_cut = block_get_cut(block);
                    final_block_pos.pixel += block_center_pixel_offset(block_cut);
                    auto pos_diff = pos_to_vec(result.collided_pos - final_block_pos);

                    auto pos_dimension_delta = fabs(fabs(pos_diff.x) - fabs(pos_diff.y));

                    // if positions are diagonal to each other and the rotation between them is odd, check if we are moving into each other
                    if(pos_dimension_delta <= DISTANCE_EPSILON && (block->rotation + entangled_block->rotation + result.collided_portal_rotations) % 2 == 1){
                         // just gtfo if this happens, we handle this case outside this function
                         bool inside_block_on_frictionless = block_on_frictionless(collided_block_pos, collided_block_pos_delta, collided_block_cut,
                                                                                   &world->tilemap, world->interactive_qt, world->block_qt);

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
                                   if(block_pos_delta.x < 0){
                                        handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                                collided_block_center, collided_block_move_mask,
                                                                                inside_block_on_frictionless, both_frictionless,
                                                                                Pixel_t{block_center_pixel_offset(collided_block_cut).x, 0},
                                                                                block_inside_result.objects + i, &result);
                                   }
                                   break;
                              case DIRECTION_RIGHT:
                                   if(block_pos_delta.x > 0){
                                        S16 offset = -(block_center_pixel_offset(collided_block_cut).x + block_get_width_in_pixels(cut));
                                        handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                                collided_block_center, collided_block_move_mask,
                                                                                inside_block_on_frictionless, both_frictionless,
                                                                                Pixel_t{offset, 0}, block_inside_result.objects + i, &result);
                                   }
                                   break;
                              }

                              switch(second_direction){
                              default:
                                   break;
                              case DIRECTION_DOWN:
                                   if(block_pos_delta.y < 0){
                                        handle_block_on_block_action_vertical(block_pos, block_pos_delta, second_direction,
                                                                              collided_block_center, collided_block_move_mask,
                                                                              inside_block_on_frictionless, both_frictionless,
                                                                              Pixel_t{0, block_center_pixel_offset(collided_block_cut).y},
                                                                              block_inside_result.objects + i, &result);
                                   }
                                   break;
                              case DIRECTION_UP:
                                   if(block_pos_delta.y > 0){
                                        S16 offset = -(block_center_pixel_offset(collided_block_cut).y + block_get_height_in_pixels(cut));
                                        handle_block_on_block_action_vertical(block_pos, block_pos_delta, second_direction,
                                                                              collided_block_center, collided_block_move_mask,
                                                                              inside_block_on_frictionless, both_frictionless,
                                                                              Pixel_t{0, offset}, block_inside_result.objects + i, &result);
                                   }
                                   break;
                              }
                         } break;
                         case MOVE_DIRECTION_LEFT:
                              if(block_pos_delta.x < 0){
                                   handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                           collided_block_center, collided_block_move_mask,
                                                                           inside_block_on_frictionless, both_frictionless,
                                                                           Pixel_t{block_center_pixel_offset(collided_block_cut).x, 0},
                                                                           block_inside_result.objects + i, &result);
                              }
                              break;
                         case MOVE_DIRECTION_RIGHT:
                              if(block_pos_delta.x > 0){
                                   S16 offset = -(block_center_pixel_offset(collided_block_cut).x + block_get_width_in_pixels(cut));
                                   handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                           collided_block_center, collided_block_move_mask,
                                                                           inside_block_on_frictionless, both_frictionless,
                                                                           Pixel_t{offset, 0}, block_inside_result.objects + i, &result);
                              }
                              break;
                         case MOVE_DIRECTION_DOWN:
                              if(block_pos_delta.y < 0){
                                   handle_block_on_block_action_vertical(block_pos, block_pos_delta, first_direction,
                                                                         collided_block_center, collided_block_move_mask,
                                                                         inside_block_on_frictionless, both_frictionless,
                                                                         Pixel_t{0, block_center_pixel_offset(collided_block_cut).y},
                                                                         block_inside_result.objects + i, &result);
                              }
                              break;
                         case MOVE_DIRECTION_UP:
                              if(block_pos_delta.y > 0){
                                   S16 offset = -(block_center_pixel_offset(collided_block_cut).y + block_get_height_in_pixels(cut));
                                   handle_block_on_block_action_vertical(block_pos, block_pos_delta, first_direction,
                                                                         collided_block_center, collided_block_move_mask,
                                                                         inside_block_on_frictionless, both_frictionless,
                                                                         Pixel_t{0, offset}, block_inside_result.objects + i, &result);
                              }
                              break;
                         }

                         continue;
                    }

                    // if we are not moving towards the entangled block, it's on them to resolve the collision, so get outta here
                    DirectionMask_t pos_diff_dir_mask = vec_direction_mask(pos_diff);
                    auto pos_delta_dir = vec_direction(block->pos_delta);
                    if(!direction_in_mask(pos_diff_dir_mask, pos_delta_dir)){
                         continue;
                    }
               }

               if(result.collided_portal_rotations){
                    // TODO: make direction_mask_rotate_counter_clockwise()
                    auto total_rotations = DIRECTION_COUNT - result.collided_portal_rotations;
                    collided_block_move_mask = direction_mask_rotate_clockwise(collided_block_move_mask, total_rotations);
               }

               if(block_inside_index != block_index){
                    // TODO: compress with code below, now that we fixed the bug
                    bool inside_block_on_frictionless = block_on_frictionless(collided_block_pos, collided_block_pos_delta, collided_block_cut,
                                                                              &world->tilemap, world->interactive_qt, world->block_qt);

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
                              if(block_pos_delta.x < 0){
                                   handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                           collided_block_center, collided_block_move_mask,
                                                                           inside_block_on_frictionless, both_frictionless,
                                                                           Pixel_t{block_center_pixel_offset(collided_block_cut).x, 0},
                                                                           block_inside_result.objects + i, &result);
                              }
                              break;
                         case DIRECTION_RIGHT:
                              if(block_pos_delta.x > 0){
                                   S16 offset = -(block_center_pixel_offset(collided_block_cut).x + block_get_width_in_pixels(cut));
                                   handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                           collided_block_center, collided_block_move_mask,
                                                                           inside_block_on_frictionless, both_frictionless,
                                                                           Pixel_t{offset, 0}, block_inside_result.objects + i, &result);
                              }
                              break;
                         }

                         switch(second_direction){
                         default:
                              break;
                         case DIRECTION_DOWN:
                              if(block_pos_delta.y < 0){
                                   handle_block_on_block_action_vertical(block_pos, block_pos_delta, second_direction,
                                                                         collided_block_center, collided_block_move_mask,
                                                                         inside_block_on_frictionless, both_frictionless,
                                                                         Pixel_t{0, block_center_pixel_offset(collided_block_cut).y},
                                                                         block_inside_result.objects + i, &result);
                              }
                              break;
                         case DIRECTION_UP:
                              if(block_pos_delta.y > 0){
                                   S16 offset = -(block_center_pixel_offset(collided_block_cut).y + block_get_height_in_pixels(cut));
                                   handle_block_on_block_action_vertical(block_pos, block_pos_delta, second_direction,
                                                                         collided_block_center, collided_block_move_mask,
                                                                         inside_block_on_frictionless, both_frictionless,
                                                                         Pixel_t{0, offset}, block_inside_result.objects + i, &result);
                              }
                              break;
                         }
                    } break;
                    case MOVE_DIRECTION_LEFT:
                         if(block_pos_delta.x < 0){
                              handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                      collided_block_center, collided_block_move_mask,
                                                                      inside_block_on_frictionless, both_frictionless,
                                                                      Pixel_t{block_center_pixel_offset(collided_block_cut).x, 0},
                                                                      block_inside_result.objects + i, &result);
                         }
                         break;
                    case MOVE_DIRECTION_RIGHT:
                         if(block_pos_delta.x > 0){
                              S16 offset = -(block_center_pixel_offset(collided_block_cut).x + block_get_width_in_pixels(cut));
                              handle_block_on_block_action_horizontal(block_pos, block_pos_delta, first_direction,
                                                                      collided_block_center, collided_block_move_mask,
                                                                      inside_block_on_frictionless, both_frictionless,
                                                                      Pixel_t{offset, 0}, block_inside_result.objects + i, &result);
                         }
                         break;
                    case MOVE_DIRECTION_DOWN:
                         if(block_pos_delta.y < 0){
                              handle_block_on_block_action_vertical(block_pos, block_pos_delta, first_direction,
                                                                    collided_block_center, collided_block_move_mask,
                                                                    inside_block_on_frictionless, both_frictionless,
                                                                    Pixel_t{0, block_center_pixel_offset(collided_block_cut).y},
                                                                    block_inside_result.objects + i, &result);
                         }
                         break;
                    case MOVE_DIRECTION_UP:
                         if(block_pos_delta.y > 0){
                              S16 offset = -(block_center_pixel_offset(collided_block_cut).y + block_get_height_in_pixels(cut));
                              handle_block_on_block_action_vertical(block_pos, block_pos_delta, first_direction,
                                                                    collided_block_center, collided_block_move_mask,
                                                                    inside_block_on_frictionless, both_frictionless,
                                                                    Pixel_t{0, offset}, block_inside_result.objects + i, &result);
                         }
                         break;
                    }
               }else{
                    Coord_t block_coord = block_get_coord(block_pos, cut);
                    Direction_t src_portal_dir = direction_between(block_coord, inside_entry->src_portal_coord);
                    Direction_t dst_portal_dir = direction_between(block_coord, inside_entry->dst_portal_coord);
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

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt){
     auto block_coord = block_get_coord(block);
     auto block_rect = block_get_inclusive_rect(block);
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

TransferMomentum_t get_push_pusher_momentum(BlockMomentumPusher_t* pusher, BlockMomentumPush_t* push, Direction_t push_direction,
                                            World_t* world){
     TransferMomentum_t result;

     auto* pusher_block = world->blocks.elements + pusher->index;
     S16 pusher_mass = get_block_stack_mass(world, pusher_block);
     assert(pusher->collided_with_block_count != 0);
     result.mass = (S16)((F32)(pusher_mass) * (1.0f / (F32)(pusher->collided_with_block_count)));

     if(push->entangled_with_push_index >= 0){
          assert(pusher_mass != 0);
          result.vel = push->entangled_momentum / pusher_mass;
          result.vel = rotate_vec_counter_clockwise_to_see_if_negates(result.vel,
                                                                      direction_is_horizontal(push_direction),
                                                                      push->portal_rotations);
     }else{
          S8 total_rotations = (push->portal_rotations + push->entangle_rotations) % DIRECTION_COUNT;
          Vec_t rotated_pusher_vel = vec_rotate_quadrants_clockwise(pusher_block->vel, total_rotations % DIRECTION_COUNT);
          Direction_t rotated_dir = direction_rotate_clockwise(push_direction, total_rotations);

          if(direction_is_horizontal(rotated_dir)){
               result.vel = rotated_pusher_vel.x;
          }else{
               result.vel = rotated_pusher_vel.y;
          }

          if(direction_is_positive(push_direction)){
               if(result.vel < 0) result.vel = -result.vel;
          }else{
               if(result.vel > 0) result.vel = -result.vel;
          }
     }

     return result;
}

TransferMomentum_t get_block_push_pusher_momentum(BlockMomentumPush_t* push, World_t* world, Direction_t push_direction){
     TransferMomentum_t result {0, 0};
     F32 total_momentum = 0;

     for(S16 p = 0; p < push->pusher_count; p++){
          auto pusher_momentum = get_push_pusher_momentum(push->pushers + p, push, push_direction, world);

          // TODO: normalize momentum to a specific mass, maybe just use the first one ?
          total_momentum += (F32)(pusher_momentum.mass) * pusher_momentum.vel;
          result.mass += pusher_momentum.mass;
     }

     result.vel = total_momentum / result.mass;
     return result;
}

BlockCollisionPushResult_t block_collision_push(BlockMomentumPush_t* push, World_t* world){
     BlockCollisionPushResult_t result;

     S8 total_push_rotations = (push->portal_rotations + push->entangle_rotations) % DIRECTION_COUNT;

     auto* pushee = world->blocks.elements + push->pushee_index;

     // rotate by the portal and entangle rotations
     Direction_t push_direction = direction_rotate_clockwise(push->direction, total_push_rotations);

     auto push_pos = block_get_position(pushee);
     auto push_pos_delta = block_get_pos_delta(pushee);

     TransferMomentum_t instant_momentum = get_block_push_pusher_momentum(push, world, push->direction);
     instant_momentum.vel = rotate_vec_clockwise_to_see_if_negates(instant_momentum.vel, direction_is_horizontal(push->direction), total_push_rotations);

     F32 total_push_momentum = instant_momentum.mass * instant_momentum.vel;
     // LOG("total push on block %d momentum: %f, mass: %d, vel: %f\n", push->pushee_index, total_push_momentum, instant_momentum.mass, instant_momentum.vel);

     auto push_result = block_push(pushee, push_pos, push_pos_delta, push_direction, world, true, 1.0f, &instant_momentum,
                                   nullptr, push->collided_with_block_count);

#if 0
     LOG("  push result pushed %d, busy %d collisions %d\n", push_result.pushed, push_result.busy, push_result.collisions.count);
     for(S16 c = 0; c < push_result.collisions.count; c++){
         auto& collision = push_result.collisions.objects[c];

         LOG("    collision pusher %d %f, pushee %d, %f -> %f\n", collision.pusher_mass, collision.pusher_velocity, collision.pushee_mass, collision.pushee_initial_velocity, collision.pushee_velocity);
     }
#endif

     // if the push itself is entangled then we don't care about the results in terms of transferring momentum back or the consequences on the other side of the push

     auto direction_to_check = direction_opposite(push_direction);

     // subtract out entangler pushes from total momentum
     for(S16 p = 0; p < push->pusher_count; p++){
          if(push->pushers[p].hit_entangler){
               auto pusher_momentum = get_push_pusher_momentum(push->pushers + p, push, push_direction, world);
               total_push_momentum -= (F32)(pusher_momentum.mass) * pusher_momentum.vel;
          }
     }
     // LOG("updated total push momentum: %f (after removing entangled pushes)\n", total_push_momentum);

     for(S16 p = 0; p < push->pusher_count; p++){
          auto* pusher = world->blocks.elements + push->pushers[p].index;
          auto local_rotations = total_push_rotations;

          if(push->pushers[p].hit_entangler) continue;

          if(push->is_entangled()){
              local_rotations = 0;
          }

          direction_to_check = direction_rotate_counter_clockwise(direction_to_check, local_rotations);

          auto pusher_direction = direction_opposite(direction_to_check);
          auto block_receiving_force = pusher;

          // go forward in the chain (towards the block we pushed) finding blocks that are going in the same direction as the chain, because we want those to
          // receive the force before the pusher
          Direction_t forward_chain_direction_to_check = pusher_direction;
          auto chain_results = find_block_chain(block_receiving_force, forward_chain_direction_to_check, world->block_qt, world->interactive_qt, &world->tilemap, 0, nullptr);
          // TODO: handle across multiple chains
          if(chain_results.count > 0 && chain_results.objects[0].count > 0){
               // skip the first block, which is our pusher, and do not go all the way to the end of the chain,
               // because that block is the one we applied our push to
               // LOG("searching %s (forward) started at block %ld seeing chain %d blocks long\n", direction_to_string(forward_chain_direction_to_check),
                   // block_receiving_force - world->blocks.elements, chain_results.objects[0].count);
               for(S16 i = 1; i < chain_results.objects[0].count; i++){
                    auto* against_block_result = chain_results.objects[0].objects + i;
                    auto new_direction_to_check = direction_rotate_clockwise(forward_chain_direction_to_check,
                                                                             against_block_result->rotations_through_portal);

                    // use the previous block if this one's velocity is no longer moving towards the stack
                    // do not use current frame's momentum like the next loop uses because we want to only go forward for blocks
                    // causing the collisions
                    if(push->pushers[p].momentum_kicked_back_in_shared_push_block_index < 0){
                         Direction_t against_block_move_dir = block_axis_move(against_block_result->block, direction_is_horizontal(forward_chain_direction_to_check));
                         against_block_move_dir = direction_rotate_clockwise(against_block_move_dir, against_block_result->rotations_through_portal);
                         if(against_block_move_dir != forward_chain_direction_to_check){
                              break;
                         }
                    }else{
                         if((block_receiving_force - world->blocks.elements) == push->pushers[p].momentum_kicked_back_in_shared_push_block_index){
                              break;
                         }
                    }

                    block_receiving_force = against_block_result->block;
                    forward_chain_direction_to_check = new_direction_to_check;
                    // LOG("  updating block receiving force to be %ld\n", block_receiving_force - world->blocks.elements);
               }
          // }else{
               // LOG("empty chain to search %s (forwards) from block %ld\n", direction_to_string(direction_to_check), block_receiving_force - world->blocks.elements);
          }

          // update direction to check (say for instance if we went forward maybe through a rotated portal)
          direction_to_check = direction_opposite(forward_chain_direction_to_check);

          // search backward in the chain to figure out which block absorbs the momentum kickback
          chain_results = find_block_chain(block_receiving_force, direction_to_check, world->block_qt, world->interactive_qt, &world->tilemap, 0, nullptr);
          // TODO: handle across multiple chains
          if(chain_results.count > 0 && chain_results.objects[0].count > 0){
               // LOG("searching %s (backward) started at block %ld seeing chain %d blocks long\n", direction_to_string(direction_to_check),
               //     block_receiving_force - world->blocks.elements, chain_results.objects[0].count);
               for(S16 i = 0; i < chain_results.objects[0].count; i++){
                    auto* against_block_result = chain_results.objects[0].objects + i;

                    // if the relevant vel is 0, then the most recent connected block is the one that can absorb the energy of the collision
                    // this happens when a block falls off of another block (into a slot) and collides with another block all at the same time
                    auto new_direction_to_check = direction_rotate_clockwise(direction_to_check,
                                                                             against_block_result->rotations_through_portal);
                    if(push->pushers[p].momentum_kicked_back_in_shared_push_block_index < 0){
                         bool horizontal_direction = direction_is_horizontal(new_direction_to_check);
                         F32 vel = horizontal_direction ? against_block_result->block->vel.x : against_block_result->block->vel.y;
                         if(vel == 0) break;

                         // use the previous block if this one's momentum is no longer moving towards the stack
                         Vec_t against_block_vel = get_block_momentum_vel(world, against_block_result->block);
                         DirectionMask_t against_block_move_mask = vec_direction_mask(against_block_vel);
                         against_block_move_mask = direction_mask_rotate_clockwise(against_block_move_mask, against_block_result->rotations_through_portal);
                         if(!direction_in_mask(against_block_move_mask, direction_opposite(direction_to_check))){
                              break;
                         }
                    }else{
                         if((block_receiving_force - world->blocks.elements) == push->pushers[p].momentum_kicked_back_in_shared_push_block_index){
                              // LOG("overriding block receiving force to be: %ld\n", block_receiving_force - world->blocks.elements);
                              break;
                         }
                    }

                    block_receiving_force = against_block_result->block;
                    direction_to_check = new_direction_to_check;
                    // LOG("  updating block receiving force to be %ld\n", block_receiving_force - world->blocks.elements);
               }
          // }else{
               // LOG("empty chain to search %s (backwards) from block %ld\n", direction_to_string(direction_to_check), block_receiving_force - world->blocks.elements);
          }

          S16 block_receiving_force_index = get_block_index(world, block_receiving_force);

          if(push_result.collisions.count == 0 && !push_result.pushed){
              if(direction_is_horizontal(direction_to_check)){
                   // LOG("stopping block %d horizontally via kickback momentum\n", block_receiving_force_index);
                   block_receiving_force->horizontal_momentum = BLOCK_MOMENTUM_STOP;
              }else{
                   // LOG("stopping block %d vertically via kickback momentum\n", block_receiving_force_index);
                   block_receiving_force->vertical_momentum = BLOCK_MOMENTUM_STOP;
              }
              continue;
          }

          // get the momentum in the current direction for the block receiving the force
          TransferMomentum_t block_receiving_force_momentum = {};

          if(block_receiving_force == pusher && push->entangled_with_push_index >= 0){
               block_receiving_force_momentum = get_push_pusher_momentum(push->pushers + p, push, push_direction, world);
          }else{
               block_receiving_force_momentum = get_block_momentum(world, block_receiving_force, direction_opposite(direction_to_check));
               block_receiving_force_momentum.mass /= push->pushers[p].collided_with_block_count;
          }

          // we fabs() because we know the momentum is travelling in the same direction as the total momentum, it
          // just may be rotated due to a portal.
          F32 pusher_contribution_percentage = fabs(((F32)(block_receiving_force_momentum.mass) * block_receiving_force_momentum.vel) / total_push_momentum);
          // LOG("  pusher %d contribution: %f mass: %d, vel: %f\n", push->pushers[p].index, pusher_contribution_percentage, block_receiving_force_momentum.mass, block_receiving_force_momentum.vel);

          for(S16 c = 0; c < push_result.collisions.count; c++){
              auto& collision = push_result.collisions.objects[c];

              // if the block pushed is the block receiving the force, and this is a pure_entangle, then do not kickback momentum
              if(push->pure_entangle && collision.pushee_index == block_receiving_force_index &&
                 push->pushee_index == collision.pushee_index){
                   continue;
              }

              Direction_t intial_kickback_direction = direction_opposite(collision.direction_pushee_hit);
              S8 rotations_in_push = direction_rotations_between(intial_kickback_direction, direction_to_check);

              F32 rotated_pushee_vel = rotate_vec_counter_clockwise_to_see_if_negates(collision.pushee_initial_velocity,
                                                                                      direction_is_horizontal(intial_kickback_direction),
                                                                                      rotations_in_push);

              // LOG("col initial vel: %f, kickback direction on hit: %s, final kickback direction: %s, rotations since push: %d, first stage vel: %f\n", collision.pushee_initial_velocity,
              //     direction_to_string(intial_kickback_direction), direction_to_string(direction_to_check), rotations_in_push, rotated_pushee_vel);

              S16 mass = block_receiving_force_momentum.mass / push_result.collisions.count;
              // LOG("elastic collision for block %d: %d %f -> %f %f\n", block_receiving_force_index, mass, block_receiving_force_momentum.vel,
              //                                                         collision.pushee_mass * pusher_contribution_percentage,
              //                                                         rotated_pushee_vel);
              auto elastic_result = elastic_transfer_momentum(mass, block_receiving_force_momentum.vel,
                                                              collision.pushee_mass * pusher_contribution_percentage,
                                                              rotated_pushee_vel);

              // if our vel doesn't change, don't add more momentum
              if(fabs(block_receiving_force_momentum.vel - elastic_result.first_final_velocity) >= FLT_EPSILON){
                   if(direction_is_horizontal(direction_to_check)){
                        // LOG("giving block %d: %f horizontal kickback momentum\n", block_receiving_force_index, (F32)(mass) * elastic_result.first_final_velocity);
                        block_add_horizontal_momentum(block_receiving_force, BLOCK_MOMENTUM_TYPE_KICKBACK, (F32)(mass) * elastic_result.first_final_velocity);
                   }else{
                        // LOG("giving block %d: %f vertical kickback momentum\n", block_receiving_force_index, (F32)(mass) * elastic_result.first_final_velocity);
                        block_add_vertical_momentum(block_receiving_force, BLOCK_MOMENTUM_TYPE_KICKBACK, (F32)(mass) * elastic_result.first_final_velocity);
                   }
              }
          }

          // store the block receiving the force
          if(push_result.collisions.count > 0){
               push->pushers[p].momentum_kicked_back_in_shared_push_block_index = block_receiving_force - world->blocks.elements;
          }
     }

     return result;
}

FindBlocksThroughPortalResult_t find_blocks_through_portals(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt,
                                                            bool require_on){
     FindBlocksThroughPortalResult_t result;

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

     Coord_t surrounding_coords[SURROUNDING_COORD_COUNT];
     coords_surrounding(surrounding_coords, SURROUNDING_COORD_COUNT, coord);

     for(S8 c = 0; c < SURROUNDING_COORD_COUNT; c++){
          Coord_t check_coord = surrounding_coords[c];
          auto portal_src_pixel = coord_to_pixel_at_center(check_coord);
          auto interactive = quad_tree_interactive_find_at(interactive_qt, check_coord);

          if(require_on){
               if(!is_active_portal(interactive)) continue;
          }else{
               if(!(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL)) continue;
          }

          PortalExit_t portal_exits = find_portal_exits(check_coord, tilemap, interactive_qt, require_on);

          for(S8 d = 0; d < DIRECTION_COUNT; d++){
               Direction_t current_portal_dir = (Direction_t)(d);
               auto portal_exit = portal_exits.directions + d;

               for(S8 p = 0; p < portal_exit->count; p++){
                    auto portal_dst_coord = portal_exit->coords[p];
                    if(portal_dst_coord == check_coord) continue;

                    Coord_t portal_dst_output_coord = portal_dst_coord + direction_opposite(current_portal_dir);

                    auto check_portal_rect = rect_surrounding_adjacent_coords(portal_dst_coord);
                    quad_tree_find_in(block_qt, check_portal_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    auto portal_dst_output_center = pixel_to_pos(coord_to_pixel_at_center(portal_dst_output_coord));
                    sort_blocks_by_ascending_height(blocks, block_count);

                    auto rotations_between_portals = portal_rotations_between(interactive->portal.face, current_portal_dir);
                    auto compatibility_rot = portal_rotations_between(current_portal_dir, interactive->portal.face);

                    for(S8 b = 0; b < block_count; b++){
                         Block_t* block = blocks[b];

                         auto portal_rotations = direction_rotations_between(interactive->portal.face, direction_opposite(current_portal_dir));

                         auto block_real_pos = block_get_final_position(block);
                         auto block_cut = block_get_cut(block);
                         auto block_center = block_get_center(block_real_pos, block_cut);
                         auto dst_offset = block_center - portal_dst_output_center;
                         auto src_coord_center = pixel_to_pos(portal_src_pixel);
                         auto block_final_pos = src_coord_center + position_rotate_quadrants_clockwise(dst_offset, compatibility_rot);
                         auto rotated_cut = block_cut_rotate_clockwise(block_cut, portal_rotations);

                         block_final_pos.pixel -= block_center_pixel_offset(rotated_cut);

                         bool duplicate = false;
                         for(S16 t = 0; t < result.count; t++){
                              if(block == result.objects[t].block && block_final_pos == result.objects[t].position){
                                   duplicate = true;
                                   break;
                              }
                         }

                         if(!duplicate){
                              BlockThroughPortal_t block_through_portal {};

                              block_through_portal.position = block_final_pos;
                              block_through_portal.block = block;
                              block_through_portal.src_portal_dir = interactive->portal.face;
                              block_through_portal.dst_portal_dir = current_portal_dir;
                              block_through_portal.src_portal = check_coord;
                              block_through_portal.dst_portal = portal_dst_coord;
                              block_through_portal.portal_rotations = portal_rotations;
                              block_through_portal.rotations_between_portals = rotations_between_portals;
                              block_through_portal.rotated_cut = rotated_cut;

                              result.insert(&block_through_portal);
                         }
                    }
               }
          }
     }

     return result;
}

BlockChainsResult_t find_block_chain(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S8 rotations, BlockChain_t* my_chain){
     BlockChainsResult_t result;

     Position_t block_pos = block_get_position(block);
     Vec_t block_pos_delta = block_get_pos_delta(block);
     auto block_cut = block_get_cut(block);

     auto against_result = block_against_other_blocks(block_pos + block_pos_delta, block_cut, direction, block_qt, interactive_qt, tilemap);

     BlockChain_t first_chain {};

     if(my_chain == NULL){
          my_chain = &first_chain;
          BlockChainEntry_t block_chain_entry {};
          block_chain_entry.block = block;
          block_chain_entry.rotations_through_portal = 0;
          my_chain->insert(&block_chain_entry);
     }

     BlockChain_t* current_chain = NULL;

     for(S16 i = 0; i < against_result.count; i++){
          BlockAgainstOther_t* against_entry = against_result.objects + i;
          Direction_t against_direction = direction_rotate_clockwise(direction, against_entry->rotations_through_portal);

          S8 against_rotations = (rotations + against_entry->rotations_through_portal) % DIRECTION_COUNT;

          BlockChain_t potential_new_chain {};

          // if it is the last against element, we use the chain we were originally passed in if available
          if(i == (against_result.count - 1)){
               current_chain = my_chain;
          }else{
               potential_new_chain = *my_chain;
               current_chain = &potential_new_chain;
          }

          BlockChainEntry_t block_chain_entry {};
          block_chain_entry.block = against_entry->block;
          block_chain_entry.rotations_through_portal = against_entry->rotations_through_portal;
          current_chain->insert(&block_chain_entry);

          auto merge_result = find_block_chain(against_entry->block, against_direction,
                                               block_qt, interactive_qt, tilemap, against_rotations, current_chain);

          if(merge_result.count == 0){
               result.insert(current_chain);
          }else{
               for(S16 a = 0; a < merge_result.count; a++){
                    result.insert(merge_result.objects + a);
               }
          }
     }

     return result;
}

bool block_on_frictionless(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                           QuadTreeNode_t<Block_t>* block_qt){
     return block_on_ice(pos, pos_delta, cut, tilemap, interactive_qt, block_qt) ||
            block_on_air(pos, pos_delta, cut, tilemap, interactive_qt, block_qt);
}

bool block_on_frictionless(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     return block_on_frictionless(block_get_position(block), block_get_pos_delta(block), block_get_cut(block), tilemap, interactive_qt, block_qt);
}

CheckBlockCollisionResult_t check_block_collision(World_t* world, Block_t* block){
     if(block->teleport){
          if(block->teleport_pos_delta.x != 0.0f || block->teleport_pos_delta.y != 0.0f){
               return check_block_collision_with_other_blocks(block->teleport_pos,
                                                              block->teleport_pos_delta,
                                                              block->teleport_vel,
                                                              block->teleport_accel,
                                                              block->teleport_cut,
                                                              block->stop_on_pixel_x,
                                                              block->stop_on_pixel_y,
                                                              block->teleport_horizontal_move,
                                                              block->teleport_vertical_move,
                                                              block->stopped_by_player_horizontal,
                                                              block->stopped_by_player_vertical,
                                                              get_block_index(world, block),
                                                              block->clone_start.x > 0,
                                                              world);
          }
     }else{
          if(block->pos_delta.x != 0.0f || block->pos_delta.y != 0.0f){
               return check_block_collision_with_other_blocks(block->pos,
                                                              block->pos_delta,
                                                              block->vel,
                                                              block->accel,
                                                              block->cut,
                                                              block->stop_on_pixel_x,
                                                              block->stop_on_pixel_y,
                                                              block->horizontal_move,
                                                              block->vertical_move,
                                                              block->stopped_by_player_horizontal,
                                                              block->stopped_by_player_vertical,
                                                              get_block_index(world, block),
                                                              block->clone_start.x > 0,
                                                              world);
          }
     }

     return CheckBlockCollisionResult_t{};
}

void stop_block_colliding_in_dir(Block_t* block, Direction_t move_dir_to_stop){
     switch(move_dir_to_stop){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
     {
          block->stop_on_pixel_x = closest_pixel(block->pos.pixel.x, block->pos.decimal.x);

          Position_t final_stop_pos = pixel_pos(Pixel_t{block->stop_on_pixel_x, 0});
          Vec_t pos_delta = pos_to_vec(final_stop_pos - block->pos);

          block->pos_delta.x = pos_delta.x;
          block->vel.x = 0;
          block->accel.x = 0;

          reset_move(&block->horizontal_move);
          break;
     }
     case DIRECTION_UP:
     case DIRECTION_DOWN:
     {
          block->stop_on_pixel_y = closest_pixel(block->pos.pixel.y, block->pos.decimal.y);

          Position_t final_stop_pos = pixel_pos(Pixel_t{0, block->stop_on_pixel_y});
          Vec_t pos_delta = pos_to_vec(final_stop_pos - block->pos);

          block->pos_delta.y = pos_delta.y;
          block->vel.y = 0;
          block->accel.y = 0;

          block->stop_on_pixel_y = 0;

          reset_move(&block->vertical_move);
          break;
     }
     }
}

void copy_block_collision_results(Block_t* block, CheckBlockCollisionResult_t* result){
     block->pos_delta = result->pos_delta;
     block->vel = result->vel;
     block->accel = result->accel;

     block->stop_on_pixel_x = result->stop_on_pixel_x;
     block->stop_on_pixel_y = result->stop_on_pixel_y;

     block->started_on_pixel_x = 0;
     block->started_on_pixel_y = 0;

     block->horizontal_move = result->horizontal_move;
     block->vertical_move = result->vertical_move;

     block->stopped_by_player_horizontal = result->stopped_by_player_horizontal;
     block->stopped_by_player_vertical = result->stopped_by_player_vertical;
}

void raise_above_blocks(World_t* world, Block_t* block){
     auto result = block_held_down_by_another_block(block, world->block_qt, world->interactive_qt, &world->tilemap);
     for(S16 i = 0; i < result.count; i++){
          Block_t* above_block = result.blocks_held[i].block;
          raise_above_blocks(world, above_block);
          above_block->pos.z++;
          above_block->held_up = BLOCK_HELD_BY_SOLID;
          raise_entangled_blocks(world, above_block);
     }
}

void raise_entangled_blocks(World_t* world, Block_t* block){
     if(block->entangle_index >= 0){
          S16 block_index = get_block_index(world, block);
          S16 entangle_index = block->entangle_index;
          while(entangle_index != block_index && entangle_index >= 0){
               auto entangled_block = world->blocks.elements + entangle_index;
               raise_above_blocks(world, entangled_block);
               entangled_block->pos.z++;
               entangled_block->held_up = BLOCK_HELD_BY_ENTANGLE;
               entangle_index = entangled_block->entangle_index;
          }
     }
}

bool block_pushes_are_the_same_collision(BlockMomentumPushes_t<128>* block_pushes, S16 start_index, S16 end_index, S16 block_index){
     if(start_index < 0 || start_index > block_pushes->count) return false;
     if(end_index < 0 || end_index > block_pushes->count) return false;
     if(start_index > end_index) return false;

     auto& start_push = block_pushes->pushes[start_index];
     auto& end_push = block_pushes->pushes[end_index];

     S16 start_push_count = 0;
     S16 end_push_count = 0;

     bool found_index = false;
     for(S16 i = 0; i < start_push.pusher_count; i++){
         auto& pusher = start_push.pushers[i];
         if(pusher.index != block_index) continue;
         found_index = true;
         start_push_count = pusher.collided_with_block_count;
     }
     if(!found_index) return false;

     found_index = false;
     for(S16 i = 0; i < end_push.pusher_count; i++){
         auto& pusher = end_push.pushers[i];
         if(pusher.index != block_index) continue;
         found_index = true;
         end_push_count = pusher.collided_with_block_count;
     }
     if(!found_index) return false;

     return (start_push_count > 1 && end_push_count > 1 && start_push_count == end_push_count &&
             start_push.direction == end_push.direction);
}

