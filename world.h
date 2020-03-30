#pragma once

#include "tile.h"
#include "player.h"
#include "interactive.h"
#include "block.h"
#include "arrow.h"
#include "quad_tree.h"
#include "undo.h"
#include "demo.h"

struct World_t{
     TileMap_t tilemap = {};
     ObjectArray_t<Player_t> players = {};
     ObjectArray_t<Block_t> blocks = {};
     ObjectArray_t<Interactive_t> interactives = {};
     ArrowArray_t arrows = {};

     QuadTreeNode_t<Interactive_t>* interactive_qt = nullptr;
     QuadTreeNode_t<Block_t>* block_qt = nullptr;

     S32 clone_instance = 0;
};

#define MAX_TELEPORT_POSITION_RESULTS 4

struct TeleportPosition_t{
     Position_t pos;
     Vec_t delta;
     U8 rotations = 0;
     Coord_t src_portal;
     Coord_t dst_portal;
};

struct TeleportPositionResult_t{
     TeleportPosition_t results[MAX_TELEPORT_POSITION_RESULTS];
     S8 count = 0;
};

struct MovePlayerThroughWorldResult_t{
     bool collided;
     bool resetting;
     Vec_t pos_delta;
     S16 pushing_block;
     Direction_t pushing_block_dir;
     S8 pushing_block_rotation;
};

struct ElasticCollisionResult_t{
     F32 first_final_velocity;
     F32 second_final_velocity;
};

struct BlockElasticCollision_t{
     S16 pusher_mass = 0;
     F32 pusher_velocity = 0;
     S16 pushee_mass = 0;
     F32 pushee_initial_velocity = 0;
     F32 pushee_velocity = 0;

     bool transferred_momentum_back(){return pusher_velocity != 0;}
};

#define BLOCK_PUSH_MAX_COLLISIONS 4

struct BlockPushResult_t{
     bool pushed = false;
     bool busy = false;

     BlockElasticCollision_t collisions[BLOCK_PUSH_MAX_COLLISIONS];
     S8 collision_count = 0;

     bool add_collision(S16 pusher_mass, F32 pusher_vel, S16 pushee_mass, F32 pushee_initial_vel, F32 pushee_vel){
         if(collision_count >= BLOCK_PUSH_MAX_COLLISIONS){
             assert(!"ya dun messed up A-ARON");
             return false;
         }
         auto& collision = collisions[collision_count];
         collision.pusher_mass = pusher_mass;
         collision.pusher_velocity = pusher_vel;
         collision.pushee_mass = pushee_mass;
         collision.pushee_initial_velocity = pushee_initial_vel;
         collision.pushee_velocity = pushee_vel;
         collision_count++;
         return true;
     }

     // TODO: do we need a merge?
};

struct BlockPushMoveDirectionResult_t{
     BlockPushResult_t horizontal_result;
     BlockPushResult_t vertical_result;
};

struct AllowedToPushResult_t{
     bool push = false;
     F32 mass_ratio = 0.0f;
};

void sort_blocks_by_ascending_height(Block_t** blocks, S16 block_count);
void sort_blocks_by_descending_height(Block_t** blocks, S16 block_count);

bool load_map_number(S32 map_number, Coord_t* player_start, World_t* world);
void reset_map(Coord_t player_start, World_t* world, Undo_t* undo);

void activate(World_t* world, Coord_t coord);

MovePlayerThroughWorldResult_t move_player_through_world(Position_t player_pos, Vec_t player_vel, Vec_t player_pos_delta,
                                                         Direction_t player_face, S8 player_clone_id, S16 player_index,
                                                         S16 player_pushing_block, Direction_t player_pushing_block_dir,
                                                         S8 player_pushing_block_rotation, World_t* world);

TeleportPositionResult_t teleport_position_across_portal(Position_t position, Vec_t pos_delta, World_t* world,
                                                         Coord_t premove_coord, Coord_t postmove_coord, bool require_on = true);

void illuminate(Coord_t coord, U8 value, World_t* world, Coord_t from_portal = Coord_t{-1, -1});

void spread_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported = false);
void melt_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported = false);

BlockPushMoveDirectionResult_t block_push(Block_t* block, MoveDirection_t move_direction, World_t* world, bool pushed_by_ice, F32 force = 1.0f, TransferMomentum_t* instant_momentum = nullptr);
BlockPushResult_t block_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force = 1.0f, TransferMomentum_t* instant_momentum = nullptr,
                             bool from_entangler = false);
BlockPushResult_t block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force = 1.0f, TransferMomentum_t* instant_momentum = nullptr, bool from_entangler = false);
bool block_pushable(Block_t* block, Direction_t direction, World_t* world);
bool reset_players(ObjectArray_t<Player_t>* players);

void describe_player(World_t* world, Player_t* player);
void describe_block(World_t* world, Block_t* block);
void describe_coord(Coord_t coord, World_t* world);
bool test_map_end_state(World_t* world, Demo_t* demo);

S16 get_block_index(World_t* world, Block_t* block);
bool setup_default_room(World_t* world);
void reset_tilemap_light(World_t* world);

bool block_in_height_range_of_player(Block_t* block, Position_t player);

void get_block_stack(World_t* world, Block_t* block, BlockList_t* block_list, S8 rotations_through_portal);
S16 get_block_stack_mass(World_t* world, Block_t* block);
S16 get_block_mass_in_direction(World_t* world, Block_t* block, Direction_t direction, bool require_on_ice = true);

F32 momentum_term(F32 mass, F32 vel);
F32 momentum_term(TransferMomentum_t* transfer_momentum);
TransferMomentum_t get_block_momentum(World_t* world, Block_t* block, Direction_t direction);
ElasticCollisionResult_t elastic_transfer_momentum(F32 mass_1, F32 vel_i_1, F32 mass_2, F32 vel_i_2);
ElasticCollisionResult_t elastic_transfer_momentum_to_block(TransferMomentum_t* transfer_momentum, World_t* world, Block_t* block, Direction_t direction);

F32 get_block_static_friction(S16 mass);
F32 get_block_expected_player_push_velocity(World_t* world, Block_t* block, F32 force = 1.0f);
AllowedToPushResult_t allowed_to_push(World_t* world, Block_t* block, Direction_t direction, F32 force = 1.0f, TransferMomentum_t* instant_momentum = nullptr);
