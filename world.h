#pragma once

#include "tile.h"
#include "player.h"
#include "interactive.h"
#include "block.h"
#include "arrow.h"
#include "quad_tree.h"
#include "undo.h"

struct World_t{
     Player_t player;
     TileMap_t tilemap = {};
     ObjectArray_t<Block_t> blocks = {};
     ObjectArray_t<Interactive_t> interactives = {};
     ArrowArray_t arrows = {};

     QuadTreeNode_t<Interactive_t>* interactive_qt = nullptr;
     QuadTreeNode_t<Block_t>* block_qt = nullptr;
};

bool load_map_number(S32 map_number, Coord_t* player_start, World_t* world);
void setup_map(Coord_t player_start, World_t* world, Undo_t* undo);

void activate(World_t* world, Coord_t coord);

Vec_t move_player_position_through_world(Position_t position, Vec_t pos_delta, Direction_t player_face, Coord_t* skip_coord,
                                         Player_t* player, TileMap_t* tilemap,
                                         QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array,
                                         Block_t** block_to_push, Direction_t* last_block_pushed_direction,
                                         bool* collided_with_interactive, bool* resetting);
S8 teleport_position_across_portal(Position_t* position, Vec_t* pos_delta, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                   TileMap_t* tilemap, Coord_t premove_coord, Coord_t postmove_coord);

void illuminate(Coord_t coord, U8 value, World_t* world, Coord_t from_portal = Coord_t{-1, -1});

void spread_ice(Coord_t center, S16 radius, World_t* world, bool teleported = false);
void melt_ice(Coord_t center, S16 radius, World_t* world, bool teleported = false);

void describe_coord(Coord_t coord, World_t* world);
