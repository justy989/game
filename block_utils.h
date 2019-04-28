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

struct CheckBlockCollisionResult_t{
     bool collided;

     Vec_t pos_delta;
     Vec_t vel;
     Vec_t accel;

     S16 stop_on_pixel_x;
     S16 stop_on_pixel_y;

     Move_t horizontal_move;
     Move_t vertical_move;

     Position_t collided_pos;
     S16 collided_block_index;
     U8 collided_portal_rotations;
};

struct BlockCollidesWithItselfResult_t{
     Direction_t push_dir;
     Vec_t vel;
     Vec_t accel;
};

#define MAX_HELD_BLOCKS 16

struct BlockHeld_t{
     Block_t* block = nullptr;
     Rect_t rect;
};

struct BlockHeldResult_t{
     BlockHeld_t blocks_held[MAX_HELD_BLOCKS];
     S16 count = 0;

     bool held(){return count > 0;}
};

void add_block_held(BlockHeldResult_t* result, Block_t* block, Rect_t rect);

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, Direction_t direction, Pixel_t* a, Pixel_t* b);

Block_t* block_against_block_in_list(Position_t pos, Block_t** blocks, S16 block_count, Direction_t direction, Pixel_t* offsets);
Block_t* block_against_another_block(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir);
Block_t* rotated_entangled_blocks_against_centroid(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                   ObjectArray_t<Block_t>* blocks_array,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap);
Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt);

BlockInsideResult_t block_inside_another_block(Block_t* block_to_check, QuadTreeNode_t<Block_t>* block_qt,
                                               QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                               ObjectArray_t<Block_t>* block_array);
Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap);
Player_t* block_against_player(Block_t* block_to_check, Direction_t direction, ObjectArray_t<Player_t>* players);

Interactive_t* block_held_up_by_popup(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt, S16 min_area = 0);
BlockHeldResult_t block_held_up_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);
BlockHeldResult_t block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);

// Rect_t block_over_air(Block_t* block, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt);

bool block_on_ice(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                  QuadTreeNode_t<Block_t>* block_qt);

bool block_on_air(Position_t pos, Vec_t pos_delta, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, QuadTreeNode_t<Block_t>* block_qt);

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, S16 block_stop_on_pixel_x, S16 block_stop_on_pixel_y,
                                                                    Move_t block_horizontal_move, Move_t block_vertical_move, S16 block_index,
                                                                    bool block_is_cloning, World_t* world);

BlockCollidesWithItselfResult_t resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                                                    Vec_t block_vel, Vec_t block_accel, Direction_t check_horizontal, Direction_t check_vertical);

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_qt, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Pixel_t* offsets);

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt);

void push_entangled_block(Block_t* block, World_t* world, Direction_t push_dir, bool pushed_by_ice, F32 instant_vel = 0);
bool blocks_are_entangled(Block_t* a, Block_t* b, ObjectArray_t<Block_t>* blocks_array);
bool blocks_are_entangled(S16 a_index, S16 b_index, ObjectArray_t<Block_t>* blocks_array);

extern Pixel_t g_collided_with_pixel;
