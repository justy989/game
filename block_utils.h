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
     Vec_t collision_pos_delta;
     Vec_t collision_overlap;

     U8 portal_rotations = 0;
     Coord_t src_portal_coord = Coord_t{0, 0};
     Coord_t dst_portal_coord = Coord_t{0, 0};
     bool invalidated = false;

     void init(Block_t* collided_block, Position_t collided_pos, Vec_t collided_pos_delta, Vec_t collided_overlap){
          block = collided_block;
          collision_pos = collided_pos;
          collision_pos_delta = collided_pos_delta;
          collision_overlap = collided_overlap;
     }

     void init_portal(Block_t* collided_block, Position_t collided_pos, Vec_t collided_pos_delta, Vec_t collided_overlap,
                      U8 through_portal_rotations, Coord_t src_portal, Coord_t dst_portal){
          init(collided_block, collided_pos, collided_pos_delta, collided_overlap);
          portal_rotations = through_portal_rotations;
          src_portal_coord = src_portal;
          dst_portal_coord = dst_portal;
     }
};

#define MAX_BLOCK_INSIDE_OTHERS_COUNT 16

using BlockInsideOthersResult_t = StaticObjectArray_t<BlockInsideBlockResult_t, MAX_BLOCK_INSIDE_OTHERS_COUNT>;

#define MAX_BLOCK_PUSHERS 4

struct BlockPusher_t{
     S16 index = 0;
     S16 collided_with_block_count = 1;
     bool hit_entangler = false; // TODO: can we remove this field ?
     S8 entangle_rotations = 0;
     S8 portal_rotations = 0;
     bool opposite_entangle_reversed = false; // TODO: remove this field
     S16 momentum_kicked_back_in_shared_push_block_index = -1;
};

struct BlockMomentumPush_t{
     BlockPusher_t pushers[MAX_BLOCK_PUSHERS];
     S8 pusher_count = 0;
     S16 pushee_index = -1;
     DirectionMask_t direction_mask = DIRECTION_MASK_NONE;
     S8 portal_rotations = 0;
     S8 entangle_rotations = 0;
     S8 pusher_rotations = 0;
     bool invalidated = false;
     bool no_consolidate = false;
     bool no_entangled_pushes = false;
     S8 entangled_with_push_index = -1; // index of which push caused this push via entanglement
     F32 entangled_momentum = 0;
     bool pure_entangle = false; // the block we are pushing isn't against any other blocks, so the push just uses the forces given to it
     F32 force = 1.0f;
     bool executed = false;
     S16 collided_with_block_count = 1;

     bool add_pusher(S16 index, S16 pusher_collided_with_block_count = 1, bool hit_entangler = false,
                     S8 pusher_entangle_rotations = 0, S8 pusher_portal_rotations = 0){
          if(pusher_count >= MAX_BLOCK_PUSHERS) return false;
          pushers[pusher_count].index = index;
          pushers[pusher_count].collided_with_block_count = pusher_collided_with_block_count;
          pushers[pusher_count].hit_entangler = hit_entangler;
          pushers[pusher_count].entangle_rotations = pusher_entangle_rotations;
          pushers[pusher_count].portal_rotations = pusher_portal_rotations;
          pusher_count++;
          return true;
     }

     bool remove_pusher(S16 index){
          if(index < 0 || index >= pusher_count) return false;

          // if there only 1 remove it and invalidate it
          if(pusher_count == 1){
              pusher_count = 0;
              invalidated = true;
          }else{
              // swap the last one to this place
              S16 last_index = pusher_count - 1;
              pushers[index] = pushers[last_index];
              pusher_count--;
          }

          return true;
     }

    bool is_entangled(){
        return entangled_with_push_index >= 0;
    }
};

template <S16 MAX_BLOCK_PUSHES>
struct BlockMomentumPushes_t{
     BlockMomentumPush_t pushes[MAX_BLOCK_PUSHES];
     S16 count = 0;

     bool add(BlockMomentumPush_t* push){
          if(count < MAX_BLOCK_PUSHES){
               pushes[count] = *push;
               count++;
               return true;
          }

          return false;
     }

     template <S16 ALTERNATE_MAX_BLOCK_PUSHES>
     void merge(BlockMomentumPushes_t<ALTERNATE_MAX_BLOCK_PUSHES>* alternate_pushes){
          for(S16 p = 0; p < alternate_pushes->count; p++){
               BlockMomentumPush_t* alternate = alternate_pushes->pushes + p;
               add(alternate);
          }
     }

     bool push_already_exists(BlockMomentumPush_t* push){
          for(S16 i = 0; i < count; i++){
               BlockMomentumPush_t* check = pushes + i;
               if(check->executed) continue;
               if(check->pusher_count <= 0) continue;

               if((check->pushers[0].index == push->pushers[0].index &&
                   check->pushee_index == push->pushee_index) ||
                  (check->pushee_index == push->pushers[0].index &&
                   check->pushers[0].index == push->pushee_index)){
                    return true;
               }
          }

          return false;
     }

     void clear(){
          count = 0;
     }
};

struct BlockCollidedWithBlock_t{
     S16 block_index = -1;
     DirectionMask_t direction_mask = DIRECTION_MASK_NONE;
     S8 portal_rotations = 0;
};

using BlockCollidedWithBlocks_t = StaticObjectArray_t<BlockCollidedWithBlock_t, MAX_BLOCK_INSIDE_OTHERS_COUNT>;

struct CheckBlockCollisionResult_t{
     bool collided = false;
     S16 block_index = -1;
     bool same_as_next = false;
     bool unused = false;

     Vec_t pos_delta;
     Vec_t original_vel;
     Vec_t vel;
     Vec_t accel;

     S16 stop_on_pixel_x = -1;
     S16 stop_on_pixel_y = -1;

     bool stopped_by_player_horizontal;
     bool stopped_by_player_vertical;

     Move_t horizontal_move;
     Move_t vertical_move;

     F32 collided_distance;

     // TODO: I don't think we use this anymore ?
     Position_t collided_pos;
     S16 collided_block_index;
     U8 collided_portal_rotations;
     DirectionMask_t collided_dir_mask;

     Coord_t collided_src_portal = Coord_t{0, 0};
     Coord_t collided_dst_portal = Coord_t{0, 0};

     F32 horizontal_momentum;
     F32 vertical_momentum;

     BlockCollidedWithBlocks_t collided_with_blocks;

     void stop_horizontally(){
          reset_move(&horizontal_move);
          vel.x = 0;
          accel.x = 0;
          stopped_by_player_horizontal = false;
     }

     void stop_vertically(){
          reset_move(&vertical_move);
          vel.y = 0;
          accel.y = 0;
          stopped_by_player_vertical = false;
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
     bool through_portal = false;
};

using BlockAgainstOthersResult_t = StaticObjectArray_t<BlockAgainstOther_t, MAX_BLOCKS_AGAINST_BLOCK>;

#define MAX_BLOCKS_IN_CHAIN 16

struct BlockChainEntry_t{
     Block_t* block = 0;
     S8 rotations_through_portal = 0;
};

using BlockChain_t = StaticObjectArray_t<BlockChainEntry_t, MAX_BLOCKS_IN_CHAIN>;
using BlockChainsResult_t = StaticObjectArray_t<BlockChain_t, MAX_BLOCKS_AGAINST_BLOCK>;

#define MAX_BLOCK_COLLISIONS 32

struct BlockMomentumCollision_t{
     S16 block_index = -1;
     S16 mass = 0;
     F32 vel = 0;
     bool x = false;
     Direction_t from = DIRECTION_COUNT;
     S16 from_block = -1;
     bool momentum_transfer = false; // whether or not the momentum collision happened. if the push is not successful,
                                     // that means the chain was probably against a wall or player
     S16 split_mass_between_blocks = 1;

     void init(S16 block_id, S16 masss, F32 velocity, bool is_x, Direction_t is_from, S16 from_block_id,
               bool momentum_was_transferred, S16 has_split_mass_between_blocks){
          block_index = block_id;
          mass = masss;
          vel = velocity;
          x = is_x;
          from = is_from;
          from_block = from_block_id;
          momentum_transfer = momentum_was_transferred;
          split_mass_between_blocks = has_split_mass_between_blocks;
     }
};

#define MAX_BLOCK_PUSHES 128

struct BlockCollisionPushResult_t{
     BlockMomentumPushes_t<MAX_BLOCK_PUSHES> additional_block_pushes;
};

struct BlockThroughPortal_t{
    Position_t position;
    Block_t* block = NULL;
    Direction_t src_portal_dir = DIRECTION_COUNT;
    Direction_t dst_portal_dir = DIRECTION_COUNT;
    Coord_t src_portal;
    Coord_t dst_portal;
    S8 portal_rotations = 0;
    S8 rotations_between_portals = 0;
    BlockCut_t rotated_cut = BLOCK_CUT_WHOLE;
};

#define MAX_BLOCKS_FOUND_THROUGH_PORTALS 64

using FindBlocksThroughPortalResult_t = StaticObjectArray_t<BlockThroughPortal_t, MAX_BLOCKS_FOUND_THROUGH_PORTALS>;

void add_block_held(BlockHeldResult_t* result, Block_t* block, Rect_t rect);
void add_interactive_held(InteractiveHeldResult_t* result, Interactive_t* interactive, Rect_t rect);

bool block_adjacent_pixels_to_check(Position_t pos, Vec_t pos_delta, BlockCut_t cut, Direction_t direction, Pixel_t* a, Pixel_t* b);

Block_t* block_against_block_in_list(Position_t pos, BlockCut_t cut, Block_t** blocks, S16 block_count, Direction_t direction, Position_t* portal_offsets);
Block_t* block_against_another_block(Position_t pos, BlockCut_t cut, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, Direction_t* push_dir);
BlockAgainstOther_t block_diagonally_against_block(Position_t pos, BlockCut_t cut, DirectionMask_t directions, TileMap_t* tilemap,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);
BlockAgainstOthersResult_t block_against_other_blocks(Position_t pos, BlockCut_t cut, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                      QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, bool require_portal_on = true);
Block_t* rotated_entangled_blocks_against_centroid(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                                   ObjectArray_t<Block_t>* blocks_array,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap);
Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt);

BlockInsideOthersResult_t block_inside_others(Position_t block_to_check_pos, Vec_t block_to_check_pos_delta,
                                              BlockCut_t cut, S16 block_to_check_index,
                                              bool block_to_check_cloning, QuadTreeNode_t<Block_t>* block_qt,
                                              QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap,
                                              ObjectArray_t<Block_t>* block_array);
Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap);
Tile_t* block_against_solid_tile(Position_t block_pos, Vec_t pos_delta, BlockCut_t cut, Direction_t direction, TileMap_t* tilemap);
bool block_diagonally_against_solid(Position_t block_pos, Vec_t pos_delta, BlockCut_t cut, Direction_t horizontal_direction,
                                    Direction_t vertical_direction, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt);
Player_t* block_against_player(Block_t* block_to_check, Direction_t direction, ObjectArray_t<Player_t>* players);

InteractiveHeldResult_t block_held_up_by_popup(Position_t block_pos, BlockCut_t cut, QuadTreeNode_t<Interactive_t>* interactive_qt, S16 min_area = 0);
BlockHeldResult_t block_held_up_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                                 QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);
BlockHeldResult_t block_held_down_by_another_block(Block_t* block, QuadTreeNode_t<Block_t>* block_qt,
                                                   QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S16 min_area = 0);
BlockHeldResult_t block_held_down_by_another_block(Pixel_t block_pixel, S8 block_z, BlockCut_t cut,
                                                   QuadTreeNode_t<Block_t>* block_qt, QuadTreeNode_t<Interactive_t>* interactive_qt,
                                                   TileMap_t* tilemap, S16 min_area = 0, bool include_pos_delta = true);

bool block_on_ice(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                  QuadTreeNode_t<Block_t>* block_qt);
bool block_on_ice(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);

bool block_on_air(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);
bool block_on_air(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);

bool block_on_frictionless(Position_t pos, Vec_t pos_delta, BlockCut_t cut, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                           QuadTreeNode_t<Block_t>* block_qt);
bool block_on_frictionless(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);

CheckBlockCollisionResult_t check_block_collision_with_other_blocks(Position_t block_pos, Vec_t block_pos_delta, Vec_t block_vel,
                                                                    Vec_t block_accel, BlockCut_t cut, S16 block_stop_on_pixel_x,
                                                                    S16 block_stop_on_pixel_y, Move_t block_horizontal_move,
                                                                    Move_t block_vertical_move, bool stopped_by_player_horizontal,
                                                                    bool stopped_by_player_vertical, S16 block_index,
                                                                    bool block_is_cloning, World_t* world);

BlockCollidesWithItselfResult_t resolve_block_colliding_with_itself(Direction_t src_portal_dir, Direction_t dst_portal_dir, DirectionMask_t move_mask,
                                                                    Position_t block_pos);

Interactive_t* block_is_teleporting(Block_t* block, QuadTreeNode_t<Interactive_t>* interactive_qt);

void push_entangled_block(Block_t* block, World_t* world, Direction_t push_dir, bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum = nullptr);
bool blocks_are_entangled(Block_t* a, Block_t* b, ObjectArray_t<Block_t>* blocks_array);
bool blocks_are_entangled(S16 a_index, S16 b_index, ObjectArray_t<Block_t>* blocks_array);

TransferMomentum_t get_block_push_pusher_momentum(BlockMomentumPush_t* push, World_t* world, Direction_t push_direction);
BlockCollisionPushResult_t block_collision_push(BlockMomentumPush_t* push, World_t* world);

FindBlocksThroughPortalResult_t find_blocks_through_portals(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt,
                                                            bool require_on = true);
// LOL
BlockChainsResult_t find_block_chain(Block_t* block, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                                     QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap, S8 rotations = 0, BlockChain_t* my_chain = NULL);
TransferMomentum_t get_block_push_pusher_momentum(BlockMomentumPush_t* push, World_t* world, Direction_t push_direction);
