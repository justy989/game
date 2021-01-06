#pragma once

#include "tile.h"
#include "player.h"
#include "interactive.h"
#include "block.h"
#include "arrow.h"
#include "quad_tree.h"
#include "undo.h"
#include "raw.h"
#include "camera.h"
#include "static_object_array.h"

struct World_t{
     TileMap_t tilemap = {};
     ObjectArray_t<Player_t> players = {};
     ObjectArray_t<Block_t> blocks = {};
     ObjectArray_t<Interactive_t> interactives = {};
     ArrowArray_t arrows = {};

     QuadTreeNode_t<Interactive_t>* interactive_qt = NULL;
     QuadTreeNode_t<Block_t>* block_qt = NULL;

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
     F32 pusher_initial_velocity = 0;
     F32 pusher_velocity = 0;
     S16 pushee_mass = 0;
     F32 pushee_initial_velocity = 0;
     F32 pushee_velocity = 0;
     S16 pushee_index = -1;
     Direction_t direction_pushee_hit = DIRECTION_COUNT;

     void init(S16 pusher_masss, F32 pusher_initial_vel, F32 pusher_vel, S16 pushee_masss, F32 pushee_initial_vel,
               F32 pushee_vel, S16 the_pushee_index, Direction_t direction_pushee_was_hit){
          pusher_mass = pusher_masss;
          pusher_initial_velocity = pusher_initial_vel;
          pusher_velocity = pusher_vel;
          pushee_mass = pushee_masss;
          pushee_initial_velocity = pushee_initial_vel;
          pushee_velocity = pushee_vel;
          pushee_index = the_pushee_index;
          direction_pushee_hit = direction_pushee_was_hit;
     }

     bool transferred_momentum_back(){return pusher_velocity != 0;}
};

#define BLOCK_PUSH_MAX_COLLISIONS 4
#define BLOCK_PUSH_MAX_AGAINSTS 8

struct BlockPushedAgainst_t{
     Block_t* block = NULL;
     Direction_t direction = DIRECTION_COUNT;
};

struct BlockPushResult_t{
     bool pushed = false;
     bool busy = false;

     StaticObjectArray_t<BlockElasticCollision_t, BLOCK_PUSH_MAX_COLLISIONS> collisions;
     StaticObjectArray_t<BlockPushedAgainst_t, BLOCK_PUSH_MAX_AGAINSTS> againsts_pushed;
};

struct BlockPushMoveDirectionResult_t{
     BlockPushResult_t horizontal_result;
     BlockPushResult_t vertical_result;
};

struct AllowedToPushResult_t{
     bool push = false;
     F32 mass_ratio = 0.0f;
};

struct PushFromEntangler_t{
    F32 accel = 0.0f;
    F32 move_time_left = 0.0f;
    F32 coast_vel = 0.0f;
    BlockCut_t cut = BLOCK_CUT_WHOLE;
};

struct LogMapNumberResult_t{
    bool success = false;
    char* filepath = NULL;
};

#define MAX_PLAYER_IN_BLOCK_RECT_RESULTS 16

struct PlayerInBlockRectResult_t{
     struct Entry_t{
          Block_t* block = nullptr;
          Position_t block_pos;
          S8 portal_rotations = 0;
     };

     StaticObjectArray_t<Entry_t, MAX_PLAYER_IN_BLOCK_RECT_RESULTS> entries;
};

void sort_blocks_by_ascending_height(Block_t** blocks, S16 block_count);
void sort_blocks_by_descending_height(Block_t** blocks, S16 block_count);

LogMapNumberResult_t load_map_number(S32 map_number, Coord_t* player_start, World_t* world);
void reset_map(Coord_t player_start, World_t* world, Undo_t* undo, Camera_t* camera);

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

void update_light_and_ice_detectors(Interactive_t* interactive, World_t* world);

BlockPushMoveDirectionResult_t block_push(Block_t* block, MoveDirection_t move_direction, World_t* world, bool pushed_by_ice,
                                          F32 force = 1.0f, TransferMomentum_t* instant_momentum = NULL, PushFromEntangler_t* from_entangler = NULL,
                                          S16 block_contributing_momentum_to_total_blocks = 1, bool side_effects = false);
bool block_would_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                      bool pushed_by_ice, F32 force = 1.0f, TransferMomentum_t* instant_momentum = NULL,
                      PushFromEntangler_t* from_entangler = NULL, S16 block_contributing_momentum_to_total_blocks = 1,
                      bool side_effects = true, BlockPushResult_t* result = NULL);
void block_do_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                   bool pushed_by_ice, BlockPushResult_t* result, F32 force = 1.0f,
                   TransferMomentum_t* instant_momentum = NULL, PushFromEntangler_t* from_entangler = NULL,
                   S16 block_contributing_momentum_to_total_blocks = 1);
// side_effects bool is to fake call these to see what would happen to a given block. The side effects are pushing other blocks
BlockPushResult_t block_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                             bool pushed_by_ice, F32 force = 1.0f, TransferMomentum_t* instant_momentum = NULL,
                             PushFromEntangler_t* from_entangler = NULL,
                             S16 block_contributing_momentum_to_total_blocks = 1, bool side_effects = true);
BlockPushResult_t block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force = 1.0f,
                             TransferMomentum_t* instant_momentum = NULL, PushFromEntangler_t* from_entangler = NULL,
                             S16 block_contributing_momentum_to_total_blocks = 1, bool side_effects = true);
bool block_pushable(Block_t* block, Direction_t direction, World_t* world, F32 force);
bool reset_players(ObjectArray_t<Player_t>* players);

void describe_player(World_t* world, Player_t* player);
void describe_block(World_t* world, Block_t* block);
void describe_coord(Coord_t coord, World_t* world);

S16 get_block_index(World_t* world, Block_t* block);
bool setup_default_room(World_t* world);
void reset_tilemap_light(World_t* world);

bool block_in_height_range_of_player(Block_t* block, Position_t player);

void get_block_stack(World_t* world, Block_t* block, BlockList_t* block_list, S8 rotations_through_portal);
S16 get_block_stack_mass(World_t* world, Block_t* block);
S16 get_block_mass_in_direction(World_t* world, Block_t* block, Direction_t direction, bool require_on_ice = true);

F32 momentum_term(F32 mass, F32 vel);
F32 momentum_term(TransferMomentum_t* transfer_momentum);
TransferMomentum_t get_block_momentum(World_t* world, Block_t* block, Direction_t direction, bool prev = false);
ElasticCollisionResult_t elastic_transfer_momentum(F32 mass_1, F32 vel_i_1, F32 mass_2, F32 vel_i_2);
ElasticCollisionResult_t elastic_transfer_momentum_to_block(TransferMomentum_t* transfer_momentum, World_t* world, Block_t* block, Direction_t direction, S16 block_contributing_momentum_to_total_blocks = 1);

F32 get_block_static_friction(S16 mass);
F32 get_block_expected_player_push_velocity(World_t* world, Block_t* block, F32 force = 1.0f);
Vec_t get_block_momentum_vel(World_t* world, Block_t* block);

AllowedToPushResult_t allowed_to_push(World_t* world, Block_t* block, Direction_t direction, F32 force = 1.0f, TransferMomentum_t* instant_momentum = NULL);
PushFromEntangler_t build_push_from_entangler(Block_t* block, Direction_t push_dir, F32 force);

Pixel_t block_pos_in_solid_boundary(Position_t pos, BlockCut_t cut, Direction_t horizontal_direction, Direction_t vertical_direction, World_t* world);
void set_against_blocks_coasting_from_player(Block_t* block, Direction_t direction, World_t* world);
bool find_and_update_connected_teleported_block(Block_t* block, Direction_t direction, World_t* world);

PlayerInBlockRectResult_t player_in_block_rect(Player_t* player, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt);
