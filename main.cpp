/*
http://www.simonstalenhag.se/
-Grant Sanderson 3blue1brown
-Shane Hendrixson
-"fail early, fail often, fail forward" -will smith youtube video
*/

#include <iostream>
#include <chrono>
#include <cfloat>
#include <cassert>

// linux
#include <dirent.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "log.h"
#include "defines.h"
#include "direction.h"
#include "bitmap.h"
#include "conversion.h"
#include "tile.h"
#include "object_array.h"
#include "interactive.h"
#include "quad_tree.h"
#include "player.h"
#include "block.h"
#include "arrow.h"
#include "undo.h"
#include "editor.h"

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

U8 portal_rotations_between(Direction_t a, Direction_t b){
     if(a == b) return 2;
     if(a == direction_opposite(b)) return 0;
     return direction_rotations_between(a, b);
}

Vec_t vec_rotate_quadrants(Vec_t vec, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = vec.x;
          vec.x = vec.y;
          vec.y = -tmp;
     }

     return vec;
}

Pixel_t pixel_rotate_quadrants(Pixel_t pixel, S8 rotations_between){
     for(S8 r = 0; r < rotations_between; r++){
          auto tmp = pixel.x;
          pixel.x = pixel.y;
          pixel.y = -tmp;
     }

     return pixel;
}

Position_t position_rotate_quadrants(Position_t pos, S8 rotations_between){
     pos.decimal = vec_rotate_quadrants(pos.decimal, rotations_between);
     pos.pixel = pixel_rotate_quadrants(pos.pixel, rotations_between);
     canonicalize(&pos);
     return pos;
}

Vec_t rotate_vec_between_dirs(Direction_t a, Direction_t b, Vec_t vec){
     U8 rotations_between = portal_rotations_between(a, b);
     return vec_rotate_quadrants(vec, rotations_between);
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

Pixel_t direction_to_pixel(Direction_t d){
     switch(d){
     default:
          break;
     case DIRECTION_LEFT:
          return Pixel_t{-TILE_SIZE_IN_PIXELS, 0};
     case DIRECTION_RIGHT:
          return Pixel_t{TILE_SIZE_IN_PIXELS, 0};
     case DIRECTION_UP:
          return Pixel_t{0, TILE_SIZE_IN_PIXELS};
     case DIRECTION_DOWN:
          return Pixel_t{0, -TILE_SIZE_IN_PIXELS};
     }

     return Pixel_t{0, 0};
}

Rect_t rect_surrounding_adjacent_coords(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);

     Rect_t rect = {};
     rect.left = pixel.x - TILE_SIZE_IN_PIXELS;
     rect.right = pixel.x + (2 * TILE_SIZE_IN_PIXELS);
     rect.bottom = pixel.y - TILE_SIZE_IN_PIXELS;
     rect.top = pixel.y + (2 * TILE_SIZE_IN_PIXELS);

     return rect;
}

Rect_t rect_surrounding_coord(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);

     Rect_t rect = {};
     rect.left = pixel.x;
     rect.right = pixel.x + TILE_SIZE_IN_PIXELS;
     rect.bottom = pixel.y;
     rect.top = pixel.y + TILE_SIZE_IN_PIXELS;

     return rect;
}

#define UNDO_MEMORY (4 * 1024 * 1024)

Interactive_t* quad_tree_interactive_find_at(QuadTreeNode_t<Interactive_t>* root, Coord_t coord){
     return quad_tree_find_at(root, coord.x, coord.y);
}

#define MAX_PORTAL_EXITS 4

struct PortalExitCoords_t{
     Coord_t coords[MAX_PORTAL_EXITS];
     S8 count;
};

struct PortalExit_t{
     PortalExitCoords_t directions[DIRECTION_COUNT];
};

void portal_exit_add(PortalExit_t* portal_exit, Direction_t direction, Coord_t coord){
     S8* count = &portal_exit->directions[direction].count;
     if(*count < MAX_PORTAL_EXITS){
          portal_exit->directions[direction].coords[*count] = coord;
          (*count)++;
     }
}

void find_portal_exits_impl(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                            PortalExit_t* portal_exit, Direction_t from){
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
          portal_exit_add(portal_exit, interactive->portal.face, coord);
          return;
     }

     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return;
     if((tile->flags & TILE_FLAG_WIRE_STATE) == 0) return;

     if((tile->flags & TILE_FLAG_WIRE_LEFT) && from != DIRECTION_LEFT){
          find_portal_exits_impl(coord + DIRECTION_LEFT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_RIGHT);
     }

     if((tile->flags & TILE_FLAG_WIRE_RIGHT) && from != DIRECTION_RIGHT){
          find_portal_exits_impl(coord + DIRECTION_RIGHT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_LEFT);
     }

     if((tile->flags & TILE_FLAG_WIRE_UP) && from != DIRECTION_UP){
          find_portal_exits_impl(coord + DIRECTION_UP, tilemap, interactive_quad_tree, portal_exit, DIRECTION_DOWN);
     }

     if((tile->flags & TILE_FLAG_WIRE_DOWN) && from != DIRECTION_DOWN){
          find_portal_exits_impl(coord + DIRECTION_DOWN, tilemap, interactive_quad_tree, portal_exit, DIRECTION_UP);
     }
}

PortalExit_t find_portal_exits(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     PortalExit_t portal_exit = {};
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL){
          portal_exit_add(&portal_exit, interactive->portal.face, coord);
          for(S8 d = 0; d < DIRECTION_COUNT; d++){
               Coord_t adjacent_coord = coord + (Direction_t)(d);
               Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
               if(!tile) continue;
               if((tile->flags & TILE_FLAG_WIRE_STATE) == 0) continue;
               if((d == DIRECTION_LEFT && tile->flags & TILE_FLAG_WIRE_RIGHT) ||
                  (d == DIRECTION_UP && tile->flags & TILE_FLAG_WIRE_DOWN) ||
                  (d == DIRECTION_RIGHT && tile->flags & TILE_FLAG_WIRE_LEFT) ||
                  (d == DIRECTION_DOWN && tile->flags & TILE_FLAG_WIRE_UP)){
                    find_portal_exits_impl(adjacent_coord, tilemap, interactive_quad_tree,
                                           &portal_exit, DIRECTION_COUNT);
               }
          }
     }
     return portal_exit;
}

// NOTE: skip_coord needs to be DIRECTION_COUNT size
void find_portal_adjacents_to_skip_collision_check(Coord_t coord, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                                   Coord_t* skip_coord){
     for(S8 i = 0; i < DIRECTION_COUNT; i++) skip_coord[i] = {-1, -1};

     // figure out which coords we can skip collision checking on, because they have portal exits
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
          skip_coord[interactive->portal.face] = coord + interactive->portal.face;
     }
}

Interactive_t* quad_tree_interactive_solid_at(QuadTreeNode_t<Interactive_t>* root, TileMap_t* tilemap, Coord_t coord){
     Interactive_t* interactive = quad_tree_find_at(root, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)){
               return interactive;
          }else if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
               PortalExit_t portal_exits = find_portal_exits(coord, tilemap, root);
               for(S8 d = 0; d < DIRECTION_COUNT; d++){
                    for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                         if(portal_exits.directions[d].coords[p] == coord) continue;

                         Coord_t through_portal_coord = portal_exits.directions[d].coords[p] - (Direction_t)(d);
                         Interactive_t* through_portal_interactive = quad_tree_find_at(root, through_portal_coord.x, through_portal_coord.y);
                         if(through_portal_interactive && interactive_is_solid(through_portal_interactive)){
                              return through_portal_interactive;
                         }
                    }
               }
          }
     }

     return nullptr;
}

S8 teleport_position_across_portal(Position_t* position, Vec_t* pos_delta, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                   TileMap_t* tilemap, Coord_t premove_coord, Coord_t postmove_coord){
     if(postmove_coord != premove_coord){
          auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, postmove_coord);
          if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL){
               if(interactive->portal.face == direction_opposite(direction_between(postmove_coord, premove_coord))){
                    Position_t offset_from_center = *position - coord_to_pos_at_tile_center(postmove_coord);
                    PortalExit_t portal_exit = find_portal_exits(postmove_coord, tilemap, interactive_quad_tree);

                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         for(S8 p = 0; p < portal_exit.directions[d].count; p++){
                              if(portal_exit.directions[d].coords[p] != postmove_coord){
                                   Position_t final_offset = offset_from_center;
                                   Direction_t opposite = direction_opposite((Direction_t)(d));
                                   U8 rotations_between = direction_rotations_between(interactive->portal.face, opposite);

                                   // TODO: compress these rotations
                                   for(U8 r = 0; r < rotations_between; r++){
                                        auto save_pixel_x = final_offset.pixel.x;
                                        final_offset.pixel.x = -final_offset.pixel.y;
                                        final_offset.pixel.y = save_pixel_x;

                                        auto save_decimal_x = final_offset.decimal.x;
                                        final_offset.decimal.x = -final_offset.decimal.y;
                                        final_offset.decimal.y = save_decimal_x;

                                   }

                                   rotations_between = portal_rotations_between(interactive->portal.face, (Direction_t)(d));

                                   if(pos_delta){
                                        *pos_delta = vec_rotate_quadrants(*pos_delta, rotations_between);
                                   }

                                   *position = coord_to_pos_at_tile_center(portal_exit.directions[d].coords[p] + opposite) + final_offset;
                                   return rotations_between;
                              }
                         }
                    }
               }
          }
     }

     return -1;
}

#define BLOCK_QUAD_TREE_MAX_QUERY 16
#define BLOCK_MAX_TOUCHING_COORDS 4
#define BLOCK_MAX_TOUCHING_HALFS 4

void block_touching_coords(Block_t* block, Coord_t* coords){
     coords[0] = pixel_to_coord(block->pos.pixel + Pixel_t{1, 1});
     coords[1] = pixel_to_coord(block->pos.pixel + Pixel_t{TILE_SIZE_IN_PIXELS - 2, 1});
     coords[2] = pixel_to_coord(block->pos.pixel + Pixel_t{1, TILE_SIZE_IN_PIXELS - 2});
     coords[3] = pixel_to_coord(block->pos.pixel + Pixel_t{TILE_SIZE_IN_PIXELS - 2, TILE_SIZE_IN_PIXELS - 2});
}

void block_touching_halfs(Block_t* block, Half_t* halfs){
     halfs[0] = pixel_to_half(block->pos.pixel + Pixel_t{1, 1});
     halfs[1] = pixel_to_half(block->pos.pixel + Pixel_t{TILE_SIZE_IN_PIXELS - 2, 1});
     halfs[2] = pixel_to_half(block->pos.pixel + Pixel_t{1, TILE_SIZE_IN_PIXELS - 2});
     halfs[3] = pixel_to_half(block->pos.pixel + Pixel_t{TILE_SIZE_IN_PIXELS - 2, TILE_SIZE_IN_PIXELS - 2});
}

bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     bool on_ice[BLOCK_MAX_TOUCHING_HALFS];
     Half_t halfs[BLOCK_MAX_TOUCHING_HALFS];

     block_touching_halfs(block, halfs);

     if(block->pos.z == 0){
          for(S8 i = 0; i < BLOCK_MAX_TOUCHING_HALFS; i++){
               on_ice[i] = false;

               Coord_t coord = half_to_coord(halfs[i]);
               Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
               if(interactive){
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         if(interactive->popup.lift.ticks == (block->pos.z + 1) && interactive->popup.iced){
                              on_ice[i] = true;
                              break;
                         }
                    }
               }

               on_ice[i] = tilemap_get_ice(tilemap, halfs[i]);
          }

          return on_ice[0] && on_ice[1] && on_ice[2] && on_ice[3];
     }

     // TODO: check for blocks below
     return false;
}

Rect_t rect_to_check_surrounding_blocks(Pixel_t center){
     Rect_t rect = {};
     rect.left = center.x - (2 * TILE_SIZE_IN_PIXELS);
     rect.right = center.x + (2 * TILE_SIZE_IN_PIXELS);
     rect.bottom = center.y - (2 * TILE_SIZE_IN_PIXELS);
     rect.top = center.y + (2 * TILE_SIZE_IN_PIXELS);
     return rect;
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
          Pixel_t src_fake_pixel = src_portal_center_pixel + pixel_rotate_quadrants(offset, rotations_between_portals);
          offsets[o] = src_fake_pixel - block_center_pixel(blocks[o]);
     }
}

// TODO: the form of this function looks a lot like block_inside_another_block(), see if we can compress these
Block_t* block_against_another_block(Block_t* block_to_check, Direction_t direction, QuadTreeNode_t<Block_t>* block_quad_tree,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir){
     Rect_t rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     for(S8 i = 0; i < BLOCK_QUAD_TREE_MAX_QUERY; i++){
          portal_offsets[i].x = 0;
          portal_offsets[i].y = 0;
     }

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
               if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
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

#define MAX_TELEPORTED_DEBUG_BLOCK_COUNT 4

Pixel_t g_collided_with_pixel = {};

Block_t* block_inside_block_list(Block_t* block_to_check, Block_t** blocks, S16 block_count, Position_t* collided_with, Pixel_t* portal_offsets){
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};

     for(S16 i = 0; i < block_count; i++){
          if(blocks[i] == block_to_check && portal_offsets[i].x == 0 && portal_offsets[i].y == 0) continue;
          Block_t* block = blocks[i];

          Pixel_t pixel_to_check = block->pos.pixel + portal_offsets[i];

          Pixel_t top_left {pixel_to_check.x, (S16)(pixel_to_check.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(pixel_to_check.x + TILE_SIZE_IN_PIXELS - 1), (S16)(pixel_to_check.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(pixel_to_check.x + TILE_SIZE_IN_PIXELS - 1), pixel_to_check.y};

          if(pixel_in_rect(pixel_to_check, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               *collided_with = block_get_center(block);
               collided_with->pixel += portal_offsets[i];
               g_collided_with_pixel = collided_with->pixel;
               return block;
          }
     }

     return nullptr;
}

#define MAX_CONNECTED_PORTALS 8

struct BlockInsideResult_t{
     Block_t* block;
     Position_t collision_pos;
     U8 portal_rotations;
     Coord_t src_portal_coord;
     Coord_t dst_portal_coord;
};

BlockInsideResult_t block_inside_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree,
                                                QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap){
     BlockInsideResult_t result = {};

     // TODO: need more complicated function to detect this
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     Pixel_t portal_offsets[BLOCK_QUAD_TREE_MAX_QUERY];
     for(S8 i = 0; i < BLOCK_QUAD_TREE_MAX_QUERY; i++){
          portal_offsets[i].x = 0;
          portal_offsets[i].y = 0;
     }

     Block_t* collided_block = block_inside_block_list(block_to_check, blocks, block_count, &result.collision_pos, portal_offsets);
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
               if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                    auto portal_exits = find_portal_exits(src_coord, tilemap, interactive_quad_tree);
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                              auto dst_coord = portal_exits.directions[d].coords[p];
                              if(dst_coord == src_coord) continue;

                              search_portal_destination_for_blocks(block_quad_tree, interactive->portal.face, (Direction_t)(d), src_coord,
                                                                   dst_coord, blocks, &block_count, portal_offsets);

                              collided_block = block_inside_block_list(block_to_check, blocks, block_count, &result.collision_pos, portal_offsets);
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

Block_t* block_held_up_by_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
     Rect_t surrounding_rect = rect_to_check_surrounding_blocks(block_center_pixel(block_to_check));
     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, surrounding_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     S8 held_at_height = block_to_check->pos.z - HEIGHT_INTERVAL;
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          if(block == block_to_check || block->pos.z != held_at_height) continue;

          Pixel_t top_left {block->pos.pixel.x, (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), block->pos.pixel.y};

          if(pixel_in_rect(block->pos.pixel, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               return block;
          }
     }

     return nullptr;
}

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
          pixel.y += (TILE_SIZE_IN_PIXELS - 1);
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
          pixel.y += (TILE_SIZE_IN_PIXELS - 1);
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
          pixel.x += (TILE_SIZE_IN_PIXELS - 1);
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
          pixel.x += (TILE_SIZE_IN_PIXELS - 1);
          *b = pixel;
          return true;
     };
     }

     return false;
}

Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap,
                                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t skip_coord[DIRECTION_COUNT];
     find_portal_adjacents_to_skip_collision_check(block_get_coord(block_to_check), interactive_quad_tree, skip_coord);

     Coord_t tile_coord = pixel_to_coord(pixel_a);

     bool skip = false;
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          if(skip_coord[d] == tile_coord){
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
     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          if(skip_coord[d] == tile_coord){
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

Vec_t collide_circle_with_line(Vec_t circle_center, F32 circle_radius, Vec_t a, Vec_t b, bool* collided){
     // move data we care about to the origin
     Vec_t c = circle_center - a;
     Vec_t l = b - a;
     Vec_t closest_point_on_l_to_c = vec_project_onto(c, l);
     Vec_t collision_to_c = closest_point_on_l_to_c - c;

     if(vec_magnitude(collision_to_c) < circle_radius){
          // find edge of circle in that direction
          Vec_t edge_of_circle = vec_normalize(collision_to_c) * circle_radius;
          *collided = true;
          return collision_to_c - edge_of_circle;
     }

     return vec_zero();
}

S16 range_passes_tile_boundary(S16 a, S16 b, S16 ignore){
     if(a == b) return 0;
     if(a > b){
          if((b % HALF_TILE_SIZE_IN_PIXELS) == 0) return 0;
          SWAP(a, b);
     }

     for(S16 i = a; i <= b; i++){
          if((i % HALF_TILE_SIZE_IN_PIXELS) == 0 && i != ignore){
               return i;
          }
     }

     return 0;
}

GLuint create_texture_from_bitmap(AlphaBitmap_t* bitmap){
     GLuint texture = 0;

     glGenTextures(1, &texture);

     glBindTexture(GL_TEXTURE_2D, texture);

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width, bitmap->height, 0,  GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);

     return texture;
}

GLuint transparent_texture_from_file(const char* filepath){
     Bitmap_t bitmap = bitmap_load_from_file(filepath);
     if(bitmap.raw.byte_count == 0) return 0;
     AlphaBitmap_t alpha_bitmap = bitmap_to_alpha_bitmap(&bitmap, BitmapPixel_t{255, 0, 255});
     free(bitmap.raw.bytes);
     GLuint texture_id = create_texture_from_bitmap(&alpha_bitmap);
     free(alpha_bitmap.pixels);
     return texture_id;
}

#define THEME_FRAMES_WIDE 16
#define THEME_FRAMES_TALL 32
#define THEME_FRAME_WIDTH 0.0625f
#define THEME_FRAME_HEIGHT 0.03125f

Vec_t theme_frame(S16 x, S16 y){
     y = (THEME_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * THEME_FRAME_WIDTH, (F32)(y) * THEME_FRAME_HEIGHT};
}

#define ARROW_FRAME_WIDTH 0.25f
#define ARROW_FRAME_HEIGHT 0.0625f
#define ARROW_FRAMES_TALL 16
#define ARROW_FRAMES_WIDE 4

Vec_t arrow_frame(S8 x, S8 y) {
     y = (ARROW_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * ARROW_FRAME_WIDTH, (F32)(y) * ARROW_FRAME_HEIGHT};
}

#define PLAYER_FRAME_WIDTH 0.25f
#define PLAYER_FRAME_HEIGHT 0.03125f
#define PLAYER_FRAMES_WIDE 4
#define PLAYER_FRAMES_TALL 32

Vec_t player_frame(S8 x, S8 y){
     y = (PLAYER_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * PLAYER_FRAME_WIDTH, (F32)(y) * PLAYER_FRAME_HEIGHT};
}

void toggle_flag(U32* flags, U32 flag){
     if(*flags & flag){
          *flags &= ~flag;
     }else{
          *flags |= flag;
     }
}

void clear_ice_flags(U32* flags){
     *flags &= ~TILE_FLAG_ICED_TOP_LEFT;
     *flags &= ~TILE_FLAG_ICED_TOP_RIGHT;
     *flags &= ~TILE_FLAG_ICED_BOTTOM_LEFT;
     *flags &= ~TILE_FLAG_ICED_BOTTOM_RIGHT;
}

void toggle_electricity(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, Coord_t coord,
                        Direction_t direction, bool activated_by_door){
     Coord_t adjacent_coord = coord + direction;
     Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
     if(!tile) return;

     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, adjacent_coord);
     if(interactive){
          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_POPUP:
          {
               interactive->popup.lift.up = !interactive->popup.lift.up;
               clear_ice_flags(&tile->flags);
          } break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = !interactive->door.lift.up;
               if(!activated_by_door) toggle_electricity(tilemap, interactive_quad_tree,
                                                         coord_move(coord, interactive->door.face, 3),
                                                         interactive->door.face, true);
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.on = !interactive->portal.on;
               break;
          }
     }

     if(tile->flags & (TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN)){
          switch(direction){
          default:
               return;
          case DIRECTION_LEFT:
               if(!(tile->flags & TILE_FLAG_WIRE_RIGHT)){
                    return;
               }
               break;
          case DIRECTION_RIGHT:
               if(!(tile->flags & TILE_FLAG_WIRE_LEFT)){
                    return;
               }
               break;
          case DIRECTION_UP:
               if(!(tile->flags & TILE_FLAG_WIRE_DOWN)){
                    return;
               }
               break;
          case DIRECTION_DOWN:
               if(!(tile->flags & TILE_FLAG_WIRE_UP)){
                    return;
               }
               break;
          }

          // toggle wire state
          if(tile->flags & TILE_FLAG_WIRE_STATE){
               tile->flags &= ~TILE_FLAG_WIRE_STATE;
          }else{
               tile->flags |= TILE_FLAG_WIRE_STATE;
          }

          if(tile->flags & TILE_FLAG_WIRE_LEFT && direction != DIRECTION_RIGHT){
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_LEFT, false);
          }

          if(tile->flags & TILE_FLAG_WIRE_RIGHT && direction != DIRECTION_LEFT){
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_RIGHT, false);
          }

          if(tile->flags & TILE_FLAG_WIRE_DOWN && direction != DIRECTION_UP){
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_DOWN, false);
          }

          if(tile->flags & TILE_FLAG_WIRE_UP && direction != DIRECTION_DOWN){
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_UP, false);
          }
     }else if(tile->flags & (TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT)){
          bool all_on_before = tile_flags_cluster_all_on(tile->flags);

          Direction_t cluster_direction = tile_flags_cluster_direction(tile->flags);
          switch(cluster_direction){
          default:
               break;
          case DIRECTION_LEFT:
               switch(direction){
               default:
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_RIGHT:
               switch(direction){
               default:
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_DOWN:
               switch(direction){
               default:
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_UP:
               switch(direction){
               default:
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) toggle_flag(&tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          }

          bool all_on_after = tile_flags_cluster_all_on(tile->flags);

          if(all_on_before != all_on_after){
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, cluster_direction, false);
          }
     }
}

void activate(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, Coord_t coord){
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(!interactive) return;

     if(interactive->type != INTERACTIVE_TYPE_LEVER &&
        interactive->type != INTERACTIVE_TYPE_PRESSURE_PLATE &&
        interactive->type != INTERACTIVE_TYPE_LIGHT_DETECTOR &&
        interactive->type != INTERACTIVE_TYPE_ICE_DETECTOR) return;

     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_LEFT, false);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_RIGHT, false);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_UP, false);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_DOWN, false);
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

void block_push(Block_t* block, Direction_t direction, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                QuadTreeNode_t<Block_t>* block_quad_tree, bool pushed_by_ice){
     Direction_t collided_block_push_dir = DIRECTION_COUNT;
     Block_t* collided_block = block_against_another_block(block, direction, block_quad_tree, interactive_quad_tree, tilemap,
                                                           &collided_block_push_dir);
     if(collided_block){
          if(collided_block == block){
               // pass
          }else if(pushed_by_ice && block_on_ice(collided_block, tilemap, interactive_quad_tree)){
               block_push(collided_block, collided_block_push_dir, tilemap, interactive_quad_tree, block_quad_tree, pushed_by_ice);
               return;
          }else{
               return;
          }
     }

     if(block_against_solid_tile(block, direction, tilemap, interactive_quad_tree)) return;
     if(block_against_solid_interactive(block, direction, tilemap, interactive_quad_tree)) return;

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          block->accel.x = -PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_RIGHT:
          block->accel.x = PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_DOWN:
          block->accel.y = -PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_UP:
          block->accel.y = PLAYER_SPEED * 0.99f;
          break;
     }

     block->push_start = block->pos.pixel;
}

void player_collide_coord(Position_t player_pos, Coord_t coord, F32 player_radius, Vec_t* pos_delta, bool* collide_with_coord){
     Coord_t player_coord = pos_to_coord(player_pos);
     Position_t relative = coord_to_pos(coord) - player_pos;
     Vec_t bottom_left = pos_to_vec(relative);
     Vec_t top_left {bottom_left.x, bottom_left.y + TILE_SIZE};
     Vec_t top_right {bottom_left.x + TILE_SIZE, bottom_left.y + TILE_SIZE};
     Vec_t bottom_right {bottom_left.x + TILE_SIZE, bottom_left.y};

     DirectionMask_t mask = directions_between(coord, player_coord);

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

#define LIGHT_MAX_LINE_LEN 16

Half_t g_illuminate_halfs[LIGHT_MAX_LINE_LEN];
S8 g_illuminate_half_count = 0;

void illuminate_line(Half_t start, Half_t end, U8 value, TileMap_t* tilemap, QuadTreeNode_t<Block_t>* block_quad_tree,
                     Half_t* origins, bool debug){
     Half_t halfs[LIGHT_MAX_LINE_LEN];
     S8 half_count = 0;
     bool xy_swapped = false;

     // determine line of points using a modified bresenham to be symmetrical
     {
          if(start.x == end.x){
               S16 step_y = (end.y - start.y >= 0) ? 1 : -1;
               S16 end_step_y = end.y + step_y;

               // build a simple vertical path
               for(S16 y = start.y; y != end_step_y; y += step_y){
                    halfs[half_count] = {start.x, y};
                    half_count++;
               }
          }else{
               F64 error = 0.0;
               F64 dx = (F64)(end.x) - (F64)(start.x);
               F64 dy = (F64)(end.y) - (F64)(start.y);
               F64 derror = fabs(dy / dx);

               // if there are more xs than ys, then flip the xs and ys so we can have a symmetrical way of handling xs and ys
               if(derror < 1.0){
                    xy_swapped = true;

                    F64 tmp = dx;
                    dx = dy;
                    dy = tmp;

                    derror = fabs(dy / dx);

                    tmp = start.x;
                    start.x = start.y;
                    start.y = tmp;

                    tmp = end.x;
                    end.x = end.y;
                    end.y = tmp;
               }

               S16 step_x = (start.x < end.x) ? 1 : -1;
               S16 step_y = (end.y - start.y >= 0) ? 1 : -1;
               S16 end_step_x = end.x + step_x;
               S16 end_step_y = end.y + step_y;
               S16 sy = start.y;

               for(S16 sx = start.x; sx != end_step_x && sy != end_step_y; sx += step_x){
                    Half_t half {sx, sy};
                    halfs[half_count] = half;
                    half_count++;

                    error += derror;
                    while(error >= 0.5){
                         if(sy == end_step_y) break;
                         half = {sx, sy};

                         // only add non-duplicate halfs
                         if(halfs[half_count - 1] != half){
                              halfs[half_count] = half;
                              half_count++;
                         }

                         sy += step_y;
                         error -= 1.0;
                    }
               }
          }
     }

     // unswap xs and ys if we have swapped!
     if(xy_swapped){
          S16 tmp = start.x;
          start.x = start.y;
          start.y = tmp;

          tmp = end.x;
          end.x = end.y;
          end.y = tmp;

          for(S8 i = 0; i < half_count; ++i){
               tmp = halfs[i].x;
               halfs[i].x = halfs[i].y;
               halfs[i].y = tmp;
          }
     }

     if(debug){
          for(S8 i = 0; i < half_count; ++i){
               g_illuminate_half_count = half_count;
               g_illuminate_halfs[i] = halfs[i];
          }
     }

     for(S8 i = 0; i < half_count; ++i){
          Coord_t coord = half_to_coord(halfs[i]);
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          if(!tile) break;

          S16 diff_x = (abs(halfs[i].x - start.x) + 1) / 2;
          S16 diff_y = (abs(halfs[i].y - start.y) + 1) / 2;
          U8 distance = static_cast<U8>(sqrt(static_cast<F32>(diff_x * diff_x + diff_y * diff_y)));

          U8 new_value = value - (distance * LIGHT_DECAY);
          if(new_value > value) break;
          if(new_value <= BASE_LIGHT) break;

          Block_t* block = nullptr;
          bool from_origin = false;

          for(S8 o = 0; o < 4; o++){
               if(origins[o] == halfs[i]){
                    from_origin = true;
                    break;
               }
          }

          if(!from_origin){
               Rect_t coord_rect = rect_surrounding_adjacent_coords(coord);

               S16 block_count = 0;
               Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
               quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

               for(S16 b = 0; b < block_count && !block; b++){
                    Half_t block_halfs[BLOCK_MAX_TOUCHING_HALFS];
                    block_touching_halfs(blocks[b], block_halfs);

                    for(S8 h = 0; h < BLOCK_MAX_TOUCHING_HALFS; h++){
                         if(block_halfs[h] == halfs[i]){
                              block = blocks[b];
                              break;
                         }
                    }
               }
          }

          U8* tile_light = tilemap_get_light(tilemap, halfs[i]);
          if(*tile_light < new_value) *tile_light = new_value;

          if(tile_is_solid(tile)) break;
          if(block){
               Half_t block_halfs[BLOCK_MAX_TOUCHING_HALFS];
               block_touching_halfs(block, block_halfs);

               for(S8 h = 0; h < BLOCK_MAX_TOUCHING_HALFS; h++){
                    tile_light = tilemap_get_light(tilemap, block_halfs[h]);
                    if(*tile_light < new_value) *tile_light = new_value;
               }
               break;
          }
     }
}

#define BOTTOM_LEFT_HALF 0
#define BOTTOM_RIGHT_HALF 1
#define TOP_LEFT_HALF 2
#define TOP_RIGHT_HALF 3

void get_coord_sized_halfs(Half_t bottom_left, Half_t* halfs){
     halfs[BOTTOM_LEFT_HALF] = bottom_left;
     halfs[BOTTOM_RIGHT_HALF] = bottom_left + Half_t{1, 0};
     halfs[TOP_LEFT_HALF] = bottom_left + Half_t{0, 1};
     halfs[TOP_RIGHT_HALF] = bottom_left + Half_t{1, 1};
}

S16 g_debug_light_iteration = 0;

void illuminate(Half_t bottom_left_half, U8 value, TileMap_t* tilemap, QuadTreeNode_t<Block_t>* block_quad_tree){
     Half_t origins[4];

     get_coord_sized_halfs(bottom_left_half, origins);

     if(bottom_left_half.x < 0 || bottom_left_half.y < 0 ||
        origins[TOP_RIGHT_HALF].x >= (tilemap->width * 2) ||
        origins[TOP_RIGHT_HALF].y >= (tilemap->height * 2)) return;

     S16 radius = ((value - BASE_LIGHT) / LIGHT_DECAY) + 1;

     if(radius < 0) return;

     Half_t delta {(S16)(radius * 2), (S16)(radius * 2)};
     Half_t min = bottom_left_half - delta;
     Half_t max = origins[TOP_RIGHT_HALF] + delta;

     // loop from just after the min to just before the max, because the opposite loop will cover those
     // S16 iteration = 0;
     for(S16 j = max.y; j >= min.y; --j) {
          if(j <= bottom_left_half.y){
               illuminate_line(origins[BOTTOM_LEFT_HALF], Half_t{min.x, j}, value, tilemap, block_quad_tree, origins, false);
               // illuminate_line(origins[BOTTOM_LEFT_HALF], Half_t{min.x, j}, value, tilemap, block_quad_tree, origins, iteration == g_debug_light_iteration);
               // iteration++;

               illuminate_line(origins[BOTTOM_RIGHT_HALF], Half_t{max.x, j}, value, tilemap, block_quad_tree, origins, false);
          }else{
               illuminate_line(origins[TOP_LEFT_HALF], Half_t{min.x, j}, value, tilemap, block_quad_tree, origins, false);
               illuminate_line(origins[TOP_RIGHT_HALF], Half_t{max.x, j}, value, tilemap, block_quad_tree, origins, false);
          }
     }

     for(S16 i = min.x; i <= max.x; ++i) {
          if(i <= origins[BOTTOM_LEFT_HALF].x){
               illuminate_line(origins[BOTTOM_LEFT_HALF], Half_t{i, min.y}, value, tilemap, block_quad_tree, origins, false);
               illuminate_line(origins[TOP_LEFT_HALF], Half_t{i, max.y}, value, tilemap, block_quad_tree, origins, false);
          }else{
               illuminate_line(origins[BOTTOM_RIGHT_HALF], Half_t{i, min.y}, value, tilemap, block_quad_tree, origins, false);
               illuminate_line(origins[TOP_RIGHT_HALF], Half_t{i, max.y}, value, tilemap, block_quad_tree, origins, false);
          }
     }
}

void spread_element(Half_t bottom_left_half, S16 radius, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                    QuadTreeNode_t<Block_t>* block_quad_tree, Element_t element, bool teleported){
     assert(element == ELEMENT_FIRE || element == ELEMENT_ICE);
     (void)(teleported); // TODO: remove
     Half_t top_right_half = bottom_left_half + Half_t{1, 1};

     if(bottom_left_half.x < 0 || bottom_left_half.y < 0 || top_right_half.x >= (tilemap->width * 2) || top_right_half.y >= (tilemap->height * 2)) return;

     Half_t delta {radius, radius};
     Half_t min = bottom_left_half - delta;
     Half_t max = top_right_half + delta;

     bool ice = element == ELEMENT_ICE;

     for(S16 y = min.y; y <= max.y; ++y){
          for(S16 x = min.x; x <= max.x; ++x){
               Half_t half{x, y};

               Tile_t* tile = tilemap_get_tile(tilemap, half);
               if(tile && !tile_is_solid(tile)){
                    Coord_t coord = half_to_coord(half);
                    Rect_t coord_rect = rect_surrounding_adjacent_coords(coord);
                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Block_t* block = nullptr;
                    for(S16 i = 0; i < block_count; i++){
                         Half_t block_halfs[BLOCK_MAX_TOUCHING_HALFS];
                         block_touching_halfs(blocks[i], block_halfs);

                         for(S8 h = 0; h < BLOCK_MAX_TOUCHING_HALFS; h++){
                              if(block_halfs[h] == half && blocks[i]->pos.z == 0){
                                   block = blocks[i];
                                   break;
                              }
                         }
                    }

                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, coord.x, coord.y);

                    if(block){
                         if(block->element == ELEMENT_NONE && ice){
                              block->element = ELEMENT_ONLY_ICED;
                         }else if(block->element == ELEMENT_ONLY_ICED && !ice){
                              block->element = ELEMENT_NONE;
                         }
                    }else{
                         if(interactive){
                              // TODO: switch
                              if(interactive->type == INTERACTIVE_TYPE_POPUP){
                                   if(interactive->popup.lift.ticks == 1){
                                        interactive->popup.iced = ice;
                                        tilemap_set_ice(tilemap, half, ice);
                                   }else{
                                        interactive->popup.iced = ice;
                                   }
                              }else if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE ||
                                       interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR ||
                                       interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR){
                                   tilemap_set_ice(tilemap, half, ice);
                              }
                         }else{
                              tilemap_set_ice(tilemap, half, ice);
                         }
                    }

#if 0
                    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on && !teleported){
                         auto portal_exits = find_portal_exits(coord, tilemap, interactive_quad_tree);
                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                                   if(portal_exits.directions[d].coords[p] == coord) continue;
                                   S16 x_diff = coord.x - center.x;
                                   S16 y_diff = coord.y - center.y;
                                   U8 distance_from_center = (U8)(sqrt(x_diff * x_diff + y_diff * y_diff));
                                   Direction_t opposite = direction_opposite((Direction_t)(d));

                                   spread_element(portal_exits.directions[d].coords[p] + opposite, radius - distance_from_center,
                                                  tilemap, interactive_quad_tree, block_quad_tree, element, true);
                              }
                         }
                    }
#endif
               }
          }
     }
}

enum PlayerActionType_t{
     PLAYER_ACTION_TYPE_MOVE_LEFT_START,
     PLAYER_ACTION_TYPE_MOVE_LEFT_STOP,
     PLAYER_ACTION_TYPE_MOVE_UP_START,
     PLAYER_ACTION_TYPE_MOVE_UP_STOP,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_START,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP,
     PLAYER_ACTION_TYPE_MOVE_DOWN_START,
     PLAYER_ACTION_TYPE_MOVE_DOWN_STOP,
     PLAYER_ACTION_TYPE_ACTIVATE_START,
     PLAYER_ACTION_TYPE_ACTIVATE_STOP,
     PLAYER_ACTION_TYPE_SHOOT_START,
     PLAYER_ACTION_TYPE_SHOOT_STOP,
     PLAYER_ACTION_TYPE_END_DEMO,
     PLAYER_ACTION_TYPE_UNDO,
     PLAYER_ACTION_TYPE_GRAB_START,
     PLAYER_ACTION_TYPE_GRAB_STOP,
     PLAYER_ACTION_TYPE_RESET_ROOM,
};

struct PlayerAction_t{
     bool move_left;
     bool move_right;
     bool move_up;
     bool move_down;
     bool activate;
     bool last_activate;
     bool shoot;
     bool reface;
     bool undo;
     bool reset_room;
     S8 move_left_rotation;
     S8 move_right_rotation;
     S8 move_up_rotation;
     S8 move_down_rotation;
};

enum DemoMode_t{
     DEMO_MODE_NONE,
     DEMO_MODE_PLAY,
     DEMO_MODE_RECORD,
};

struct DemoEntry_t{
     S64 frame;
     PlayerActionType_t player_action_type;
};

void demo_entry_get(DemoEntry_t* demo_entry, FILE* file){
     size_t read_count = fread(demo_entry, sizeof(*demo_entry), 1, file);
     if(read_count != 1) demo_entry->frame = (S64)(-1);
}

void player_action_perform(PlayerAction_t* player_action, Player_t* player, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count){
     switch(player_action_type){
     default:
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_START:
          if(player_action->move_left) return;
          player_action->move_left = true;
          if(player->face != DIRECTION_LEFT){
               player->face = DIRECTION_LEFT;
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_LEFT, player_action->move_left_rotation)){
               player_action->reface = true;
          }
          player_action->move_left = false;
          player_action->move_left_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_START:
          if(player_action->move_up) return;
          player_action->move_up = true;
          if(player->face != DIRECTION_UP){
               player->face = DIRECTION_UP;
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_UP, player_action->move_up_rotation)){
               player_action->reface = true;
          }
          player_action->move_up = false;
          player_action->move_up_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_START:
          if(player_action->move_right) return;
          player_action->move_right = true;
          if(player->face != DIRECTION_RIGHT){
               player->face = DIRECTION_RIGHT;
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP:
     {
          if(player->face == direction_rotate_clockwise(DIRECTION_RIGHT, player_action->move_right_rotation)){
               player_action->reface = true;
          }
          player_action->move_right = false;
          player_action->move_right_rotation = 0;
     } break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_START:
          if(player_action->move_down) return;
          player_action->move_down = true;
          if(player->face != DIRECTION_DOWN){
               player->face = DIRECTION_DOWN;
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_DOWN, player_action->move_down_rotation)){
               player_action->reface = true;
          }
          player_action->move_down = false;
          player_action->move_down_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_START:
          if(player_action->activate) return;
          player_action->activate = true;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_STOP:
          player_action->activate = false;
          break;
     case PLAYER_ACTION_TYPE_SHOOT_START:
          if(player_action->shoot) return;
          player_action->shoot = true;
          break;
     case PLAYER_ACTION_TYPE_SHOOT_STOP:
          player_action->shoot = false;
          break;
     case PLAYER_ACTION_TYPE_UNDO:
          if(player_action->undo) return;
          player_action->undo = true;
          break;
     case PLAYER_ACTION_TYPE_GRAB_START:
          break;
     case PLAYER_ACTION_TYPE_GRAB_STOP:
          break;
     case PLAYER_ACTION_TYPE_RESET_ROOM:
          if(player_action->reset_room) return;
          player_action->reset_room = true;
          break;
     }

     if(demo_mode == DEMO_MODE_RECORD){
          DemoEntry_t demo_entry {frame_count, player_action_type};
          fwrite(&demo_entry, sizeof(demo_entry), 1, demo_file);
     }
}

#define MAP_VERSION 2

#pragma pack(push, 1)
enum TileFlagV1_t : U16{
     TILE_FLAG_V1_ICED = 1,
     TILE_FLAG_V1_CHECKPOINT = 1 << 1,
     TILE_FLAG_V1_RESET_IMMUNE = 1 << 2,

     TILE_FLAG_V1_WIRE_STATE = 1 << 3,

     TILE_FLAG_V1_WIRE_LEFT = 1 << 4,
     TILE_FLAG_V1_WIRE_UP = 1 << 5,
     TILE_FLAG_V1_WIRE_RIGHT = 1 << 6,
     TILE_FLAG_V1_WIRE_DOWN = 1 << 7,

     TILE_FLAG_V1_WIRE_CLUSTER_LEFT = 1 << 8,
     TILE_FLAG_V1_WIRE_CLUSTER_MID = 1 << 9,
     TILE_FLAG_V1_WIRE_CLUSTER_RIGHT = 1 << 10,

     TILE_FLAG_V1_WIRE_CLUSTER_LEFT_ON = 1 << 11,
     TILE_FLAG_V1_WIRE_CLUSTER_MID_ON = 1 << 12,
     TILE_FLAG_V1_WIRE_CLUSTER_RIGHT_ON = 1 << 13,

     // last 2 bits combine to say which direction the cluster is facing
     // 00 left
     // 10 right
     // 11 up
     // 01 down

     TILE_FLAG_V1_FIRST_DIRECTION_BIT = 1 << 14,
     TILE_FLAG_V1_SECOND_DIRECTION_BIT = 1 << 15,
};

struct MapTileV1_t{
     U8 id;
     U16 flags;
};

struct MapTileV2_t{
     U8 id;
     U32 flags;
};

struct MapBlockV1_t{
     Pixel_t pixel;
     Direction_t face;
     Element_t element;
     S8 z;
};

struct MapPopupV1_t{
     bool up;
     bool iced;
};

struct MapDoorV1_t{
     bool up;
     Direction_t face;
};

struct MapInteractiveV1_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          Stairs_t stairs;
          MapDoorV1_t door; // up or down
          Portal_t portal;
     };
};
#pragma pack(pop)

void load_v1_blocks(ObjectArray_t<Block_t>* block_array, MapBlockV1_t* map_blocks, S16 block_count){
     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          block->pos.pixel = map_blocks[i].pixel;
          block->pos.z = map_blocks[i].z;
          block->face = map_blocks[i].face;
          block->element = map_blocks[i].element;
     }
}

void load_v1_interactives(ObjectArray_t<Interactive_t>* interactive_array, MapInteractiveV1_t* map_interactives, S16 interactive_count){
     for(S16 i = 0; i < interactive_count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          interactive->coord = map_interactives[i].coord;
          interactive->type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactives[i].popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactives[i].popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactives[i].door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactives[i].portal.face;
               interactive->portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactives[i].stairs.up;
               interactive->stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_PROMPT:
               break;
          }
     }
}

bool save_map_to_file(FILE* file, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                      ObjectArray_t<Interactive_t>* interactive_array){
     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(tilemap->width) * (S32)(tilemap->height);
     MapTileV2_t* map_tiles = (MapTileV2_t*)(calloc(map_tile_count, sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc(block_array->count, sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_array->count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc(interactive_array->count, sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_array->count);
          return false;
     }

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               map_tiles[index].id = tilemap->tiles[y][x].id;
               map_tiles[index].flags = tilemap->tiles[y][x].flags;
               index++;
          }
     }

     for(S16 i = 0; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          map_blocks[i].pixel = block->pos.pixel;
          map_blocks[i].z = block->pos.z;
          map_blocks[i].face = block->face;
          map_blocks[i].element = block->element;
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          map_interactives[i].coord = interactive_array->elements[i].coord;
          map_interactives[i].type = interactive_array->elements[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               map_interactives[i].pressure_plate = interactive_array->elements[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               map_interactives[i].detector = interactive_array->elements[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               map_interactives[i].popup.up = interactive_array->elements[i].popup.lift.up;
               map_interactives[i].popup.iced = interactive_array->elements[i].popup.iced;
               break;
          case INTERACTIVE_TYPE_DOOR:
               map_interactives[i].door.up = interactive_array->elements[i].door.lift.up;
               map_interactives[i].door.face = interactive_array->elements[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               map_interactives[i].portal.face = interactive_array->elements[i].portal.face;
               map_interactives[i].portal.on = interactive_array->elements[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               map_interactives[i].stairs.up = interactive_array->elements[i].stairs.up;
               map_interactives[i].stairs.face = interactive_array->elements[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_PROMPT:
               break;
          }
     }

     U8 map_version = MAP_VERSION;
     fwrite(&map_version, sizeof(map_version), 1, file);
     fwrite(&player_start, sizeof(player_start), 1, file);
     fwrite(&tilemap->width, sizeof(tilemap->width), 1, file);
     fwrite(&tilemap->height, sizeof(tilemap->height), 1, file);
     fwrite(&block_array->count, sizeof(block_array->count), 1, file);
     fwrite(&interactive_array->count, sizeof(interactive_array->count), 1, file);
     fwrite(map_tiles, sizeof(*map_tiles), map_tile_count, file);
     fwrite(map_blocks, sizeof(*map_blocks), block_array->count, file);
     fwrite(map_interactives, sizeof(*map_interactives), interactive_array->count, file);

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool save_map(const char* filepath, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array){
     // write to file
     FILE* f = fopen(filepath, "wb");
     if(!f){
          LOG("%s: fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = save_map_to_file(f, player_start, tilemap, block_array, interactive_array);
     LOG("saved map %s\n", filepath);
     fclose(f);
     return success;
}

template <typename T1, typename T2>
void convert_bit_field(T1 field_a, T2* field_b, T1 flag_a, T2 flag_b){
     if(field_a & flag_a){
          *field_b |= flag_b;
     }
}

void tilemap_reset_light(TileMap_t* tilemap){
     S32 light_width = tilemap->width * 2;
     S32 light_height = tilemap->height * 2;

     for(S32 y = 0; y < light_height; y++){
          for(S32 x = 0; x < light_width; x++){
               tilemap->light[y][x] = BASE_LIGHT;
          }
     }
}

bool load_v1_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc(map_tile_count, sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc(block_count, sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc(interactive_count, sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), map_tile_count, file);
     fread(map_blocks, sizeof(*map_blocks), block_count, file);
     fread(map_interactives, sizeof(*map_interactives), interactive_count, file);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               tilemap->tiles[y][x].id = map_tiles[index].id;

               if(map_tiles[index].flags & TILE_FLAG_V1_ICED){
                    tilemap->tiles[y][x].flags |= TILE_FLAG_ICED_TOP_LEFT;
                    tilemap->tiles[y][x].flags |= TILE_FLAG_ICED_TOP_RIGHT;
                    tilemap->tiles[y][x].flags |= TILE_FLAG_ICED_BOTTOM_LEFT;
                    tilemap->tiles[y][x].flags |= TILE_FLAG_ICED_BOTTOM_RIGHT;
               }

               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_CHECKPOINT, TILE_FLAG_CHECKPOINT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_RESET_IMMUNE, TILE_FLAG_RESET_IMMUNE);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_STATE, TILE_FLAG_WIRE_STATE);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_LEFT, TILE_FLAG_WIRE_LEFT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_UP, TILE_FLAG_WIRE_UP);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_RIGHT, TILE_FLAG_WIRE_RIGHT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_DOWN, TILE_FLAG_WIRE_DOWN);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_LEFT, TILE_FLAG_WIRE_CLUSTER_LEFT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_MID, TILE_FLAG_WIRE_CLUSTER_MID);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_RIGHT, TILE_FLAG_WIRE_CLUSTER_RIGHT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_LEFT_ON, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_MID_ON, TILE_FLAG_WIRE_CLUSTER_MID_ON);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_WIRE_CLUSTER_RIGHT_ON, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_FIRST_DIRECTION_BIT, TILE_FLAG_FIRST_DIRECTION_BIT);
               convert_bit_field<U16, U32>(map_tiles[index].flags, &tilemap->tiles[y][x].flags, TILE_FLAG_V1_SECOND_DIRECTION_BIT, TILE_FLAG_SECOND_DIRECTION_BIT);

               index++;
          }
     }

     tilemap_reset_light(tilemap);
     load_v1_blocks(block_array, map_blocks, block_count);
     load_v1_interactives(interactive_array, map_interactives, interactive_count);

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_v2_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV2_t* map_tiles = (MapTileV2_t*)(calloc(map_tile_count, sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc(block_count, sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc(interactive_count, sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), map_tile_count, file);
     fread(map_blocks, sizeof(*map_blocks), block_count, file);
     fread(map_interactives, sizeof(*map_interactives), interactive_count, file);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               tilemap->tiles[y][x].id = map_tiles[index].id;
               tilemap->tiles[y][x].flags = map_tiles[index].flags;
               index++;
          }
     }

     tilemap_reset_light(tilemap);

     load_v1_blocks(block_array, map_blocks, block_count);
     load_v1_interactives(interactive_array, map_interactives, interactive_count);

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                        ObjectArray_t<Interactive_t>* interactive_array, const char* filepath){
     // can we load this version ?
     U8 map_version = MAP_VERSION;
     fread(&map_version, sizeof(map_version), 1, file);

     switch(map_version){
     default:
          LOG("%s(): mismatched version loading '%s', actual %d, expected %d\n", __FUNCTION__, filepath, map_version, MAP_VERSION);
          break;
     case 1:
          return load_v1_map_from_file(file, player_start, tilemap, block_array, interactive_array);
     case 2:
          return load_v2_map_from_file(file, player_start, tilemap, block_array, interactive_array);
     }

     return false;

}

bool load_map(const char* filepath, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array){
     FILE* f = fopen(filepath, "rb");
     if(!f){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = load_map_from_file(f, player_start, tilemap, block_array, interactive_array, filepath);
     fclose(f);
     return success;
}

bool load_map_number(S32 map_number, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                     ObjectArray_t<Interactive_t>* interactive_array){
     // search through directory to find file starting with 3 digit map number
     DIR* d = opendir("content");
     if(!d) return false;
     struct dirent* dir;
     char filepath[64] = {};
     char match[4] = {};
     snprintf(match, 4, "%03d", map_number);
     while((dir = readdir(d)) != nullptr){
          if(strncmp(dir->d_name, match, 3) == 0 &&
             strstr(dir->d_name, ".bm")){ // TODO: create strendswith() func for this?
               snprintf(filepath, 64, "content/%s", dir->d_name);
               break;
          }
     }

     closedir(d);

     if(!filepath[0]) return false;

     LOG("load map %s\n", filepath);
     return load_map(filepath, player_start, tilemap, block_array, interactive_array);
}

void draw_theme_frame(Vec_t tex_vec, Vec_t pos_vec){
     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos_vec.x, pos_vec.y);
     glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x, pos_vec.y + TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
}

void draw_double_theme_frame(Vec_t tex_vec, Vec_t pos_vec){
     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos_vec.x, pos_vec.y);
     glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
}

void tile_id_draw(U8 id, Vec_t pos){
     U8 id_x = id % 16;
     U8 id_y = id / 16;

     draw_theme_frame(theme_frame(id_x, id_y), pos);
}

void tile_flags_draw(U32 flags, Vec_t tile_pos){
     if(flags == 0) return;

     if(flags & TILE_FLAG_CHECKPOINT){
          draw_theme_frame(theme_frame(0, 21), tile_pos);
     }

     if(flags & TILE_FLAG_RESET_IMMUNE){
          draw_theme_frame(theme_frame(1, 21), tile_pos);
     }

     if(flags & (TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN)){
          S16 frame_y = 9;
          S16 frame_x = flags >> 6;

          if(flags & TILE_FLAG_WIRE_STATE) frame_y++;

          draw_theme_frame(theme_frame(frame_x, frame_y), tile_pos);
     }

     if(flags & (TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT)){
          S16 frame_y = 17 + tile_flags_cluster_direction(flags);
          S16 frame_x = 0;

          if(flags & TILE_FLAG_WIRE_CLUSTER_LEFT){
               if(flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON){
                    frame_x = 1;
               }else{
                    frame_x = 0;
               }

               draw_theme_frame(theme_frame(frame_x, frame_y), tile_pos);
          }

          if(flags & TILE_FLAG_WIRE_CLUSTER_MID){
               if(flags & TILE_FLAG_WIRE_CLUSTER_MID_ON){
                    frame_x = 3;
               }else{
                    frame_x = 2;
               }

               draw_theme_frame(theme_frame(frame_x, frame_y), tile_pos);
          }

          if(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT){
               if(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON){
                    frame_x = 5;
               }else{
                    frame_x = 4;
               }

               draw_theme_frame(theme_frame(frame_x, frame_y), tile_pos);
          }
     }
}

void tile_ice_draw(U32 flags, Vec_t pos, GLuint theme_texture){
     if(tile_flags_have_ice(flags)){
          glEnd();

          // get state ready for ice
          glBindTexture(GL_TEXTURE_2D, 0);
          glColor4f(196.0f / 255.0f, 217.0f / 255.0f, 1.0f, 0.45f);
          glBegin(GL_QUADS);

          if(flags & TILE_FLAG_ICED_TOP_LEFT){
               glVertex2f(pos.x, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x, pos.y + TILE_SIZE);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + TILE_SIZE);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + HALF_TILE_SIZE);
          }

          if(flags & TILE_FLAG_ICED_TOP_RIGHT){
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + TILE_SIZE);
               glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
               glVertex2f(pos.x + TILE_SIZE, pos.y + HALF_TILE_SIZE);
          }

          if(flags & TILE_FLAG_ICED_BOTTOM_LEFT){
               glVertex2f(pos.x, pos.y);
               glVertex2f(pos.x, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y);
          }

          if(flags & TILE_FLAG_ICED_BOTTOM_RIGHT){
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y);
               glVertex2f(pos.x + HALF_TILE_SIZE, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x + TILE_SIZE, pos.y + HALF_TILE_SIZE);
               glVertex2f(pos.x + TILE_SIZE, pos.y);
          }

          glEnd();

          // reset state back to default
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
     }

}

void block_draw(Block_t* block, Vec_t pos_vec){
     Vec_t tex_vec = theme_frame(0, 6);
     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos_vec.x, pos_vec.y);
     glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);

     if(block->element == ELEMENT_ONLY_ICED || block->element == ELEMENT_ICE ){
          tex_vec = theme_frame(4, 12);
          glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x, pos_vec.y);
          glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
          glColor3f(1.0f, 1.0f, 1.0f);
     }

     if(block->element == ELEMENT_FIRE){
          tex_vec = theme_frame(1, 6);
          // TODO: compress
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x, pos_vec.y);
          glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
     }else if(block->element == ELEMENT_ICE){
          tex_vec = theme_frame(5, 6);
          // TODO: compress
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x, pos_vec.y);
          glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
     }
}

void interactive_draw(Interactive_t* interactive, Vec_t pos_vec){
     Vec_t tex_vec = {};
     switch(interactive->type){
     default:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
     {
          int frame_x = 7;
          if(interactive->pressure_plate.down) frame_x++;
          tex_vec = theme_frame(frame_x, 8);
          draw_theme_frame(tex_vec, pos_vec);
     } break;
     case INTERACTIVE_TYPE_LEVER:
          tex_vec = theme_frame(0, 12);
          draw_double_theme_frame(tex_vec, pos_vec);
          break;
     case INTERACTIVE_TYPE_BOW:
          draw_theme_frame(theme_frame(0, 9), pos_vec);
          break;
     case INTERACTIVE_TYPE_POPUP:
          tex_vec = theme_frame(interactive->popup.lift.ticks - 1, 8);
          draw_double_theme_frame(tex_vec, pos_vec);
          break;
     case INTERACTIVE_TYPE_DOOR:
          tex_vec = theme_frame(interactive->door.lift.ticks + 8, 11 + interactive->door.face);
          draw_theme_frame(tex_vec, pos_vec);
          break;
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          draw_theme_frame(theme_frame(1, 11), pos_vec);
          if(interactive->detector.on){
               draw_theme_frame(theme_frame(2, 11), pos_vec);
          }
          break;
     case INTERACTIVE_TYPE_ICE_DETECTOR:
          draw_theme_frame(theme_frame(1, 12), pos_vec);
          if(interactive->detector.on){
               draw_theme_frame(theme_frame(2, 12), pos_vec);
          }
          break;
     case INTERACTIVE_TYPE_PORTAL:
          if(!interactive->portal.on){
               S16 frame_x = 0;
               S16 frame_y = 0;

               switch(interactive->portal.face){
               default:
                    break;
               case DIRECTION_LEFT:
                    frame_x = 3;
                    frame_y = 1;
                    break;
               case DIRECTION_UP:
                    frame_x = 0;
                    frame_y = 2;
                    break;
               case DIRECTION_RIGHT:
                    frame_x = 2;
                    frame_y = 2;
                    break;
               case DIRECTION_DOWN:
                    frame_x = 1;
                    frame_y = 1;
                    break;
               }

               draw_theme_frame(theme_frame(frame_x, frame_y), pos_vec);
          }

          draw_theme_frame(theme_frame(interactive->portal.face, 26 + interactive->portal.on), pos_vec);
          break;
     }

}

struct Quad_t{
     F32 left;
     F32 bottom;
     F32 right;
     F32 top;
};

void draw_quad_wireframe(const Quad_t* quad, F32 red, F32 green, F32 blue){
     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

     glBegin(GL_QUADS);
     glColor3f(red, green, blue);
     glVertex2f(quad->left,  quad->top);
     glVertex2f(quad->left,  quad->bottom);
     glVertex2f(quad->right, quad->bottom);
     glVertex2f(quad->right, quad->top);
     glEnd();

     glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void selection_draw(Coord_t selection_start, Coord_t selection_end, Position_t camera, F32 red, F32 green, F32 blue){
     if(selection_start.x > selection_end.x) SWAP(selection_start.x, selection_end.x);
     if(selection_start.y > selection_end.y) SWAP(selection_start.y, selection_end.y);

     Position_t start_location = coord_to_pos(selection_start) - camera;
     Position_t end_location = coord_to_pos(selection_end) - camera;
     Vec_t start_vec = pos_to_vec(start_location);
     Vec_t end_vec = pos_to_vec(end_location);

     Quad_t selection_quad {start_vec.x, start_vec.y, end_vec.x + TILE_SIZE, end_vec.y + TILE_SIZE};
     glBindTexture(GL_TEXTURE_2D, 0);
     draw_quad_wireframe(&selection_quad, red, green, blue);
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

S32 mouse_select_index(Vec_t mouse_screen){
     Coord_t coord = mouse_select_coord(mouse_screen);
     return coord.y * ROOM_TILE_SIZE + coord.x;
}

Vec_t coord_to_screen_position(Coord_t coord){
     Pixel_t pixel = coord_to_pixel(coord);
     Position_t relative_loc {pixel, 0, {0.0f, 0.0f}};
     return pos_to_vec(relative_loc);
}

void apply_stamp(Stamp_t* stamp, Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>** interactive_quad_tree, bool combine){
     switch(stamp->type){
     default:
          break;
     case STAMP_TYPE_TILE_ID:
     {
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          if(tile) tile->id = stamp->tile_id;
     } break;
     case STAMP_TYPE_TILE_FLAGS:
     {
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          if(tile){
               if(combine){
                    tile->flags |= stamp->tile_flags;
               }else{
                    tile->flags = stamp->tile_flags;
               }
          }
     } break;
     case STAMP_TYPE_BLOCK:
     {
          int index = block_array->count;
          resize(block_array, block_array->count + 1);
          // TODO: Check if block is in the way with the quad tree

          Block_t* block = block_array->elements + index;
          *block = {};
          block->pos = coord_to_pos(coord);
          block->vel = vec_zero();
          block->accel = vec_zero();
          block->element = stamp->block.element;
          block->face = stamp->block.face;
     } break;
     case STAMP_TYPE_INTERACTIVE:
     {
          Interactive_t* interactive = quad_tree_interactive_find_at(*interactive_quad_tree, coord);
          if(interactive) return;

          int index = interactive_array->count;
          resize(interactive_array, interactive_array->count + 1);
          interactive_array->elements[index] = stamp->interactive;
          interactive_array->elements[index].coord = coord;
          quad_tree_free(*interactive_quad_tree);
          *interactive_quad_tree = quad_tree_build(interactive_array);
     } break;
     }
}

void coord_clear(Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(tile){
          tile->id = 0;
          tile->flags = 0;
     }

     auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(interactive){
          S16 index = interactive - interactive_array->elements;
          if(index >= 0){
               remove(interactive_array, index);
               quad_tree_free(interactive_quad_tree);
               interactive_quad_tree = quad_tree_build(interactive_array);
          }
     }

     S16 block_index = -1;
     for(S16 i = 0; i < block_array->count; i++){
          if(pos_to_coord(block_array->elements[i].pos) == coord){
               block_index = i;
               break;
          }
     }

     if(block_index >= 0){
          remove(block_array, block_index);
     }
}

Rect_t editor_selection_bounds(Editor_t* editor){
     Rect_t rect {};
     for(S16 i = 0; i < editor->selection.count; i++){
          auto* stamp = editor->selection.elements + i;
          if(rect.left > stamp->offset.x) rect.left = stamp->offset.x;
          if(rect.bottom > stamp->offset.y) rect.bottom = stamp->offset.y;
          if(rect.right < stamp->offset.x) rect.right = stamp->offset.x;
          if(rect.top < stamp->offset.y) rect.top = stamp->offset.y;
     }

     rect.left += editor->selection_start.x;
     rect.right += editor->selection_start.x;
     rect.top += editor->selection_start.y;
     rect.bottom += editor->selection_start.y;

     return rect;
}

S32 mouse_select_stamp_index(Coord_t screen_coord, ObjectArray_t<ObjectArray_t<Stamp_t>>* stamp_array){
     S32 index = -1;
     Rect_t current_rect = {};
     S16 row_height = 0;
     for(S16 i = 0; i < stamp_array->count; i++){
          Coord_t dimensions = stamp_array_dimensions(stamp_array->elements + i);

          if(row_height < dimensions.y) row_height = dimensions.y; // track max

          current_rect.right = current_rect.left + dimensions.x;
          current_rect.top = current_rect.bottom + dimensions.y;

          if(screen_coord.x >= current_rect.left && screen_coord.x < current_rect.right &&
             screen_coord.y >= current_rect.bottom && screen_coord.y < current_rect.top){
               index = i;
               break;
          }

          current_rect.left += dimensions.x;

          // wrap around to next row if necessary
          if(current_rect.left >= ROOM_TILE_SIZE){
               current_rect.left = 0;
               current_rect.bottom += row_height;
          }
     }
     return index;
}

FILE* load_demo_number(S32 map_number, const char** demo_filepath){
     char filepath[64] = {};
     snprintf(filepath, 64, "content/%03d.bd", map_number);
     *demo_filepath = strdup(filepath);
     return fopen(*demo_filepath, "rb");
}

void reset_map(Player_t* player, Coord_t player_start, ObjectArray_t<Interactive_t>* interactive_array,
               QuadTreeNode_t<Interactive_t>** interactive_quad_tree, ArrowArray_t* arrow_array){
     *player = {};
     player->walk_frame_delta = 1;
     player->pos = coord_to_pos_at_tile_center(player_start);
     player->has_bow = true;

     // update interactive quad tree
     quad_tree_free(*interactive_quad_tree);
     *interactive_quad_tree = quad_tree_build(interactive_array);
     init(arrow_array);
}

void position_move_against_block(Position_t pos, Position_t block_pos, Vec_t* pos_delta, bool* collide_with_block){
     Position_t relative = block_pos - pos;
     Vec_t bottom_left = pos_to_vec(relative);
     if(vec_magnitude(bottom_left) > (2 * TILE_SIZE)) return;

     Vec_t top_left {bottom_left.x, bottom_left.y + TILE_SIZE};
     Vec_t top_right {bottom_left.x + TILE_SIZE, bottom_left.y + TILE_SIZE};
     Vec_t bottom_right {bottom_left.x + TILE_SIZE, bottom_left.y};

     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, top_left, collide_with_block);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, top_left, top_right, collide_with_block);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_left, bottom_right, collide_with_block);
     *pos_delta += collide_circle_with_line(*pos_delta, PLAYER_RADIUS, bottom_right, top_right, collide_with_block);
}

Vec_t move_player_position_through_world(Position_t position, Vec_t pos_delta, Direction_t player_face, Coord_t* skip_coord,
                                         Player_t* player, TileMap_t* tilemap,
                                         QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array,
                                         Block_t** block_to_push, Direction_t* last_block_pushed_direction,
                                         bool* collided_with_interactive){
     // figure out tiles that are close by
     Position_t final_player_pos = position + pos_delta;
     Coord_t player_coord = pos_to_coord(final_player_pos);

     // NOTE: this assumes the player can't move more than 1 coord in 1 frame
     Coord_t min = player_coord - Coord_t{1, 1};
     Coord_t max = player_coord + Coord_t{1, 1};
     // S8 player_top = player.pos.z + 2 * HEIGHT_INTERVAL;
     min = coord_clamp_zero_to_dim(min, tilemap->width - 1, tilemap->height - 1);
     max = coord_clamp_zero_to_dim(max, tilemap->width - 1, tilemap->height - 1);

     // TODO: convert to use quad tree
     Vec_t collided_block_delta {};
     Direction_t collided_block_dir = DIRECTION_COUNT;
     Block_t* collided_block = nullptr;
     for(S16 i = 0; i < block_array->count; i++){
          Vec_t pos_delta_save = pos_delta;

          bool collide_with_block = false;
          Position_t block_pos = block_array->elements[i].pos;
          position_move_against_block(position, block_pos, &pos_delta, &collide_with_block);
          Coord_t block_coord = block_get_coord(block_array->elements + i);
          U8 portal_rotations = 0;

          if(!collide_with_block){
               // check if the block is in a portal and try to collide with it
               Vec_t coord_offset = pos_to_vec(block_array->elements[i].pos +
                                               pixel_to_pos(Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS}) -
                                               coord_to_pos_at_tile_center(block_coord));
               for(S8 r = 0; r < DIRECTION_COUNT && !collide_with_block; r++){
                    Coord_t check_coord = block_coord + (Direction_t)(r);
                    Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, check_coord);

                    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                         PortalExit_t portal_exits = find_portal_exits(check_coord, tilemap, interactive_quad_tree);

                         for(S8 d = 0; d < DIRECTION_COUNT && !collide_with_block; d++){
                              Vec_t final_coord_offset = rotate_vec_between_dirs(interactive->portal.face, (Direction_t)(d), coord_offset);

                              for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                                   if(portal_exits.directions[d].coords[p] == check_coord) continue;

                                   Position_t portal_pos = coord_to_pos_at_tile_center(portal_exits.directions[d].coords[p]) + final_coord_offset -
                                                           pixel_to_pos(Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
                                   position_move_against_block(position, portal_pos, &pos_delta, &collide_with_block);

                                   if(collide_with_block){
                                        block_pos = portal_pos;
                                        if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                                             player_face = direction_opposite(interactive->portal.face);
                                             portal_rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));
                                             break;
                                        }
                                   }
                              }
                         }
                    }
               }
          }

          if(collide_with_block){
               collided_block = block_array->elements + i;
               Vec_t pos_delta_diff = pos_delta - pos_delta_save;
               collided_block_delta = vec_rotate_quadrants(pos_delta_diff, 4 - portal_rotations);
               collided_block_dir = relative_quadrant(position.pixel, block_pos.pixel +
                                                      Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
               Vec_t rotated_accel = vec_rotate_quadrants(collided_block->accel, portal_rotations);
               Vec_t rotated_vel = vec_rotate_quadrants(collided_block->vel, portal_rotations);

               Position_t pre_move = collided_block->pos;

               switch(collided_block_dir){
               default:
                    break;
               case DIRECTION_LEFT:
                    if(rotated_accel.x > 0.0f){
                         collided_block->pos -= collided_block_delta;
                         pos_delta -= pos_delta_diff;
                         rotated_accel.x = 0.0f;
                         rotated_vel.x = 0.0f;
                         U8 unrotations = 4 - portal_rotations;
                         collided_block->accel = vec_rotate_quadrants(rotated_accel, unrotations);
                         collided_block->vel = vec_rotate_quadrants(rotated_vel, unrotations);
                    }
                    break;
               case DIRECTION_RIGHT:
                    if(rotated_accel.x < 0.0f){
                         collided_block->pos -= collided_block_delta;
                         pos_delta -= pos_delta_diff;
                         rotated_accel.x = 0.0f;
                         rotated_vel.x = 0.0f;
                         U8 unrotations = 4 - portal_rotations;
                         collided_block->accel = vec_rotate_quadrants(rotated_accel, unrotations);
                         collided_block->vel = vec_rotate_quadrants(rotated_vel, unrotations);
                    }
                    break;
               case DIRECTION_UP:
                    if(rotated_accel.y < 0.0f){
                         collided_block->pos -= collided_block_delta;
                         pos_delta -= pos_delta_diff;
                         rotated_accel.y = 0.0f;
                         rotated_vel.y = 0.0f;
                         U8 unrotations = 4 - portal_rotations;
                         collided_block->accel = vec_rotate_quadrants(rotated_accel, unrotations);
                         collided_block->vel = vec_rotate_quadrants(rotated_vel, unrotations);
                    }
                    break;
               case DIRECTION_DOWN:
                    if(rotated_accel.y > 0.0f){
                         collided_block->pos -= collided_block_delta;
                         pos_delta -= pos_delta_diff;
                         rotated_accel.y = 0.0f;
                         rotated_vel.y = 0.0f;
                         U8 unrotations = 4 - portal_rotations;
                         collided_block->accel = vec_rotate_quadrants(rotated_accel, unrotations);
                         collided_block->vel = vec_rotate_quadrants(rotated_vel, unrotations);
                    }
                    break;
               }

               Coord_t coord = pixel_to_coord(collided_block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
               Coord_t premove_coord = pixel_to_coord(pre_move.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});

               Position_t block_center = collided_block->pos;
               block_center.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               S8 rotations_between = teleport_position_across_portal(&block_center, NULL, interactive_quad_tree, tilemap, premove_coord,
                                                                      coord);
               if(rotations_between >= 0){
                    collided_block->pos = block_center;
                    collided_block->pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               }

               auto rotated_player_face = direction_rotate_clockwise(player_face, portal_rotations);
               if(collided_block_dir == rotated_player_face && (player->accel.x != 0.0f || player->accel.y != 0.0f) &&
                  *block_to_push == nullptr){ // also check that the player is actually pushing against the block
                    *block_to_push = block_array->elements + i;
                    *last_block_pushed_direction = player_face;
               }
          }
     }

     Direction_t collided_tile_dir = DIRECTION_COUNT;
     for(S16 y = min.y; y <= max.y; y++){
          for(S16 x = min.x; x <= max.x; x++){
               if(tilemap->tiles[y][x].id){
                    Coord_t coord {x, y};
                    bool skip = false;
                    for(S16 d = 0; d < DIRECTION_COUNT; d++){
                         if(skip_coord[d] == coord){
                              skip = true;
                              break;
                         }
                    }
                    if(skip) continue;
                    bool collide_with_tile = false;
                    player_collide_coord(position, coord, PLAYER_RADIUS, &pos_delta, &collide_with_tile);
                    if(collide_with_tile) collided_tile_dir = direction_between(player_coord, coord);
               }
          }
     }

     Direction_t collided_interactive_dir = DIRECTION_COUNT;
     *collided_with_interactive = false;
     for(S16 y = min.y; y <= max.y; y++){
          for(S16 x = min.x; x <= max.x; x++){
               Coord_t coord {x, y};

               Interactive_t* interactive = quad_tree_interactive_solid_at(interactive_quad_tree, tilemap, coord);
               if(interactive){
                    bool collided = false;
                    player_collide_coord(position, coord, PLAYER_RADIUS, &pos_delta, &collided);
                    if(collided && !(*collided_with_interactive)){
                         *collided_with_interactive = true;
                         collided_interactive_dir = direction_between(player_coord, coord);
                    }
               }
          }
     }

     if(collided_block_dir != DIRECTION_COUNT &&
        (collided_block_dir == direction_opposite(collided_interactive_dir) ||
         collided_block_dir == direction_opposite(collided_tile_dir))){

          switch(collided_block_dir){
          default:
               break;
          case DIRECTION_LEFT:
               if(collided_block->accel.x > 0){
                    collided_block->pos -= collided_block_delta;
                    collided_block->accel.x = 0.0f;
                    collided_block->vel.x = 0.0f;
               }
               break;
          case DIRECTION_RIGHT:
               if(collided_block->accel.x < 0){
                    collided_block->pos -= collided_block_delta;
                    collided_block->accel.x = 0.0f;
                    collided_block->vel.x = 0.0f;
               }
               break;
          case DIRECTION_DOWN:
               if(collided_block->accel.y > 0){
                    collided_block->pos -= collided_block_delta;
                    collided_block->accel.y = 0.0f;
                    collided_block->vel.y = 0.0f;
               }
               break;
          case DIRECTION_UP:
               if(collided_block->accel.y < 0){
                    collided_block->pos -= collided_block_delta;
                    collided_block->accel.y = 0.0f;
                    collided_block->vel.y = 0.0f;
               }
               break;
          }
     }

     return pos_delta;
}

void draw_flats(Vec_t pos, Tile_t* tile, Interactive_t* interactive, GLuint theme_texture,
                U8 portal_rotations){
     tile_id_draw(tile->id, pos);

     U32 tile_flags = tile->flags;
     for(U8 i = 0; i < portal_rotations; i++){
          U32 new_flags = tile_flags & ~(TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN);

          if(tile_flags & TILE_FLAG_WIRE_LEFT) new_flags |= TILE_FLAG_WIRE_UP;
          if(tile_flags & TILE_FLAG_WIRE_UP) new_flags |= TILE_FLAG_WIRE_RIGHT;
          if(tile_flags & TILE_FLAG_WIRE_RIGHT) new_flags |= TILE_FLAG_WIRE_DOWN;
          if(tile_flags & TILE_FLAG_WIRE_DOWN) new_flags |= TILE_FLAG_WIRE_LEFT;

          tile_flags = new_flags;
     }

     tile_flags_draw(tile_flags, pos);

     // draw flat interactives that could be covered by ice
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
               if(!tile_is_iced(tile) && interactive->pressure_plate.iced_under){
                    // TODO: compress with above ice drawing code
                    glEnd();

                    // get state ready for ice
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glColor4f(196.0f / 255.0f, 217.0f / 255.0f, 1.0f, 0.45f);
                    glBegin(GL_QUADS);
                    glVertex2f(pos.x, pos.y);
                    glVertex2f(pos.x, pos.y + TILE_SIZE);
                    glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
                    glVertex2f(pos.x + TILE_SIZE, pos.y);
                    glEnd();

                    // reset state back to default
                    glBindTexture(GL_TEXTURE_2D, theme_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }

               interactive_draw(interactive, pos);
          }else if(interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks == 1){
               if(interactive->popup.iced){
                    pos.y += interactive->popup.lift.ticks * PIXEL_SIZE;
                    Vec_t tex_vec = theme_frame(3, 12);
                    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(pos.x, pos.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(pos.x, pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(pos.x + TILE_SIZE, pos.y);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }

               interactive_draw(interactive, pos);
          }else if(interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR ||
                   interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR){
               interactive_draw(interactive, pos);
          }
     }

     tile_ice_draw(tile->flags, pos, theme_texture);
}

void draw_solids(Vec_t pos, Interactive_t* interactive, Block_t** blocks, S16 block_count, Player_t* player,
                 Position_t screen_camera, GLuint theme_texture, GLuint player_texture,
                 Coord_t source_coord, Coord_t destination_coord, U8 portal_rotations){
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE ||
             interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR ||
             interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR ||
             (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks == 1)){
               // pass, these are flat
          }else{
               interactive_draw(interactive, pos);

               if(interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.iced){
                    pos.y += interactive->popup.lift.ticks * PIXEL_SIZE;
                    Vec_t tex_vec = theme_frame(3, 12);
                    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(pos.x, pos.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(pos.x, pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(pos.x + TILE_SIZE, pos.y);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }
          }
     }

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          Position_t block_camera_offset = block->pos;
          block_camera_offset.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
          if(destination_coord.x >= 0){
               Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
               Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
               Position_t center_delta = block_camera_offset - source_pos;
               center_delta = position_rotate_quadrants(center_delta, portal_rotations);
               block_camera_offset = destination_pos + center_delta;
          }

          block_camera_offset.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
          block_camera_offset -= screen_camera;
          block_camera_offset.pixel.y += block->pos.z;
          block_draw(block, pos_to_vec(block_camera_offset));
     }

     if(player){
          Vec_t pos_vec = pos_to_vec(player->pos);
          if(destination_coord.x >= 0){
               Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
               Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
               Position_t center_delta = player->pos - source_pos;
               center_delta = position_rotate_quadrants(center_delta, portal_rotations);
               pos_vec = pos_to_vec(destination_pos + center_delta);
          }

          S8 player_frame_y = direction_rotate_clockwise(player->face, portal_rotations);
          if(player->push_time > FLT_EPSILON) player_frame_y += 4;
          if(player->has_bow){
               player_frame_y += 8;
               if(player->bow_draw_time > (PLAYER_BOW_DRAW_DELAY / 2.0f)){
                    player_frame_y += 8;
                    if(player->bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                         player_frame_y += 4;
                    }
               }
          }
          Vec_t tex_vec = player_frame(player->walk_frame, player_frame_y);
          pos_vec.y += (5.0f * PIXEL_SIZE);

          glEnd();
          glBindTexture(GL_TEXTURE_2D, player_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x, tex_vec.y + PLAYER_FRAME_HEIGHT);
          glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y + PLAYER_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
          glEnd();

          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
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


void check_block_collision_with_other_blocks(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree,
                                             QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                             Player_t* player, Block_t* last_block_pushed, Direction_t last_block_pushed_direction){
     for(BlockInsideResult_t block_inside_result = block_inside_another_block(block_to_check, block_quad_tree, interactive_quad_tree, tilemap);
         block_inside_result.block && blocks_at_collidable_height(block_to_check, block_inside_result.block);
         block_inside_result = block_inside_another_block(block_to_check, block_quad_tree, interactive_quad_tree, tilemap)){

#if 0
          (void)(player);
          (void)(last_block_pushed);
          (void)(last_block_pushed_direction);

          // TODO: remove, this is just for debuging exactly when collisions occur
          block_to_check->vel.x = 0.0f;
          block_to_check->accel.x = 0.0f;
          block_to_check->vel.y = 0.0f;
          block_to_check->accel.y = 0.0f;

          block_inside_result.block->vel.x = 0.0f;
          block_inside_result.block->accel.x = 0.0f;
          block_inside_result.block->vel.y = 0.0f;
          block_inside_result.block->accel.y = 0.0f;
#else
          auto block_center_pixel = block_to_check->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
          auto quadrant = relative_quadrant(block_center_pixel, block_inside_result.collision_pos.pixel);

          // check if they are on ice before we adjust the position on our block to check
          bool a_on_ice = block_on_ice(block_to_check, tilemap, interactive_quad_tree);
          bool b_on_ice = block_on_ice(block_inside_result.block, tilemap, interactive_quad_tree);

          if(block_inside_result.block == block_to_check){
               block_to_check->pos = coord_to_pos(block_get_coord(block_to_check));
          }else{
               switch(quadrant){
               default:
                    break;
               case DIRECTION_LEFT:
                    block_to_check->pos.pixel.x = block_inside_result.collision_pos.pixel.x + HALF_TILE_SIZE_IN_PIXELS;
                    block_to_check->pos.decimal.x = 0.0f;
                    block_to_check->vel.x = 0.0f;
                    block_to_check->accel.x = 0.0f;
                    break;
               case DIRECTION_RIGHT:
                    block_to_check->pos.pixel.x = block_inside_result.collision_pos.pixel.x - HALF_TILE_SIZE_IN_PIXELS - TILE_SIZE_IN_PIXELS;
                    block_to_check->pos.decimal.x = 0.0f;
                    block_to_check->vel.x = 0.0f;
                    block_to_check->accel.x = 0.0f;
                    break;
               case DIRECTION_DOWN:
                    block_to_check->pos.pixel.y = block_inside_result.collision_pos.pixel.y + HALF_TILE_SIZE_IN_PIXELS;
                    block_to_check->pos.decimal.y = 0.0f;
                    block_to_check->vel.y = 0.0f;
                    block_to_check->accel.y = 0.0f;
                    break;
               case DIRECTION_UP:
                    block_to_check->pos.pixel.y = block_inside_result.collision_pos.pixel.y - HALF_TILE_SIZE_IN_PIXELS - TILE_SIZE_IN_PIXELS;
                    block_to_check->pos.decimal.y = 0.0f;
                    block_to_check->vel.y = 0.0f;
                    block_to_check->accel.y = 0.0f;
                    break;
               }
          }

          if(block_to_check == last_block_pushed && quadrant == last_block_pushed_direction){
               player->push_time = 0.0f;
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

               if(push) block_push(block_inside_result.block, push_dir, tilemap, interactive_quad_tree, block_quad_tree, true);
          }
#endif

          // TODO: there is no way this is the right way to do this
          if(block_inside_result.block == block_to_check) break;
     }
}

using namespace std::chrono;

void describe_coord(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, QuadTreeNode_t<Block_t>* block_quad_tree){
     (void)(block_quad_tree);

     LOG("\ndescribe_coord(%d, %d)\n", coord.x, coord.y);
     auto* tile = tilemap_get_tile(tilemap, coord);
     if(tile){
          Half_t half = coord_to_half(coord);
          auto* light_top_left = tilemap_get_light(tilemap, half);
          auto* light_top_right = tilemap_get_light(tilemap, half + Half_t{1, 0});
          auto* light_bottom_left = tilemap_get_light(tilemap, half + Half_t{0, 1});
          auto* light_bottom_right = tilemap_get_light(tilemap, half + Half_t{1, 1});
          LOG("Tile: id: %u, light: {%u, %u, %u, %u}\n", tile->id, *light_top_left, *light_top_right,
              *light_bottom_left, *light_bottom_right);
          if(tile->flags){
               LOG(" flags:\n");
               if(tile->flags & TILE_FLAG_ICED_TOP_LEFT) printf("  ICED TOP LEFT\n");
               if(tile->flags & TILE_FLAG_ICED_TOP_RIGHT) printf("  ICED TOP RIGHT\n");
               if(tile->flags & TILE_FLAG_ICED_BOTTOM_LEFT) printf("  ICED BOTTOM LEFT\n");
               if(tile->flags & TILE_FLAG_ICED_BOTTOM_RIGHT) printf("  ICED BOTTOM RIGHT\n");
               if(tile->flags & TILE_FLAG_CHECKPOINT) printf("  CHECKPOINT\n");
               if(tile->flags & TILE_FLAG_RESET_IMMUNE) printf("  RESET_IMMUNE\n");
               if(tile->flags & TILE_FLAG_WIRE_STATE) printf("  WIRE_STATE\n");
               if(tile->flags & TILE_FLAG_WIRE_LEFT) printf("  WIRE_LEFT\n");
               if(tile->flags & TILE_FLAG_WIRE_UP) printf("  WIRE_UP\n");
               if(tile->flags & TILE_FLAG_WIRE_RIGHT) printf("  WIRE_RIGHT\n");
               if(tile->flags & TILE_FLAG_WIRE_DOWN) printf("  WIRE_DOWN\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) printf("  CLUSTER_LEFT\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) printf("  CLUSTER_MID\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) printf("  CLUSTER_RIGHT\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON) printf("  CLUSTER_LEFT_ON\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID_ON) printf("  CLUSTER_MID_ON\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON) printf("  CLUSTER_RIGHT_ON\n");
          }
     }

     auto* interactive = quad_tree_find_at(interactive_quad_tree, coord.x, coord.y);
     if(interactive){
          const char* type_string = "INTERACTIVE_TYPE_UKNOWN";
          const int info_string_len = 32;
          char info_string[info_string_len];
          memset(info_string, 0, info_string_len);

          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_NONE:
               type_string = "NONE";
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               type_string = "PRESSURE_PLATE";
               snprintf(info_string, info_string_len, "down: %d, iced_undo: %d",
                        interactive->pressure_plate.down, interactive->pressure_plate.iced_under);
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
               type_string = "LIGHT_DETECTOR";
               snprintf(info_string, info_string_len, "on: %d", interactive->detector.on);
               break;
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               type_string = "ICE_DETECTOR";
               snprintf(info_string, info_string_len, "on: %d", interactive->detector.on);
               break;
          case INTERACTIVE_TYPE_POPUP:
               type_string = "POPUP";
               snprintf(info_string, info_string_len, "lift: ticks: %u, up: %d, iced: %d", interactive->popup.lift.ticks, interactive->popup.lift.up, interactive->popup.iced);
               break;
          case INTERACTIVE_TYPE_LEVER:
               type_string = "LEVER";
               break;
          case INTERACTIVE_TYPE_DOOR:
               type_string = "DOOR";
               snprintf(info_string, info_string_len, "face: %s, lift: ticks %u, up: %d",
                        direction_to_string(interactive->door.face), interactive->door.lift.ticks,
                        interactive->door.lift.up);
               break;
          case INTERACTIVE_TYPE_PORTAL:
               type_string = "PORTAL";
               snprintf(info_string, info_string_len, "face: %s, on: %d", direction_to_string(interactive->portal.face),
                        interactive->portal.on);
               break;
          case INTERACTIVE_TYPE_BOMB:
               type_string = "BOMB";
               break;
          case INTERACTIVE_TYPE_BOW:
               type_string = "BOW";
               break;
          case INTERACTIVE_TYPE_STAIRS:
               type_string = "STAIRS";
               break;
          case INTERACTIVE_TYPE_PROMPT:
               type_string = "PROMPT";
               break;
          }

          LOG("type: %s %s\n", type_string, info_string);
     }

     Rect_t coord_rect;

     coord_rect.left = coord.x * TILE_SIZE_IN_PIXELS;
     coord_rect.bottom = coord.y * TILE_SIZE_IN_PIXELS;
     coord_rect.right = coord_rect.left + TILE_SIZE_IN_PIXELS;
     coord_rect.top = coord_rect.bottom + TILE_SIZE_IN_PIXELS;

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     for(S16 i = 0; i < block_count; i++){
          auto* block = blocks[i];
          LOG("block: %d, %d, dir: %s, element: %s\n", block->pos.pixel.x, block->pos.pixel.y,
              direction_to_string(block->face), element_to_string(block->element));
     }
}

int main(int argc, char** argv){
     DemoMode_t demo_mode = DEMO_MODE_NONE;
     const char* demo_filepath = nullptr;
     const char* load_map_filepath = nullptr;
     bool test = false;
     bool suite = false;
     bool show_suite = false;
     S16 map_number = -1;
     S16 first_map_number = 0;
     S16 map_count = 0;

     for(int i = 1; i < argc; i++){
          if(strcmp(argv[i], "-play") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_PLAY;
          }else if(strcmp(argv[i], "-record") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_RECORD;
          }else if(strcmp(argv[i], "-load") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               load_map_filepath = argv[next];
          }else if(strcmp(argv[i], "-test") == 0){
               test = true;
          }else if(strcmp(argv[i], "-suite") == 0){
               test = true;
               suite = true;
          }else if(strcmp(argv[i], "-show") == 0){
               show_suite = true;
          }else if(strcmp(argv[i], "-map") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               map_number = atoi(argv[next]);
               first_map_number = map_number;
          }else if(strcmp(argv[i], "-count") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               map_count = atoi(argv[next]);
          }
     }

     const char* log_path = "bryte.log";
     if(!Log_t::create(log_path)){
          fprintf(stderr, "failed to create log file: '%s'\n", log_path);
          return -1;
     }

     if(test && !load_map_filepath && !suite){
          LOG("cannot test without specifying a map to load\n");
          return 1;
     }

     int window_width = 1024;
     int window_height = 1024;
     SDL_Window* window = nullptr;
     SDL_GLContext opengl_context = 0;
     GLuint theme_texture = 0;
     GLuint player_texture = 0;
     GLuint arrow_texture = 0;

     if(!suite || show_suite){
          if(SDL_Init(SDL_INIT_EVERYTHING) != 0){
               return 1;
          }

          SDL_DisplayMode display_mode;
          if(SDL_GetCurrentDisplayMode(0, &display_mode) != 0){
               return 1;
          }

          LOG("Create window: %d, %d\n", window_width, window_height);
          window = SDL_CreateWindow("bryte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL);
          if(!window) return 1;

          opengl_context = SDL_GL_CreateContext(window);

          SDL_GL_SetSwapInterval(SDL_TRUE);
          glViewport(0, 0, window_width, window_height);
          glClearColor(0.0, 0.0, 0.0, 1.0);
          glEnable(GL_TEXTURE_2D);
          glViewport(0, 0, window_width, window_height);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          glBlendEquation(GL_FUNC_ADD);

          theme_texture = transparent_texture_from_file("content/theme.bmp");
          if(theme_texture == 0) return 1;
          player_texture = transparent_texture_from_file("content/player.bmp");
          if(player_texture == 0) return 1;
          arrow_texture = transparent_texture_from_file("content/arrow.bmp");
          if(arrow_texture == 0) return 1;
     }

#if 0 // TODO: do we want this in the future?
     Rect_t rooms[2];
     {
          rooms[0].left = 0;
          rooms[0].bottom = 0;
          rooms[0].top = 16;
          rooms[0].right = 16;

          rooms[1].left = 17;
          rooms[1].bottom = 5;
          rooms[1].top = 15;
          rooms[1].right = 27;
     }
#endif

     FILE* demo_file = nullptr;
     DemoEntry_t demo_entry {};
     switch(demo_mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          demo_file = fopen(demo_filepath, "w");
          if(!demo_file){
               LOG("failed to open demo file: %s\n", demo_filepath);
               return 1;
          }
          // TODO: write header
          break;
     case DEMO_MODE_PLAY:
          demo_file = fopen(demo_filepath, "r");
          if(!demo_file){
               LOG("failed to open demo file: %s\n", demo_filepath);
               return 1;
          }
          LOG("playing demo %s\n", demo_filepath);
          // TODO: read header
          demo_entry_get(&demo_entry, demo_file);
          break;
     }

     TileMap_t tilemap = {};
     ObjectArray_t<Block_t> block_array = {};
     ObjectArray_t<Interactive_t> interactive_array = {};
     ArrowArray_t arrow_array = {};
     Coord_t player_start {2, 8};

     if(load_map_filepath){
          if(!load_map(load_map_filepath, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }
     }else if(suite){
          if(map_number == -1) map_number = 0;

          if(!load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }

          demo_mode = DEMO_MODE_PLAY;
          demo_file = load_demo_number(map_number, &demo_filepath);
          if(!demo_file){
               LOG("missing map %d corresponding demo.\n", map_number);
               return 1;
          }
          LOG("testing demo %s\n", demo_filepath);
          demo_entry_get(&demo_entry, demo_file);
     }else if(map_number >= 0){
          if(!load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }
     }else{
          init(&tilemap, ROOM_TILE_SIZE, ROOM_TILE_SIZE);

          for(S16 i = 0; i < tilemap.width; i++){
               tilemap.tiles[0][i].id = 33;
               tilemap.tiles[1][i].id = 17;
               tilemap.tiles[tilemap.height - 1][i].id = 16;
               tilemap.tiles[tilemap.height - 2][i].id = 32;
          }

          for(S16 i = 0; i < tilemap.height; i++){
               tilemap.tiles[i][0].id = 18;
               tilemap.tiles[i][1].id = 19;
               tilemap.tiles[i][tilemap.width - 2].id = 34;
               tilemap.tiles[i][tilemap.height - 1].id = 35;
          }

          tilemap.tiles[0][0].id = 36;
          tilemap.tiles[0][1].id = 37;
          tilemap.tiles[1][0].id = 20;
          tilemap.tiles[1][1].id = 21;

          tilemap.tiles[16][0].id = 22;
          tilemap.tiles[16][1].id = 23;
          tilemap.tiles[15][0].id = 38;
          tilemap.tiles[15][1].id = 39;

          tilemap.tiles[15][15].id = 40;
          tilemap.tiles[15][16].id = 41;
          tilemap.tiles[16][15].id = 24;
          tilemap.tiles[16][16].id = 25;

          tilemap.tiles[0][15].id = 42;
          tilemap.tiles[0][16].id = 43;
          tilemap.tiles[1][15].id = 26;
          tilemap.tiles[1][16].id = 27;
          if(!init(&interactive_array, 1)){
               return 1;
          }
          interactive_array.elements[0].coord.x = -1;
          interactive_array.elements[0].coord.y = -1;

          if(!init(&block_array, 1)){
               return 1;
          }
          block_array.elements[0].pos = coord_to_pos(Coord_t{-1, -1});
     }

     Player_t player;
     QuadTreeNode_t<Interactive_t>* interactive_quad_tree = nullptr;
     QuadTreeNode_t<Block_t>* block_quad_tree = nullptr;

     reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);

     Undo_t undo = {};
     init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);

     undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);

     bool quit = false;

     float resetting = 0;
     bool has_reset = false;

     Vec_t user_movement = {};
     PlayerAction_t player_action {};

     auto last_time = system_clock::now();
     auto current_time = last_time;

     Position_t camera = coord_to_pos(Coord_t{8, 8});

     Block_t* last_block_pushed = nullptr;
     Direction_t last_block_pushed_direction = DIRECTION_LEFT;
     Block_t* block_to_push = nullptr;

     // bool left_click_down = false;
     Vec_t mouse_screen = {}; // 0.0f to 1.0f
     Position_t mouse_world = {};
     bool ctrl_down;

     Editor_t editor;
     init(&editor);

     S64 frame_count = 0;
     F32 dt = 0.0f;

     while(!quit){
          if(!suite || show_suite){
               current_time = system_clock::now();
               duration<double> elapsed_seconds = current_time - last_time;
               dt = (F64)(elapsed_seconds.count());
               if(dt < 0.0166666f) continue; // limit 60 fps
          }

          // TODO: consider 30fps as minimum for random noobs computers
          //if(demo_mode) dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.
          dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.

          quad_tree_free(block_quad_tree);
          block_quad_tree = quad_tree_build(&block_array);

          frame_count++;

          last_time = current_time;

          last_block_pushed = block_to_push;
          block_to_push = nullptr;

          player_action.last_activate = player_action.activate;
          player_action.reface = false;

          if(demo_mode == DEMO_MODE_PLAY){
               bool end_of_demo = false;
               if(demo_entry.player_action_type == PLAYER_ACTION_TYPE_END_DEMO){
                    end_of_demo = (frame_count == demo_entry.frame);
               }else{
                    while(frame_count == demo_entry.frame && !feof(demo_file)){
                         player_action_perform(&player_action, &player, demo_entry.player_action_type, demo_mode,
                                               demo_file, frame_count);
                         demo_entry_get(&demo_entry, demo_file);
                    }
               }

               if(end_of_demo){
                    if(test){
                         TileMap_t check_tilemap = {};
                         ObjectArray_t<Block_t> check_block_array = {};
                         ObjectArray_t<Interactive_t> check_interactive_array = {};
                         Coord_t check_player_start;
                         Pixel_t check_player_pixel;
                         if(!load_map_from_file(demo_file, &check_player_start, &check_tilemap, &check_block_array, &check_interactive_array, demo_filepath)){
                              LOG("failed to load map state from end of file\n");
                              demo_mode = DEMO_MODE_NONE;
                         }else{
                              bool test_passed = true;

                              fread(&check_player_pixel, sizeof(check_player_pixel), 1, demo_file);
                              if(check_tilemap.width != tilemap.width){
                                   LOG_MISMATCH("tilemap width", "%d", check_tilemap.width, tilemap.width);
                              }else if(check_tilemap.height != tilemap.height){
                                   LOG_MISMATCH("tilemap height", "%d", check_tilemap.height, tilemap.height);
                              }else{
                                   for(S16 j = 0; j < check_tilemap.height; j++){
                                        for(S16 i = 0; i < check_tilemap.width; i++){
                                             if(check_tilemap.tiles[j][i].flags != tilemap.tiles[j][i].flags){
                                                  char name[64];
                                                  snprintf(name, 64, "tile %d, %d flags", i, j);
                                                  LOG_MISMATCH(name, "%d", check_tilemap.tiles[j][i].flags, tilemap.tiles[j][i].flags);
                                             }
                                        }
                                   }
                              }

                              if(check_player_pixel.x != player.pos.pixel.x){
                                   LOG_MISMATCH("player pixel x", "%d", check_player_pixel.x, player.pos.pixel.x);
                              }

                              if(check_player_pixel.y != player.pos.pixel.y){
                                   LOG_MISMATCH("player pixel y", "%d", check_player_pixel.y, player.pos.pixel.y);
                              }

                              if(check_block_array.count != block_array.count){
                                   LOG_MISMATCH("block count", "%d", check_block_array.count, block_array.count);
                              }else{
                                   for(S16 i = 0; i < check_block_array.count; i++){
                                        // TODO: consider checking other things
                                        Block_t* check_block = check_block_array.elements + i;
                                        Block_t* block = block_array.elements + i;
                                        if(check_block->pos.pixel.x != block->pos.pixel.x){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos x", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.pixel.x, block->pos.pixel.x);
                                        }

                                        if(check_block->pos.pixel.y != block->pos.pixel.y){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos y", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.pixel.y, block->pos.pixel.y);
                                        }

                                        if(check_block->pos.z != block->pos.z){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos z", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.z, block->pos.z);
                                        }

                                        if(check_block->element != block->element){
                                             char name[64];
                                             snprintf(name, 64, "block %d element", i);
                                             LOG_MISMATCH(name, "%d", check_block->element, block->element);
                                        }
                                   }
                              }

                              if(check_interactive_array.count != interactive_array.count){
                                   LOG_MISMATCH("interactive count", "%d", check_interactive_array.count, interactive_array.count);
                              }else{
                                   for(S16 i = 0; i < check_interactive_array.count; i++){
                                        // TODO: consider checking other things
                                        Interactive_t* check_interactive = check_interactive_array.elements + i;
                                        Interactive_t* interactive = interactive_array.elements + i;

                                        if(check_interactive->type != interactive->type){
                                             LOG_MISMATCH("interactive type", "%d", check_interactive->type, interactive->type);
                                        }else{
                                             switch(check_interactive->type){
                                             default:
                                                  break;
                                             case INTERACTIVE_TYPE_PRESSURE_PLATE:
                                                  if(check_interactive->pressure_plate.down != interactive->pressure_plate.down){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d pressure plate down",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->pressure_plate.down,
                                                                    interactive->pressure_plate.down);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_ICE_DETECTOR:
                                             case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                                                  if(check_interactive->detector.on != interactive->detector.on){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d detector on",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->detector.on,
                                                                    interactive->detector.on);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_POPUP:
                                                  if(check_interactive->popup.iced != interactive->popup.iced){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d popup iced",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->popup.iced,
                                                                    interactive->popup.iced);
                                                  }
                                                  if(check_interactive->popup.lift.up != interactive->popup.lift.up){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d popup lift up",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->popup.lift.up,
                                                                    interactive->popup.lift.up);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_DOOR:
                                                  if(check_interactive->door.lift.up != interactive->door.lift.up){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d door lift up",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->door.lift.up,
                                                                    interactive->door.lift.up);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_PORTAL:
                                                  if(check_interactive->portal.on != interactive->portal.on){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d portal on",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->portal.on,
                                                                    interactive->portal.on);
                                                  }
                                                  break;
                                             }
                                        }
                                   }
                              }

                              if(!test_passed){
                                   LOG("test failed\n");
                                   demo_mode = DEMO_MODE_NONE;
                                   if(suite && !show_suite) return 1;
                              }else if(suite){
                                   map_number++;
                                   S16 maps_tested = map_number - first_map_number;
                                   if(map_count > 0 && maps_tested >= map_count){
                                        LOG("Done Testing %d maps.\n", map_count);
                                        return 0;
                                   }
                                   if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                                        reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                                        destroy(&undo);
                                        init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);
                                        undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);

                                        // reset some vars
                                        player_action = {};
                                        last_block_pushed = nullptr;
                                        last_block_pushed_direction = DIRECTION_LEFT;
                                        block_to_push = nullptr;

                                        fclose(demo_file);
                                        demo_file = load_demo_number(map_number, &demo_filepath);
                                        if(demo_file){
                                             LOG("testing demo %s\n", demo_filepath);
                                             demo_entry_get(&demo_entry, demo_file);
                                             frame_count = 0;
                                             continue; // reset to the top of the loop
                                        }else{
                                             LOG("missing map %d corresponding demo.\n", map_number);
                                             return 1;
                                        }
                                   }else{
                                        LOG("Done Testing %d maps.\n", maps_tested);
                                        return 0;
                                   }
                              }else{
                                   LOG("test passed\n");
                              }
                         }
                    }else{
                         demo_mode = DEMO_MODE_NONE;
                         LOG("end of demo %s\n", demo_filepath);
                    }
               }
          }

          SDL_Event sdl_event;
          while(SDL_PollEvent(&sdl_event)){
               switch(sdl_event.type){
               default:
                    break;
               case SDL_KEYDOWN:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x--;
                              editor.selection_end.x--;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x++;
                              editor.selection_end.x++;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_UP:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y++;
                              editor.selection_end.y++;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y--;
                              editor.selection_end.y--;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_SPACE:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_SHOOT_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_L:
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                              destroy(&undo);
                              init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);
                              undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);
                              quad_tree_free(block_quad_tree);
                              block_quad_tree = quad_tree_build(&block_array);
                         }
                         break;
                    case SDL_SCANCODE_LEFTBRACKET:
                         map_number--;
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                              destroy(&undo);
                              init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);
                              undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);
                              quad_tree_free(block_quad_tree);
                              block_quad_tree = quad_tree_build(&block_array);
                         }else{
                              map_number++;
                         }
                         break;
                    case SDL_SCANCODE_RIGHTBRACKET:
                         map_number++;
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              // TODO: compress with code above
                              reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                              destroy(&undo);
                              init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);
                              undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);
                              quad_tree_free(block_quad_tree);
                              block_quad_tree = quad_tree_build(&block_array);
                         }else{
                              map_number--;
                         }
                         break;
                    case SDL_SCANCODE_V:
                    {
                         char filepath[64];
                         snprintf(filepath, 64, "content/%03d.bm", map_number);
                         save_map(filepath, player_start, &tilemap, &block_array, &interactive_array);
                    } break;
                    case SDL_SCANCODE_U:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_UNDO, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_G:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_GRAB_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_Z:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_RESET_ROOM, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_N:
                    {
                         Tile_t* tile = tilemap_get_tile(&tilemap, mouse_select_world(mouse_screen, camera));
                         if(tile){
                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_LEFT_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_LEFT_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_MID_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_MID_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_RIGHT_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_RIGHT_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_LEFT ||
                                 tile->flags & TILE_FLAG_WIRE_UP ||
                                 tile->flags & TILE_FLAG_WIRE_RIGHT ||
                                 tile->flags & TILE_FLAG_WIRE_DOWN){
                                   if(tile->flags & TILE_FLAG_WIRE_STATE){
                                        tile->flags &= ~TILE_FLAG_WIRE_STATE;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_STATE;
                                   }
                              }
                         }
                    } break;
                    // TODO: #ifdef DEBUG
                    case SDL_SCANCODE_GRAVE:
                         if(editor.mode == EDITOR_MODE_OFF){
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }else{
                              editor.mode = EDITOR_MODE_OFF;
                              editor.selection_start = {};
                              editor.selection_end = {};
                         }
                         break;
                    case SDL_SCANCODE_TAB:
                         if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                              editor.mode = EDITOR_MODE_STAMP_HIDE;
                         }else{
                              editor.mode = EDITOR_MODE_STAMP_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_RETURN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);

                              // clear coords below stamp
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }

                              for(int i = 0; i < editor.selection.count; i++){
                                   Coord_t coord = editor.selection_start + editor.selection.elements[i].offset;
                                   apply_stamp(editor.selection.elements + i, coord,
                                               &tilemap, &block_array, &interactive_array, &interactive_quad_tree, ctrl_down);
                              }

                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_T:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              // TODO: compress selection sort
                              if(editor.selection_start.x > editor.selection_end.x) SWAP(editor.selection_start.x, editor.selection_end.x);
                              if(editor.selection_start.y > editor.selection_end.y) SWAP(editor.selection_start.y, editor.selection_end.y);

                              S16 height_offset = (editor.selection_end.y - editor.selection_start.y) - 1;

                              // perform rotation on each offset
                              for(S16 i = 0; i < editor.selection.count; i++){
                                   auto* stamp = editor.selection.elements + i;
                                   Coord_t rot {stamp->offset.y, (S16)(-stamp->offset.x + height_offset)};
                                   stamp->offset = rot;
                              }
                         }
                         break;
                    case SDL_SCANCODE_X:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              destroy(&editor.clipboard);
                              shallow_copy(&editor.selection, &editor.clipboard);
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_P:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT && editor.clipboard.count){
                              destroy(&editor.selection);
                              shallow_copy(&editor.clipboard, &editor.selection);
                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         }
                         break;
                    case SDL_SCANCODE_M:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              player_start = mouse_select_world(mouse_screen, camera);
                         }
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = true;
                         break;
                    case SDL_SCANCODE_B:
#if 0
                    {
                         undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                         Coord_t min = pos_to_coord(player.pos) - Coord_t{1, 1};
                         Coord_t max = pos_to_coord(player.pos) + Coord_t{1, 1};
                         for(S16 y = min.y; y <= max.y; y++){
                              for(S16 x = min.x; x <= max.x; x++){
                                   Coord_t coord {x, y};
                                   coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                              }
                         }
                    } break;
#endif
                    case SDL_SCANCODE_5:
                         player.pos.pixel = mouse_select_world_pixel(mouse_screen, camera) + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
                         player.pos.decimal.x = 0;
                         player.pos.decimal.y = 0;
                         break;
                    case SDL_SCANCODE_H:
                    {
                         Coord_t coord = mouse_select_world(mouse_screen, camera);
                         describe_coord(coord, &tilemap, interactive_quad_tree, block_quad_tree);
                    } break;
                    case SDL_SCANCODE_9:
                         g_debug_light_iteration--;
                         if(g_debug_light_iteration < 0) g_debug_light_iteration = 0;
                         break;
                    case SDL_SCANCODE_0:
                         g_debug_light_iteration++;
                         break;
                    }
                    break;
               case SDL_KEYUP:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_SPACE:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_SHOOT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = false;
                         break;
                    case SDL_SCANCODE_G:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_GRAB_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONDOWN:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = true;

                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_OFF:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              Coord_t mouse_coord = vec_to_coord(mouse_screen);
                              S32 select_index = (mouse_coord.y * ROOM_TILE_SIZE) + mouse_coord.x;
                              if(select_index < EDITOR_CATEGORY_COUNT){
                                   editor.mode = EDITOR_MODE_STAMP_SELECT;
                                   editor.category = select_index;
                                   editor.stamp = 0;
                              }else{
                                   editor.mode = EDITOR_MODE_CREATE_SELECTION;
                                   editor.selection_start = mouse_select_world(mouse_screen, camera);
                                   editor.selection_end = editor.selection_start;
                              }
                         } break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              S32 select_index = mouse_select_stamp_index(vec_to_coord(mouse_screen),
                                                                          editor.category_array.elements + editor.category);
                              if(editor.mode != EDITOR_MODE_STAMP_HIDE && select_index < editor.category_array.elements[editor.category].count && select_index >= 0){
                                   editor.stamp = select_index;
                              }else{
                                   undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                                   Coord_t select_coord = mouse_select_world(mouse_screen, camera);
                                   auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
                                   for(S16 s = 0; s < stamp_array->count; s++){
                                        auto* stamp = stamp_array->elements + s;
                                        apply_stamp(stamp, select_coord + stamp->offset,
                                                    &tilemap, &block_array, &interactive_array, &interactive_quad_tree, ctrl_down);
                                   }

                                   quad_tree_free(block_quad_tree);
                                   block_quad_tree = quad_tree_build(&block_array);
                              }
                         } break;
                         }
                         break;
                    case SDL_BUTTON_RIGHT:
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              coord_clear(mouse_select_world(mouse_screen, camera), &tilemap, &interactive_array,
                                          interactive_quad_tree, &block_array);
                              break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              Coord_t start = mouse_select_world(mouse_screen, camera);
                              Coord_t end = start + stamp_array_dimensions(editor.category_array.elements[editor.category].elements + editor.stamp);
                              for(S16 j = start.y; j < end.y; j++){
                                   for(S16 i = start.x; i < end.x; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }
                         } break;
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONUP:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = false;
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CREATE_SELECTION:
                         {
                              editor.selection_end = mouse_select_world(mouse_screen, camera);

                              // sort selection range
                              if(editor.selection_start.x > editor.selection_end.x) SWAP(editor.selection_start.x, editor.selection_end.x);
                              if(editor.selection_start.y > editor.selection_end.y) SWAP(editor.selection_start.y, editor.selection_end.y);

                              destroy(&editor.selection);

                              S16 stamp_count = (((editor.selection_end.x - editor.selection_start.x) + 1) * ((editor.selection_end.y - editor.selection_start.y) + 1)) * 2;
                              init(&editor.selection, stamp_count);
                              S16 stamp_index = 0;
                              for(S16 j = editor.selection_start.y; j <= editor.selection_end.y; j++){
                                   for(S16 i = editor.selection_start.x; i <= editor.selection_end.x; i++){
                                        Coord_t coord = {i, j};
                                        Coord_t offset = coord - editor.selection_start;

                                        // tile id
                                        Tile_t* tile = tilemap_get_tile(&tilemap, coord);
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_ID;
                                        editor.selection.elements[stamp_index].tile_id = tile->id;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // tile flags
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_FLAGS;
                                        editor.selection.elements[stamp_index].tile_flags = tile->flags;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // interactive
                                        auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
                                        if(interactive){
                                             resize(&editor.selection, editor.selection.count + 1);
                                             auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                             stamp->type = STAMP_TYPE_INTERACTIVE;
                                             stamp->interactive = *interactive;
                                             stamp->offset = offset;
                                        }

                                        for(S16 b = 0; b < block_array.count; b++){
                                             auto* block = block_array.elements + b;
                                             if(pos_to_coord(block->pos) == coord){
                                                  resize(&editor.selection, editor.selection.count + 1);
                                                  auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                                  stamp->type = STAMP_TYPE_BLOCK;
                                                  stamp->block.face = block->face;
                                                  stamp->block.element = block->element;
                                                  stamp->offset = offset;
                                             }
                                        }
                                   }
                              }

                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEMOTION:
                    mouse_screen = Vec_t{((F32)(sdl_event.button.x) / (F32)(window_width)),
                                         1.0f - ((F32)(sdl_event.button.y) / (F32)(window_height))};
                    mouse_world = vec_to_pos(mouse_screen);
                    switch(editor.mode){
                    default:
                         break;
                    case EDITOR_MODE_CREATE_SELECTION:
                         if(editor.selection_start.x >= 0 && editor.selection_start.y >= 0){
                              editor.selection_end = pos_to_coord(mouse_world);
                         }
                         break;
                    }
                    break;
               }
          }

          if(quit) break;

          // reset base light
          S32 light_width = tilemap.width * 2;
          S32 light_height = tilemap.height * 2;
          for(S16 j = 0; j < light_height; j++){
               for(S16 i = 0; i < light_width; i++){
                    tilemap.light[j][i] = BASE_LIGHT;
               }
          }

          // update interactives
          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               if(interactive->type == INTERACTIVE_TYPE_POPUP){
                    lift_update(&interactive->popup.lift, POPUP_TICK_DELAY, dt, 1, HEIGHT_INTERVAL + 1);
               }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                    lift_update(&interactive->door.lift, POPUP_TICK_DELAY, dt, 0, DOOR_MAX_HEIGHT);
               }else if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                    bool should_be_down = false;
                    Coord_t player_coord = pos_to_coord(player.pos);
                    if(interactive->coord == player_coord){
                         should_be_down = true;
                    }else{
                         Tile_t* tile = tilemap_get_tile(&tilemap, interactive->coord);
                         if(tile){
                              if(!tile_is_iced(tile)){
                                   Pixel_t center = coord_to_pixel(interactive->coord) + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
                                   Rect_t rect = {};
                                   rect.left = center.x - (2 * TILE_SIZE_IN_PIXELS);
                                   rect.right = center.x + (2 * TILE_SIZE_IN_PIXELS);
                                   rect.bottom = center.y - (2 * TILE_SIZE_IN_PIXELS);
                                   rect.top = center.y + (2 * TILE_SIZE_IN_PIXELS);

                                   S16 block_count = 0;
                                   Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                                   quad_tree_find_in(block_quad_tree, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                   Coord_t block_coords[BLOCK_MAX_TOUCHING_COORDS];

                                   for(S16 b = 0; b < block_count; b++){
                                        block_touching_coords(blocks[b], block_coords);

                                        for(int c = 0; c < BLOCK_MAX_TOUCHING_COORDS; c++){
                                             if(interactive->coord == block_coords[c]){
                                                  should_be_down = true;
                                                  break;
                                             }
                                        }
                                   }
                              }
                         }
                    }

                    if(should_be_down != interactive->pressure_plate.down){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->pressure_plate.down = should_be_down;
                    }
               }
          }

          Rect_t player_rect = {};
          player_rect.left = player.pos.pixel.x - 3;
          player_rect.right = player_rect.left + 6;
          player_rect.bottom = player.pos.pixel.y - 3;
          player_rect.top = player_rect.bottom + 6;

          // update arrows
          for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
               Arrow_t* arrow = arrow_array.arrows + i;
               if(!arrow->alive) continue;

               Coord_t pre_move_coord = pixel_to_coord(arrow->pos.pixel);

               if(arrow->element == ELEMENT_FIRE){
                    illuminate(pixel_to_half(arrow->pos.pixel), 255 - LIGHT_DECAY, &tilemap, block_quad_tree);
               }

               if(arrow->stick_time > 0.0f){
                    arrow->stick_time += dt;
                    if(arrow->stick_time > ARROW_DISINTEGRATE_DELAY){
                         arrow->alive = false;
                    }

                    switch(arrow->stick_type){
                    default:
                         break;
                    case STICK_TYPE_BLOCK:
                         arrow->pos = arrow->stick_to_block->pos + arrow->stick_offset;
                         break;
                    case STICK_TYPE_POPUP:
                         arrow->pos.z = arrow->stick_to_popup->popup.lift.ticks + arrow->stick_offset.z;
                         break;
                    }
                    continue;
               }

               F32 arrow_friction = 0.9999f;

               if(arrow->pos.z > 0){
                    // TODO: fall based on the timer !
                    arrow->fall_time += dt;
                    if(arrow->fall_time > ARROW_FALL_DELAY){
                         arrow->fall_time -= ARROW_FALL_DELAY;
                         arrow->pos.z--;
                    }
               }else{
                    arrow_friction = 0.9f;
               }

               Vec_t direction = {};
               switch(arrow->face){
               default:
                    break;
               case DIRECTION_LEFT:
                    direction.x = -1;
                    break;
               case DIRECTION_RIGHT:
                    direction.x = 1;
                    break;
               case DIRECTION_DOWN:
                    direction.y = -1;
                    break;
               case DIRECTION_UP:
                    direction.y = 1;
                    break;
               }

               arrow->pos += (direction * dt * arrow->vel);
               arrow->vel *= arrow_friction;

               if(arrow->pos.z >= player.pos.z &&
                  arrow->pos.z < player.pos.z + (HEIGHT_INTERVAL * 2) &&
                  pixel_in_rect(arrow->pos.pixel, player_rect)){
                    arrow->stick_time += dt;
                    arrow->stick_offset = arrow->pos - player.pos;
                    arrow->stick_type = STICK_TYPE_PLAYER;
                    arrow->stick_to_player = &player;
                    resetting = dt;
                    continue;
               }

               Coord_t post_move_coord = pixel_to_coord(arrow->pos.pixel);

               Rect_t coord_rect {(S16)(arrow->pos.pixel.x - TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.y - TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.x + TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.y + TILE_SIZE_IN_PIXELS)};

               S16 block_count = 0;
               Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
               quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
               for(S16 b = 0; b < block_count; b++){
                    // blocks on the coordinate and on the ground block light
                    Rect_t block_rect = block_get_rect(blocks[b]);
                    S16 block_index = blocks[b] - block_array.elements;
                    if(pixel_in_rect(arrow->pos.pixel, block_rect) && arrow->element_from_block != block_index){
                         arrow->element_from_block = block_index;
                         if(arrow->element != blocks[b]->element){
                              Element_t arrow_element = arrow->element;
                              arrow->element = transition_element(arrow->element, blocks[b]->element);
                              if(arrow_element){
                                   blocks[b]->element = transition_element(blocks[b]->element, arrow_element);
                              }
                         }
                         break;
                    }
               }

               if(block_count == 0){
                    arrow->element_from_block = -1;
               }

               Coord_t skip_coord[DIRECTION_COUNT];
               find_portal_adjacents_to_skip_collision_check(pre_move_coord, interactive_quad_tree, skip_coord);

               if(pre_move_coord != post_move_coord){
                    bool skip = false;
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         if(post_move_coord == skip_coord[d]){
                              skip = true;
                         }
                    }

                    if(!skip){
                         Tile_t* tile = tilemap_get_tile(&tilemap, post_move_coord);
                         if(tile_is_solid(tile)){
                              arrow->stick_time = dt;
                         }else{
                              Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, post_move_coord.x, post_move_coord.y);
                              if(interactive){
                                   switch(interactive->type){
                                   default:
                                        break;
                                   case INTERACTIVE_TYPE_PORTAL:
                                        if(!interactive->portal.on) arrow->stick_time = dt;
                                        break;
                                   case INTERACTIVE_TYPE_POPUP:
                                   {
                                        S8 popup_height = interactive->popup.lift.ticks - 1;
                                        if(popup_height > arrow->pos.z){
                                             arrow->stick_time = dt;
                                             arrow->stick_offset = arrow->pos - coord_to_pos_at_tile_center(post_move_coord);
                                             arrow->stick_type = STICK_TYPE_POPUP;
                                             arrow->stick_to_popup = interactive;
                                        }
                                   }
                                        break;
                                   }
                              }
                         }

                         if(arrow->stick_time < dt){
                              Rect_t check_rect = rect_surrounding_coord(post_move_coord);
                              S16 stick_block_count = 0;
                              Block_t* stick_blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(block_quad_tree, check_rect, stick_blocks, &stick_block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                              for(S8 b = 0; b < stick_block_count; b++){
                                   Rect_t block_rect = block_get_rect(stick_blocks[b]);
                                   if(pixel_in_rect(arrow->pos.pixel, block_rect) &&
                                      arrow->pos.z >= stick_blocks[b]->pos.z &&
                                      arrow->pos.z < (stick_blocks[b]->pos.z + HEIGHT_INTERVAL)){
                                        arrow->stick_time = dt;
                                        arrow->stick_offset = arrow->pos - stick_blocks[b]->pos;
                                        arrow->stick_type = STICK_TYPE_BLOCK;
                                        arrow->stick_to_block = stick_blocks[b];
                                        break;
                                   }
                              }
                         }
                    }

                    // catch or give elements
                    if(arrow->element == ELEMENT_FIRE || arrow->element == ELEMENT_ICE){
                         spread_element(pixel_to_half(arrow->pos.pixel), 0, &tilemap, interactive_quad_tree, block_quad_tree,
                                        arrow->element, false);
                    }

                    Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, post_move_coord);
                    if(interactive){
                         if(interactive->type == INTERACTIVE_TYPE_LEVER){
                              if(arrow->pos.z >= HEIGHT_INTERVAL){
                                   activate(&tilemap, interactive_quad_tree, post_move_coord);
                              }else{
                                   arrow->stick_time = dt;
                              }
                         }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                              if(interactive->door.lift.ticks < arrow->pos.z){
                                   arrow->stick_time = dt;
                              }
                         }
                    }

                    S8 rotations_between = teleport_position_across_portal(&arrow->pos, NULL, interactive_quad_tree, &tilemap, pre_move_coord,
                                                                           post_move_coord);
                    if(rotations_between >= 0){
                         arrow->face = direction_rotate_clockwise(arrow->face, rotations_between);
                    }
               }
          }

          if(resetting > 0){
               if(has_reset){
                    resetting -= dt;
                    if(resetting <= 0){
                         resetting = 0;
                         has_reset = false;
                         player_action.reset_room = false;
                    }
               }else{
                    if(resetting >= RESET_TIME){
                         has_reset = true;
                         if(load_map_filepath){
                              if(!load_map(load_map_filepath, &player_start, &tilemap, &block_array, &interactive_array)){
                                   return 1;
                              }

                              reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                         }else if(map_number >= 0){
                              if(!load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                                   return 1;
                              }

                              reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                         }
                    }else{
                         resetting += dt;
                    }
               }
          }

          // update player
          if(resetting == 0 || has_reset){
               user_movement = vec_zero();

               if(player_action.move_left){
                    Direction_t direction = DIRECTION_LEFT;
                    direction = direction_rotate_clockwise(direction, player_action.move_left_rotation);
                    user_movement += direction_to_vec(direction);
                    if(player_action.reface) player.face = direction;
               }

               if(player_action.move_right){
                    Direction_t direction = DIRECTION_RIGHT;
                    direction = direction_rotate_clockwise(direction, player_action.move_right_rotation);
                    user_movement += direction_to_vec(direction);
                    if(player_action.reface) player.face = direction;
               }

               if(player_action.move_up){
                    Direction_t direction = DIRECTION_UP;
                    direction = direction_rotate_clockwise(direction, player_action.move_up_rotation);
                    user_movement += direction_to_vec(direction);
                    if(player_action.reface) player.face = direction;
               }

               if(player_action.move_down){
                    Direction_t direction = DIRECTION_DOWN;
                    direction = direction_rotate_clockwise(direction, player_action.move_down_rotation);
                    user_movement += direction_to_vec(direction);
                    if(player_action.reface) player.face = direction;
               }

               if(player_action.activate && !player_action.last_activate){
                    undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                    activate(&tilemap, interactive_quad_tree, pos_to_coord(player.pos) + player.face);
               }

               if(player_action.undo){
                    undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                    undo_revert(&undo, &player, &tilemap, &block_array, &interactive_array);
                    quad_tree_free(interactive_quad_tree);
                    interactive_quad_tree = quad_tree_build(&interactive_array);
                    quad_tree_free(block_quad_tree);
                    block_quad_tree = quad_tree_build(&block_array);
                    player_action.undo = false;
               }
          }

          if(player_action.reset_room){
               player_action.reset_room = false;
               resetting = dt;
          }

          if(player.has_bow && player_action.shoot && player.bow_draw_time < PLAYER_BOW_DRAW_DELAY){
               player.bow_draw_time += dt;
          }else if(!player_action.shoot){
               if(player.bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                    undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);

                    Position_t arrow_pos = player.pos;
                    switch(player.face){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         arrow_pos.pixel.y -= 2;
                         arrow_pos.pixel.x -= 8;
                         break;
                    case DIRECTION_RIGHT:
                         arrow_pos.pixel.y -= 2;
                         arrow_pos.pixel.x += 8;
                         break;
                    case DIRECTION_UP:
                         arrow_pos.pixel.y += 7;
                         break;
                    case DIRECTION_DOWN:
                         arrow_pos.pixel.y -= 11;
                         break;
                    }

                    arrow_pos.z += ARROW_SHOOT_HEIGHT;
                    arrow_spawn(&arrow_array, arrow_pos, player.face);
               }
               player.bow_draw_time = 0.0f;
          }

          if(!player_action.move_left && !player_action.move_right && !player_action.move_up && !player_action.move_down){
               player.walk_frame = 1;
          }else{
               player.walk_frame_time += dt;

               if(player.walk_frame_time > PLAYER_WALK_DELAY){
                    if(vec_magnitude(player.vel) > PLAYER_IDLE_SPEED){
                         player.walk_frame_time = 0.0f;

                         player.walk_frame += player.walk_frame_delta;
                         if(player.walk_frame > 2 || player.walk_frame < 0){
                              player.walk_frame = 1;
                              player.walk_frame_delta = -player.walk_frame_delta;
                         }
                    }else{
                         player.walk_frame = 1;
                         player.walk_frame_time = 0.0f;
                    }
               }
          }

          Vec_t pos_vec = pos_to_vec(player.pos);

#if 0
          // TODO: do we want this in the future?
          // figure out what room we should focus on
          Position_t room_center {};
          for(U32 i = 0; i < ELEM_COUNT(rooms); i++){
               if(coord_in_rect(vec_to_coord(pos_vec), rooms[i])){
                    S16 half_room_width = (rooms[i].right - rooms[i].left) / 2;
                    S16 half_room_height = (rooms[i].top - rooms[i].bottom) / 2;
                    Coord_t room_center_coord {(S16)(rooms[i].left + half_room_width),
                                               (S16)(rooms[i].bottom + half_room_height)};
                    room_center = coord_to_pos(room_center_coord);
                    break;
               }
          }
#else
          Position_t room_center = coord_to_pos(Coord_t{8, 8});
#endif

          Position_t camera_movement = room_center - camera;
          camera += camera_movement * 0.05f;

          float drag = 0.625f;

          // block movement
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;

               //Coord_t block_prev_coord = pos_to_coord(block->pos);

               // TODO: compress with player movement
               Vec_t pos_delta = (block->accel * dt * dt * 0.5f) + (block->vel * dt);
               block->vel += block->accel * dt;
               block->vel *= drag;

               // TODO: blocks with velocity need to be checked against other blocks

               Position_t pre_move = block->pos;
               block->pos += pos_delta;

               bool stop_on_boundary_x = false;
               bool stop_on_boundary_y = false;
               bool held_up = false;

               if(pos_delta.x != 0.0f || pos_delta.y != 0.0f){
                    check_block_collision_with_other_blocks(block, block_quad_tree, interactive_quad_tree, &tilemap,
                                                            &player, last_block_pushed, last_block_pushed_direction);
               }

               // get the current coord of the center of the block
               Pixel_t center = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               Coord_t coord = pixel_to_coord(center);

               Coord_t skip_coord[DIRECTION_COUNT];
               find_portal_adjacents_to_skip_collision_check(coord, interactive_quad_tree, skip_coord);

               // check for adjacent walls
               if(block->vel.x > 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_RIGHT, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_x = true;
                    }else if(coord_b != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }else if(block->vel.x < 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_LEFT, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_x = true;
                    }else if(coord_b != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }

               if(block->vel.y > 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_UP, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_UP] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_y = true;
                    }else if(coord_b != skip_coord[DIRECTION_UP] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }else if(block->vel.y < 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_DOWN, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_y = true;
                    }else if(coord_b != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }

               if(block != last_block_pushed && !block_on_ice(block, &tilemap, interactive_quad_tree)){
                    stop_on_boundary_x = true;
                    stop_on_boundary_y = true;
               }

               if(stop_on_boundary_x){
                    // stop on tile boundaries separately for each axis
                    S16 boundary_x = range_passes_tile_boundary(pre_move.pixel.x, block->pos.pixel.x, block->push_start.x);
                    if(boundary_x){
                         block->pos.pixel.x = boundary_x;
                         block->pos.decimal.x = 0.0f;
                         block->vel.x = 0.0f;
                         block->accel.x = 0.0f;
                    }
               }

               if(stop_on_boundary_y){
                    S16 boundary_y = range_passes_tile_boundary(pre_move.pixel.y, block->pos.pixel.y, block->push_start.y);
                    if(boundary_y){
                         block->pos.pixel.y = boundary_y;
                         block->pos.decimal.y = 0.0f;
                         block->vel.y = 0.0f;
                         block->accel.y = 0.0f;
                    }
               }

               held_up = block_held_up_by_another_block(block, block_quad_tree);

               // TODO: should we care about the decimal component of the position ?
               Coord_t block_coords[BLOCK_MAX_TOUCHING_COORDS];
               block_touching_coords(block, block_coords);

               for(int c = 0; c < BLOCK_MAX_TOUCHING_COORDS; c++){
                    auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, block_coords[c]);
                    if(interactive){
                         if(interactive->type == INTERACTIVE_TYPE_POPUP){
                              if(block->pos.z == interactive->popup.lift.ticks - 2){
                                   block->pos.z++;
                                   held_up = true;
                              }else if(block->pos.z == (interactive->popup.lift.ticks - 1)){
                                   held_up = true;
                              }
                         }
                    }
               }

               if(!held_up && block->pos.z > 0){
                    block->fall_time += dt;
                    if(block->fall_time >= FALL_TIME){
                         block->fall_time -= FALL_TIME;
                         block->pos.z--;
                    }
               }

               coord = pixel_to_coord(block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
               Coord_t premove_coord = pixel_to_coord(pre_move.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});

               Position_t block_center = block->pos;
               block_center.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               S8 rotations_between = teleport_position_across_portal(&block_center, NULL, interactive_quad_tree, &tilemap, premove_coord,
                                                                      coord);
               if(rotations_between >= 0){
                    block->pos = block_center;
                    block->pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

                    block->vel = vec_rotate_quadrants(block->vel, rotations_between);
                    block->accel = vec_rotate_quadrants(block->accel, rotations_between);

                    check_block_collision_with_other_blocks(block, block_quad_tree, interactive_quad_tree, &tilemap,
                                                            &player, last_block_pushed, last_block_pushed_direction);

                    // try teleporting if we collided with a block
                    premove_coord = pixel_to_coord(block_center.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
                    coord = pixel_to_coord(block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});

                    block_center = block_get_center(block);

                    rotations_between = teleport_position_across_portal(&block_center, NULL, interactive_quad_tree, &tilemap, premove_coord,
                                                                        coord);
                    if(rotations_between >= 0){
                         block->pos = block_center;
                         block->pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

                         block->vel = vec_rotate_quadrants(block->vel, rotations_between);
                         block->accel = vec_rotate_quadrants(block->accel, rotations_between);
                    }
               }
          }

          // illuminate and ice
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;
               if(block->element == ELEMENT_FIRE){
                    illuminate(pixel_to_half(block->pos.pixel), 255, &tilemap, block_quad_tree);
               }else if(block->element == ELEMENT_ICE){
                    spread_element(pixel_to_half(block->pos.pixel), 2, &tilemap, interactive_quad_tree, block_quad_tree,
                                   ELEMENT_ICE, false);
               }
          }

          // melt ice
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;
               if(block->element == ELEMENT_FIRE){
                    spread_element(pixel_to_half(block->pos.pixel), 2, &tilemap, interactive_quad_tree, block_quad_tree,
                                   ELEMENT_FIRE, false);
               }
          }

          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               if(interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR){
                    Half_t half = coord_to_half(interactive->coord);
                    U8* light_top_left = tilemap_get_light(&tilemap, half);
                    U8* light_top_right = tilemap_get_light(&tilemap, half);
                    U8* light_bottom_left = tilemap_get_light(&tilemap, half);
                    U8* light_bottom_right = tilemap_get_light(&tilemap, half);
                    Rect_t coord_rect = rect_surrounding_adjacent_coords(interactive->coord);

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Block_t* block = nullptr;
                    for(S16 b = 0; b < block_count; b++){
                         Coord_t block_coords[BLOCK_MAX_TOUCHING_COORDS];
                         block_touching_coords(blocks[b], block_coords);

                         // blocks on the coordinate and on the ground block light
                         for(int c = 0; c < BLOCK_MAX_TOUCHING_COORDS; c++){
                              if(block_coords[c] == interactive->coord && blocks[b]->pos.z == 0){
                                   block = blocks[b];
                                   break;
                              }
                         }
                    }

                    bool light_above_threshold = false;

                    bool light_below_threshold = *light_top_left < LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_top_right < LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_bottom_left < LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_bottom_right < LIGHT_DETECTOR_THRESHOLD;

                    if(!light_below_threshold){
                         light_above_threshold = *light_top_left >= LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_top_right >= LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_bottom_left >= LIGHT_DETECTOR_THRESHOLD &&
                                                 *light_bottom_right >= LIGHT_DETECTOR_THRESHOLD;
                    }

                    if(interactive->detector.on && (light_below_threshold || block)){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->detector.on = false;
                    }else if(!interactive->detector.on && light_above_threshold && !block){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->detector.on = true;
                    }
               }else if(interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR){
                    Tile_t* tile = tilemap_get_tile(&tilemap, interactive->coord);
                    if(tile){
                         if(interactive->detector.on && !tile_is_iced(tile)){
                              activate(&tilemap, interactive_quad_tree, interactive->coord);
                              interactive->detector.on = false;
                         }else if(!interactive->detector.on && tile_is_iced(tile)){
                              activate(&tilemap, interactive_quad_tree, interactive->coord);
                              interactive->detector.on = true;
                         }
                    }
               }
          }

          Position_t portal_player_pos = {};

          // player movement
          user_movement = vec_normalize(user_movement);
          player.accel = user_movement * PLAYER_SPEED;

          Vec_t pos_delta = (player.accel * dt * dt * 0.5f) + (player.vel * dt);
          player.vel += player.accel * dt;
          player.vel *= drag;

          if(fabs(vec_magnitude(player.vel)) > PLAYER_SPEED){
               player.vel = vec_normalize(player.vel) * PLAYER_SPEED;
          }

          Coord_t skip_coord[DIRECTION_COUNT];
          Coord_t player_previous_coord = pos_to_coord(player.pos);
          Coord_t player_coord = pos_to_coord(player.pos + pos_delta);

          find_portal_adjacents_to_skip_collision_check(player_coord, interactive_quad_tree, skip_coord);
          S8 rotations_between = teleport_position_across_portal(&player.pos, &pos_delta, interactive_quad_tree, &tilemap, player_previous_coord,
                                                                 player_coord);

          player_coord = pos_to_coord(player.pos + pos_delta);

          bool collide_with_interactive = false;
          Vec_t player_delta_pos = move_player_position_through_world(player.pos, pos_delta, player.face, skip_coord,
                                                                      &player, &tilemap, interactive_quad_tree,
                                                                      &block_array, &block_to_push,
                                                                      &last_block_pushed_direction,
                                                                      &collide_with_interactive);

          player_coord = pos_to_coord(player.pos + player_delta_pos);

          if(block_to_push){
               F32 before_time = player.push_time;

               player.push_time += dt;
               if(player.push_time > BLOCK_PUSH_TIME){
                    if(before_time <= BLOCK_PUSH_TIME) undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                    block_push(block_to_push, last_block_pushed_direction, &tilemap, interactive_quad_tree, block_quad_tree, false);
                    if(block_to_push->pos.z > 0) player.push_time = -0.5f;
               }
          }else{
               player.push_time = 0;
          }

          if(rotations_between < 0){
               rotations_between = teleport_position_across_portal(&player.pos, &player_delta_pos, interactive_quad_tree, &tilemap, player_previous_coord,
                                                                   player_coord);
          }else{
               teleport_position_across_portal(&player.pos, &player_delta_pos, interactive_quad_tree, &tilemap, player_previous_coord, player_coord);
          }

          if(rotations_between >= 0){
               player.face = direction_rotate_clockwise(player.face, rotations_between);
               player.vel = vec_rotate_quadrants(player.vel, rotations_between);
               player.accel = vec_rotate_quadrants(player.accel, rotations_between);

               // set rotations for each direction the player wants to move
               if(player_action.move_left) player_action.move_left_rotation = (player_action.move_left_rotation + rotations_between) % DIRECTION_COUNT;
               if(player_action.move_right) player_action.move_right_rotation = (player_action.move_right_rotation + rotations_between) % DIRECTION_COUNT;
               if(player_action.move_up) player_action.move_up_rotation = (player_action.move_up_rotation + rotations_between) % DIRECTION_COUNT;
               if(player_action.move_down) player_action.move_down_rotation = (player_action.move_down_rotation + rotations_between) % DIRECTION_COUNT;
          }

          player.pos += player_delta_pos;

          if(suite && !show_suite) continue;

          glClear(GL_COLOR_BUFFER_BIT);

          Position_t screen_camera = camera - Vec_t{0.5f, 0.5f} + Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};

          Coord_t min = pos_to_coord(screen_camera);
          Coord_t max = min + Coord_t{ROOM_TILE_SIZE, ROOM_TILE_SIZE};
          min = coord_clamp_zero_to_dim(min, tilemap.width - 1, tilemap.height - 1);
          max = coord_clamp_zero_to_dim(max, tilemap.width - 1, tilemap.height - 1);
          Position_t tile_bottom_left = coord_to_pos(min);
          Vec_t camera_offset = pos_to_vec(tile_bottom_left - screen_camera);

          // draw flats
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    Tile_t* tile = tilemap.tiles[y] + x;
                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, x, y);
                    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                         Coord_t coord {x, y};
                         PortalExit_t portal_exits = find_portal_exits(coord, &tilemap, interactive_quad_tree);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                                   Tile_t* portal_tile = tilemap.tiles[portal_coord.y] + portal_coord.x;
                                   Interactive_t* portal_interactive = quad_tree_find_at(interactive_quad_tree, portal_coord.x, portal_coord.y);
                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                   draw_flats(tile_pos, portal_tile, portal_interactive, theme_texture, portal_rotations);
                              }
                         }
                    }else{
                         draw_flats(tile_pos, tile, interactive, theme_texture, 0);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    Coord_t coord {x, y};
                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, coord.x, coord.y);

                    if(interactive){
                         if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                              PortalExit_t portal_exits = find_portal_exits(coord, &tilemap, interactive_quad_tree);

                              for(S8 d = 0; d < DIRECTION_COUNT; d++){
                                   for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                        if(portal_exits.directions[d].coords[i] == coord) continue;
                                        Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));

                                        S16 px = portal_coord.x * TILE_SIZE_IN_PIXELS;
                                        S16 py = portal_coord.y * TILE_SIZE_IN_PIXELS;
                                        Rect_t coord_rect {(S16)(px - HALF_TILE_SIZE_IN_PIXELS), (S16)(py - HALF_TILE_SIZE_IN_PIXELS),
                                                           (S16)(px + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS),
                                                           (S16)(py + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS)};

                                        S16 block_count = 0;
                                        Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

                                        quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                        Interactive_t* portal_interactive = quad_tree_find_at(interactive_quad_tree, portal_coord.x, portal_coord.y);

                                        U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                        Player_t* player_ptr = nullptr;
                                        Pixel_t portal_center_pixel = coord_to_pixel_at_center(portal_coord);
                                        if(pixel_distance_between(portal_center_pixel, player.pos.pixel) <= 20){
                                             player_ptr = &player;
                                        }
                                        draw_solids(tile_pos, portal_interactive, blocks, block_count, player_ptr, screen_camera,
                                                    theme_texture, player_texture, portal_coord, coord, portal_rotations);

                                   }
                              }

                              interactive_draw(interactive, tile_pos);
                         }
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = tilemap.tiles[y] + x;
                    if(tile && tile->id >= 16){
                         Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                         (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                         tile_id_draw(tile->id, tile_pos);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               Player_t* player_ptr = nullptr;
               if(pos_to_coord(player.pos).y == y) player_ptr = &player;

               for(S16 x = min.x; x <= max.x; x++){
                    Coord_t coord {x, y};

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    S16 px = coord.x * TILE_SIZE_IN_PIXELS;
                    S16 py = coord.y * TILE_SIZE_IN_PIXELS;
                    Rect_t coord_rect {(S16)(px - HALF_TILE_SIZE_IN_PIXELS), (S16)(py - HALF_TILE_SIZE_IN_PIXELS),
                                       (S16)(px + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS),
                                       (S16)(py + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS)};

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, x, y);

                    draw_solids(tile_pos, interactive, blocks, block_count, player_ptr, screen_camera, theme_texture, player_texture,
                                Coord_t{-1, -1}, Coord_t{-1, -1}, 0);
               }

               // draw arrows
               static Vec_t arrow_tip_offset[DIRECTION_COUNT] = {
                    {0.0f,               9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  16.0f * PIXEL_SIZE},
                    {16.0f * PIXEL_SIZE, 9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  0.0f * PIXEL_SIZE},
               };

               for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
                    Arrow_t* arrow = arrow_array.arrows + a;
                    if(!arrow->alive) continue;
                    if((arrow->pos.pixel.y / TILE_SIZE_IN_PIXELS) != y) continue;

                    Vec_t arrow_vec = pos_to_vec(arrow->pos - screen_camera);
                    arrow_vec.x -= arrow_tip_offset[arrow->face].x;
                    arrow_vec.y -= arrow_tip_offset[arrow->face].y;

                    glEnd();
                    glBindTexture(GL_TEXTURE_2D, arrow_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);

                    // shadow
                    //arrow_vec.y -= (arrow->pos.z * PIXEL_SIZE);
                    Vec_t tex_vec = arrow_frame(arrow->face, 1);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(arrow_vec.x, arrow_vec.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y);

                    arrow_vec.y += (arrow->pos.z * PIXEL_SIZE);

                    S8 y_frame = 0;
                    if(arrow->element) y_frame = 2 + ((arrow->element - 1) * 4);

                    tex_vec = arrow_frame(arrow->face, y_frame);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(arrow_vec.x, arrow_vec.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y);

                    glEnd();

                    glBindTexture(GL_TEXTURE_2D, theme_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }
          }

          glEnd();

          // player circle
          Position_t player_camera_offset = player.pos - screen_camera;
          pos_vec = pos_to_vec(player_camera_offset);

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_LINES);
          if(block_to_push){
               glColor3f(1.0f, 0.0f, 0.0f);
          }else{
               glColor3f(1.0f, 1.0f, 1.0f);
          }
          Vec_t prev_vec {pos_vec.x + PLAYER_RADIUS, pos_vec.y};
          S32 segments = 32;
          F32 delta = 3.14159f * 2.0f / (F32)(segments);
          F32 angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * PLAYER_RADIUS;
               F32 dy = sin(angle) * PLAYER_RADIUS;

               glVertex2f(prev_vec.x, prev_vec.y);
               glVertex2f(pos_vec.x + dx, pos_vec.y + dy);
               prev_vec.x = pos_vec.x + dx;
               prev_vec.y = pos_vec.y + dy;
               angle += delta;
          }

          pos_vec = pos_to_vec(portal_player_pos - screen_camera);
          prev_vec = {pos_vec.x + PLAYER_RADIUS, pos_vec.y};
          segments = 32;
          delta = 3.14159f * 2.0f / (F32)(segments);
          angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * PLAYER_RADIUS;
               F32 dy = sin(angle) * PLAYER_RADIUS;

               glVertex2f(prev_vec.x, prev_vec.y);
               glVertex2f(pos_vec.x + dx, pos_vec.y + dy);
               prev_vec.x = pos_vec.x + dx;
               prev_vec.y = pos_vec.y + dy;
               angle += delta;
          }

          glEnd();

// debug block outline
#if 0
          Vec_t collided_with_center = {(float)(g_collided_with_pixel.x) * PIXEL_SIZE, (float)(g_collided_with_pixel.y) * PIXEL_SIZE};
          const float half_block_size = PIXEL_SIZE * HALF_TILE_SIZE_IN_PIXELS;
          Quad_t collided_with_quad = {collided_with_center.x - half_block_size, collided_with_center.y - half_block_size,
                                       collided_with_center.x + half_block_size, collided_with_center.y + half_block_size};
          draw_quad_wireframe(&collided_with_quad, 255.0f, 0.0f, 255.0f);
#endif

// debug light outline
          const float half_coord_size = PIXEL_SIZE * HALF_TILE_SIZE_IN_PIXELS;
          for(S8 i = 0; i < g_illuminate_half_count; i++){
               Vec_t bottom_left = {(F32)(g_illuminate_halfs[i].x * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE,
                                    (F32)(g_illuminate_halfs[i].y * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE};
               Quad_t quad = {bottom_left.x, bottom_left.y,
                              bottom_left.x + half_coord_size, bottom_left.y + half_coord_size};
               draw_quad_wireframe(&quad, 255.0f, 0.0f, 255.0f);
          }
          if(g_illuminate_half_count){
               const float quarter_coord_size = PIXEL_SIZE * HALF_TILE_SIZE_IN_PIXELS * 0.5f;
               Vec_t light_start {(F32)(g_illuminate_halfs[0].x * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE + quarter_coord_size,
                                  (F32)(g_illuminate_halfs[0].y * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE + quarter_coord_size,};
               Vec_t light_end {(F32)(g_illuminate_halfs[g_illuminate_half_count - 1].x * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE + quarter_coord_size,
                                (F32)(g_illuminate_halfs[g_illuminate_half_count - 1].y * HALF_TILE_SIZE_IN_PIXELS) * PIXEL_SIZE + quarter_coord_size,};

               glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

               glBegin(GL_LINES);
               glColor3f(0, 1, 0);
               glVertex2f(light_start.x, light_start.y);
               glVertex2f(light_end.x, light_end.y);
               glEnd();

               glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
          }

#if 1
          // light
          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_QUADS);

          S32 light_min_x = min.x * 2;
          S32 light_min_y = min.y * 2;
          S32 light_max_x = max.x * 2 + 1;
          S32 light_max_y = max.y * 2 + 1;

          for(S32 y = light_min_y; y <= light_max_y; y++){
               for(S32 x = light_min_x; x <= light_max_x; x++){
                    U8 light = tilemap.light[y][x];

                    Vec_t tile_pos {(F32)(x - light_min_x) * HALF_TILE_SIZE + camera_offset.x,
                                    (F32)(y - light_min_y) * HALF_TILE_SIZE + camera_offset.y};
                    glColor4f(0.0f, 0.0f, 0.0f, (F32)(255 - light) / 255.0f);
                    glVertex2f(tile_pos.x, tile_pos.y);
                    glVertex2f(tile_pos.x, tile_pos.y + HALF_TILE_SIZE);
                    glVertex2f(tile_pos.x + HALF_TILE_SIZE, tile_pos.y + HALF_TILE_SIZE);
                    glVertex2f(tile_pos.x + HALF_TILE_SIZE, tile_pos.y);

               }
          }
          glEnd();
#endif

          static float demo_blink_timer = 0.0f;

          demo_blink_timer += dt;

          if(demo_blink_timer > 0.66f){

               if(demo_blink_timer > 1.22f) demo_blink_timer -= 1.22f;

               switch(demo_mode){
               default:
                    break;
               case DEMO_MODE_RECORD:
               {
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBegin(GL_POLYGON);
                    glColor3f(1.0f, 0.0f, 0.0f);
                    delta = 3.14159f * 2.0f / (F32)(segments);
                    angle = 0.0f  + delta;
                    for(S32 i = 0; i <= segments; i++){
                         F32 dx = cos(angle) * 0.01;
                         F32 dy = sin(angle) * 0.01;

                         glVertex2f(0.98f + dx, 0.02f + dy);
                         angle += delta;
                    }
                    glEnd();
               } break;
               case DEMO_MODE_PLAY:
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBegin(GL_TRIANGLES);
                    glColor3f(1.0f, 0.0f, 0.0f);
                    glVertex2f(0.975f, 0.01f);
                    glVertex2f(0.975f, 0.03f);
                    glVertex2f(0.995f, 0.02f);
                    glEnd();
                    break;
               }
          }

          {
               float reset_alpha = resetting / RESET_TIME;
               if(reset_alpha > 0){
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBegin(GL_QUADS);
                    glColor4f(0.0f, 0.0f, 0.0f, reset_alpha);
                    glVertex2f(0.0, 0.0);
                    glVertex2f(1.0, 0.0);
                    glVertex2f(1.0, 1.0);
                    glVertex2f(0.0, 1.0);
                    glEnd();
               }
          }

          // player start
          selection_draw(player_start, player_start, screen_camera, 0.0f, 1.0f, 0.0f);

          // editor
          switch(editor.mode){
          default:
               break;
          case EDITOR_MODE_OFF:
               // pass
               break;
          case EDITOR_MODE_CATEGORY_SELECT:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               Vec_t vec = {0.0f, 0.0f};

               for(S32 g = 0; g < editor.category_array.count; ++g){
                    auto* category = editor.category_array.elements + g;
                    auto* stamp_array = category->elements + 0;

                    for(S16 s = 0; s < stamp_array->count; s++){
                         auto* stamp = stamp_array->elements + s;
                         if(g && (g % ROOM_TILE_SIZE) == 0){
                              vec.x = 0.0f;
                              vec.y += TILE_SIZE;
                         }

                         switch(stamp->type){
                         default:
                              break;
                         case STAMP_TYPE_TILE_ID:
                              tile_id_draw(stamp->tile_id, vec);
                              break;
                         case STAMP_TYPE_TILE_FLAGS:
                              tile_flags_draw(stamp->tile_flags, vec);
                              tile_ice_draw(stamp->tile_flags, vec, theme_texture);
                              break;
                         case STAMP_TYPE_BLOCK:
                         {
                              Block_t block = {};
                              block.element = stamp->block.element;
                              block.face = stamp->block.face;

                              block_draw(&block, vec);
                         } break;
                         case STAMP_TYPE_INTERACTIVE:
                         {
                              interactive_draw(&stamp->interactive, vec);
                         } break;
                         }
                    }

                    vec.x += TILE_SIZE;
               }

               glEnd();
          } break;
          case EDITOR_MODE_STAMP_SELECT:
          case EDITOR_MODE_STAMP_HIDE:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               // draw stamp at mouse
               auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
               Coord_t mouse_coord = mouse_select_coord(mouse_screen);

               for(S16 s = 0; s < stamp_array->count; s++){
                    auto* stamp = stamp_array->elements + s;
                    Vec_t stamp_pos = coord_to_screen_position(mouse_coord + stamp->offset);
                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         tile_id_draw(stamp->tile_id, stamp_pos);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         tile_flags_draw(stamp->tile_flags, stamp_pos);
                         tile_ice_draw(stamp->tile_flags, stamp_pos, theme_texture);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;
                         block_draw(&block, stamp_pos);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         interactive_draw(&stamp->interactive, stamp_pos);
                    } break;
                    }
               }

               if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                    // draw stamps to select from at the bottom
                    Vec_t pos = {0.0f, 0.0f};
                    S16 row_height = 1;
                    auto* category = editor.category_array.elements + editor.category;

                    for(S32 g = 0; g < category->count; ++g){
                         stamp_array = category->elements + g;
                         Coord_t dimensions = stamp_array_dimensions(stamp_array);
                         if(dimensions.y > row_height) row_height = dimensions.y;

                         for(S32 s = 0; s < stamp_array->count; s++){
                              auto* stamp = stamp_array->elements + s;
                              Vec_t stamp_vec = pos + coord_to_vec(stamp->offset);

                              switch(stamp->type){
                              default:
                                   break;
                              case STAMP_TYPE_TILE_ID:
                                   tile_id_draw(stamp->tile_id, stamp_vec);
                                   break;
                              case STAMP_TYPE_TILE_FLAGS:
                                   tile_flags_draw(stamp->tile_flags, stamp_vec);
                                   tile_ice_draw(stamp->tile_flags, stamp_vec, theme_texture);
                                   break;
                              case STAMP_TYPE_BLOCK:
                              {
                                   Block_t block = {};
                                   block.element = stamp->block.element;
                                   block.face = stamp->block.face;
                                   block_draw(&block, stamp_vec);
                              } break;
                              case STAMP_TYPE_INTERACTIVE:
                              {
                                   interactive_draw(&stamp->interactive, stamp_vec);
                              } break;
                              }
                         }

                         pos.x += (dimensions.x * TILE_SIZE);
                         if(pos.x >= 1.0f){
                              pos.x = 0.0f;
                              pos.y += row_height * TILE_SIZE;
                              row_height = 1;
                         }
                    }
               }

               glEnd();
          } break;
          case EDITOR_MODE_CREATE_SELECTION:
               selection_draw(editor.selection_start, editor.selection_end, screen_camera, 1.0f, 0.0f, 0.0f);
               break;
          case EDITOR_MODE_SELECTION_MANIPULATION:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               for(S32 g = 0; g < editor.selection.count; ++g){
                    auto* stamp = editor.selection.elements + g;
                    Position_t stamp_pos = coord_to_pos(editor.selection_start + stamp->offset);
                    Vec_t stamp_vec = pos_to_vec(stamp_pos);

                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         tile_id_draw(stamp->tile_id, stamp_vec);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         tile_flags_draw(stamp->tile_flags, stamp_vec);
                         tile_ice_draw(stamp->tile_flags, stamp_vec, theme_texture);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;
                         block_draw(&block, stamp_vec);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         interactive_draw(&stamp->interactive, stamp_vec);
                    } break;
                    }
               }
               glEnd();

               Rect_t selection_bounds = editor_selection_bounds(&editor);
               Coord_t min_coord {selection_bounds.left, selection_bounds.bottom};
               Coord_t max_coord {selection_bounds.right, selection_bounds.top};
               selection_draw(min_coord, max_coord, screen_camera, 1.0f, 0.0f, 0.0f);
          } break;
          }

          SDL_GL_SwapWindow(window);
     }

     switch(demo_mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_END_DEMO, demo_mode,
                                demo_file, frame_count);
          // save map and player position
          save_map_to_file(demo_file, player_start, &tilemap, &block_array, &interactive_array);
          fwrite(&player.pos.pixel, sizeof(player.pos.pixel), 1, demo_file);
          fclose(demo_file);
          break;
     case DEMO_MODE_PLAY:
          fclose(demo_file);
          break;
     }

     quad_tree_free(interactive_quad_tree);
     quad_tree_free(block_quad_tree);

     destroy(&block_array);
     destroy(&interactive_array);
     destroy(&undo);
     destroy(&tilemap);
     destroy(&editor);

     if(!suite){
          glDeleteTextures(1, &theme_texture);
          glDeleteTextures(1, &player_texture);
          glDeleteTextures(1, &arrow_texture);

          SDL_GL_DeleteContext(opengl_context);
          SDL_DestroyWindow(window);
          SDL_Quit();
     }

     Log_t::destroy();

     return 0;
}
