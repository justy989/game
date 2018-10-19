#pragma once

#include "block.h"
#include "tile.h"
#include "interactive.h"
#include "player.h"
#include "quad_tree.h"
#include "world.h"

struct BlockInsideResult_t{
     Block_t* block;
     Position_t collision_pos;
     U8 portal_rotations;
     Coord_t src_portal_coord;
     Coord_t dst_portal_coord;
};

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, Direction_t direction, Pixel_t* a, Pixel_t* b);

Block_t* block_against_block_in_list(Block_t* block_to_check, Block_t** blocks, S16 block_count, Direction_t direction, Pixel_t* offsets);
Block_t* block_against_another_block(Block_t* block_to_check, Direction_t direction, QuadTreeNode_t<Block_t>* block_quad_tree,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir);
Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);

Block_t* block_inside_block_list(Block_t* block_to_check, Block_t** blocks, S16 block_count, ObjectArray_t<Block_t>* blocks_array,
                                 Position_t* collided_with, Pixel_t* portal_offsets);
BlockInsideResult_t block_inside_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree,
                                               QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                               ObjectArray_t<Block_t>* block_array);
Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap,
                                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree);

Block_t* block_held_up_by_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_quad_tree);

bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);

bool check_block_collision_with_other_blocks(Block_t* block_to_check, World_t* world);

void resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                         Block_t* block, Direction_t check_horizontal, Direction_t check_vertical, Direction_t* push_dir);

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_quad_tree, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Pixel_t* offsets);

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt);

extern Pixel_t g_collided_with_pixel;
