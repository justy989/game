#include "block_utils.h"
#include "utils.h"
#include "defines.h"
#include "conversion.h"
#include "portal_exit.h"

Pixel_t g_collided_with_pixel = {};

bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     if(block->pos.z == 0){
          Coord_t coord = block_get_coord(block);
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
