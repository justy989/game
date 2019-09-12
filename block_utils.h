#pragma once

#include "block.h"
#include "tile.h"
#include "interactive.h"
#include "player.h"
#include "quad_tree.h"
#include "world.h"

struct BlockInsideBlockResult_t{
     Block_t* block = nullptr;
     Position_t collision_pos;
     U8 portal_rotations = 0;
     Coord_t src_portal_coord;
     Coord_t dst_portal_coord;
     bool invalidated = false;
};

#define MAX_BLOCK_INSIDE_OTHERS_COUNT 16

struct BlockInsideOthersResult_t{
     BlockInsideBlockResult_t entries[MAX_BLOCK_INSIDE_OTHERS_COUNT];
     S8 count = 0;

     bool add(Block_t* block, Position_t collision_pos, U8 portal_rotations, Coord_t src_portal_coord, Coord_t dst_portal_coord){
          if(count >= MAX_BLOCK_INSIDE_OTHERS_COUNT) return false;
          // refuse duplicates
          for(S8 i = 0; i < count; i++){
               if(block == entries[i].block) return false;
          }
          entries[count].block = block;
          entries[count].collision_pos = collision_pos;
          entries[count].portal_rotations = portal_rotations;
          entries[count].src_portal_coord = src_portal_coord;
          entries[count].dst_portal_coord = dst_portal_coord;
          entries[count].invalidated = false;
          count++;
          return true;
     }
};

enum BlockChangeType_t : S8{
     BLOCK_CHANGE_TYPE_POS_DELTA_X,
     BLOCK_CHANGE_TYPE_POS_DELTA_Y,
     BLOCK_CHANGE_TYPE_VEL_X,
     BLOCK_CHANGE_TYPE_VEL_Y,
     BLOCK_CHANGE_TYPE_ACCEL_X,
     BLOCK_CHANGE_TYPE_ACCEL_Y,
     BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_STATE,
     BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_SIGN,
     BLOCK_CHANGE_TYPE_HORIZONTAL_MOVE_TIME_LEFT,
     BLOCK_CHANGE_TYPE_VERTICAL_MOVE_STATE,
     BLOCK_CHANGE_TYPE_VERTICAL_MOVE_SIGN,
     BLOCK_CHANGE_TYPE_VERTICAL_MOVE_TIME_LEFT,
     BLOCK_CHANGE_TYPE_STOP_ON_PIXEL_X,
     BLOCK_CHANGE_TYPE_STOP_ON_PIXEL_Y,
};

struct BlockChange_t{
     S16 block_index;
     BlockChangeType_t type;

     union{
          S16 integer;
          F32 decimal;
          MoveState_t move_state;
          MoveSign_t move_sign;
     };
};

#define MAX_BLOCK_CHANGES 32

struct BlockChanges_t{
     BlockChange_t changes[MAX_BLOCK_CHANGES];
     S16 count = 0;

     bool add(BlockChange_t* change){
          if(count < MAX_BLOCK_CHANGES){
               changes[count] = *change;
               count++;
               return true;
          }

          return false;
     }

     bool add(S16 block_index, BlockChangeType_t type, S16 integer){
          BlockChange_t change;
          change.type = type;
          change.block_index = block_index;
          change.integer = integer;
          return add(&change);
     }

     bool add(S16 block_index, BlockChangeType_t type, F32 decimal){
          BlockChange_t change;
          change.type = type;
          change.block_index = block_index;
          change.decimal = decimal;
          return add(&change);
     }

     bool add(S16 block_index, BlockChangeType_t type, MoveState_t move_state){
          BlockChange_t change;
          change.type = type;
          change.block_index = block_index;
          change.move_state = move_state;
          return add(&change);
     }

     bool add(S16 block_index, BlockChangeType_t type, MoveSign_t move_sign){
          BlockChange_t change;
          change.type = type;
          change.block_index = block_index;
          change.move_sign = move_sign;
          return add(&change);
     }

     void merge(BlockChanges_t* block_changes){
          for(S16 i = 0; i < block_changes->count; i++){
               add(block_changes->changes + i);
          }
     }

     void clear(){count = 0;}
};

#define MAX_BLOCK_PUSHERS 4

struct BlockPush_t{
     S16 pusher_indices[MAX_BLOCK_PUSHERS];
     S8 pusher_count = 0;
     S16 pushee_index = -1;
     DirectionMask_t direction_mask = DIRECTION_MASK_NONE;
     S8 portal_rotations;
     S16 collided_with_block_count = 1;
     bool invalidated = false;
     bool entangled = false;

     bool add_pusher(S16 index){
          if(pusher_count >= MAX_BLOCK_PUSHERS) return false;
          pusher_indices[pusher_count] = index;
          pusher_count++;
          return true;
     }
};

template <S16 MAX_BLOCK_PUSHES>
struct BlockPushes_t{
     BlockPush_t pushes[MAX_BLOCK_PUSHES];
     S16 count = 0;

     bool add(BlockPush_t* push){
          if(count < MAX_BLOCK_PUSHES){
               pushes[count] = *push;
               count++;
               return true;
          }

          return false;
     }

     // TODO: it'd be nice if this wasn't N^2
     template <S16 ALTERNATE_MAX_BLOCK_PUSHES>
     void merge(BlockPushes_t<ALTERNATE_MAX_BLOCK_PUSHES>* alternate_pushes){
          for(S16 p = 0; p < alternate_pushes->count; p++){
               BlockPush_t* alternate = alternate_pushes->pushes + p;
               bool unique = true;

               if(alternate->pusher_count > 0){
                    for(S16 i = 0; i < count; i++){
                         BlockPush_t* check = pushes + i;
                         if(check->pusher_count <= 0) continue;
                         if((check->pusher_indices[0] == alternate->pusher_indices[0] &&
                             check->pushee_index == alternate->pushee_index) ||
                            (check->pushee_index == alternate->pusher_indices[0] &&
                             check->pusher_indices[0] == alternate->pushee_index)){
                              unique = false;
                              break;
                         }
                    }
               }

               if(unique) add(alternate);
          }
     }
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

     F32 horizontal_momentum;
     F32 vertical_momentum;

     BlockPushes_t<4> block_pushes;
     // BlockChanges_t block_changes;

     void stop_horizontally(){
          reset_move(&horizontal_move);
          vel.x = 0;
          accel.x = 0;
     }

     void stop_vertically(){
          reset_move(&vertical_move);
          vel.y = 0;
          accel.y = 0;
     }
};

struct BlockCollidesWithItselfResult_t{
     S16 stop_on_pixel_x = -1;
     S16 stop_on_pixel_y = -1;
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

#define MAX_HELD_INTERACTIVES 4

struct InteractiveHeld_t{
     Interactive_t* interactive = nullptr;
     Rect_t rect;
};

struct InteractiveHeldResult_t{
     InteractiveHeld_t interactives_held[MAX_HELD_INTERACTIVES];
     S16 count = 0;

     bool held(){return count > 0;}
};

#define MAX_BLOCKS_AGAINST_BLOCK 4

struct BlockAgainstOther_t{
     Block_t* block = nullptr;
     S8 rotations_through_portal = 0;
};

struct BlockAgainstOthersResult_t{
     BlockAgainstOther_t againsts[MAX_BLOCKS_AGAINST_BLOCK];
     S16 count = 0;

     bool add(BlockAgainstOther_t against_other){
          if(count >= MAX_BLOCKS_AGAINST_BLOCK) return false;
          // refuse duplicates
          for(S8 i = 0; i < count; i++){
               if(againsts[i].block == against_other.block) return false;
          }
          againsts[count] = against_other;
          count++;
          return true;
     }
};

struct BlockMomentumChange_t{
     S16 block_index = -1;
     F32 change = 0;
     bool x = false;
};

struct BlockMomentumChanges_t{
     BlockMomentumChange_t changes[MAX_BLOCK_CHANGES];
     S16 count = 0;

     bool add(S16 block_index, F32 change, bool x){
          if(count >= MAX_BLOCK_CHANGES) return false;
          changes[count].block_index = block_index;
          changes[count].change = change;
          changes[count].x = x;
          count++;
          return true;
     }

     void merge(BlockMomentumChanges_t* block_changes){
          for(S16 i = 0; i < block_changes->count; i++){
               auto* change = block_changes->changes + i;
               add(change->block_index, change->change, change->x);
          }
     }
};

struct BlockPushed_t{
     S16 block_index;
     Direction_t direction;
};

#define MAX_BLOCKS_PUSHED 8
#define MAX_ENTANGLE_BLOCK_PUSHES (MAX_BLOCKS_PUSHED * 4)

struct BlockCollisionPushResult_t{
     BlockPushes_t<MAX_ENTANGLE_BLOCK_PUSHES> entangled_block_pushes;
     BlockPushed_t blocks_pushed[MAX_BLOCKS_PUSHED];
     S8 count = 0;

     bool add_block_pushed(S16 block_index, Direction_t direction){
          if(count >= MAX_BLOCKS_PUSHED) return false;
          blocks_pushed[count].block_index = block_index;
          blocks_pushed[count].direction = direction;
          count++;
          return true;
     }

     BlockMomentumChanges_t momentum_changes;
};

void add_block_held(BlockHeldResult_t* result, Block_t* block, Rect_t rect);
void add_interactive_held(InteractiveHeldResult_t* result, Interactive_t* interactive, Rect_t rect);

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, Direction_t direction, Pixel_t* a, Pixel_t* b);

Block_t* block_against_block_in_list(Position_t pos, Block_t** blocks, S16 block_count, Direction_t direction, Position_t* portal_offsets);
Block_t* block_against_another_block(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap, Direction_t* push_dir);
BlockAgainstOthersResult_t block_against_other_blocks(Position_t pos, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                      QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap);
Block_t* rotated_entangled_blocks_against_centroid(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                   ObjectArray_t<Block_t>* blocks_array,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap);
Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt);

BlockInsideOthersResult_t block_inside_others(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta, S16 block_to_check_index,
                                               bool block_to_check_cloning, QuadTreeNode_t<Block_t>* block_qt,
                                               QuadTreeNode_t<Interactive_t>* interactive_quad_tree, TileMap_t* tilemap,
                                               ObjectArray_t<Block_t>* block_array);
Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap);
Tile_t* block_against_solid_tile(Position_t block_pos, Vec_t pos_delta, Direction_t direction, TileMap_t* tilemap);
Player_t* block_against_player(Block_t* block_to_check, Direction_t direction, ObjectArray_t<Player_t>* players);

InteractiveHeldResult_t block_held_up_by_popup(Position_t block_pos, QuadTreeNode_t<Interactive_t>* interactive_qt, S16 min_area = 0);
BlockHeldResult_t block_held_up_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);
BlockHeldResult_t block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);
BlockHeldResult_t block_held_down_by_another_block(Pixel_t block_pixel, S8 block_z, QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);

bool block_on_ice(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                  QuadTreeNode_t<Block_t>* block_qt);

bool block_on_air(Position_t pos, Vec_t pos_delta, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);
bool block_on_air(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, S16 block_stop_on_pixel_x, S16 block_stop_on_pixel_y,
                                                                    Move_t block_horizontal_move, Move_t block_vertical_move,
                                                                    S16 block_index, bool block_is_cloning, World_t* world);

BlockCollidesWithItselfResult_t resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                                                    Position_t block_pos);

void search_portal_destination_for_blocks(QuadTreeNode_t<Block_t>* block_qt, Direction_t src_portal_face,
                                          Direction_t dst_portal_face, Coord_t src_portal_coord,
                                          Coord_t dst_portal_coord, Block_t** blocks, S16* block_count, Position_t* offsets);

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt);

void push_entangled_block(Block_t* block, World_t* world, Direction_t push_dir, bool pushed_by_ice, TransferMomentum_t* instant_momentum = nullptr);
bool blocks_are_entangled(Block_t* a, Block_t* b, ObjectArray_t<Block_t>* blocks_array);
bool blocks_are_entangled(S16 a_index, S16 b_index, ObjectArray_t<Block_t>* blocks_array);

void apply_block_change(ObjectArray_t<Block_t>* blocks_array, BlockChange_t* change);
BlockCollisionPushResult_t block_collision_push(BlockPush_t* push, World_t* world);

extern Pixel_t g_collided_with_pixel;
