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
};

bool load_map_number(S32 map_number, Coord_t* player_start, World_t* world);
void setup_map(Coord_t player_start, World_t* world, Undo_t* undo);

void activate(World_t* world, Coord_t coord);

MovePlayerThroughWorldResult_t move_player_through_world(Position_t player_pos, Vec_t player_vel, Vec_t player_pos_delta,
                                                         Direction_t player_face, S8 player_clone_id, S16 player_index,
                                                         S16 player_pushing_block, Direction_t player_pushing_block_dir,
                                                         Coord_t* skip_coord, World_t* world);

TeleportPositionResult_t teleport_position_across_portal(Position_t position, Vec_t pos_delta, World_t* world,
                                                         Coord_t premove_coord, Coord_t postmove_coord);

void illuminate(Coord_t coord, U8 value, World_t* world, Coord_t from_portal = Coord_t{-1, -1});

void spread_ice(Coord_t center, S16 radius, World_t* world, bool teleported = false);
void melt_ice(Coord_t center, S16 radius, World_t* world, bool teleported = false);

bool block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 instant_vel = 0);
bool reset_players(ObjectArray_t<Player_t>* players);

void describe_coord(Coord_t coord, World_t* world);
bool test_map_end_state(World_t* world, Demo_t* demo);

S16 get_block_index(World_t* world, Block_t* block);
