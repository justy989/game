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
     ObjectArray_t<Block_t> block_array = {};
     ObjectArray_t<Interactive_t> interactive_array = {};
     ArrowArray_t arrow_array = {};

     QuadTreeNode_t<Interactive_t>* interactive_quad_tree = nullptr;
     QuadTreeNode_t<Block_t>* block_quad_tree = nullptr;
};

bool load_map_number(S32 map_number, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                     ObjectArray_t<Interactive_t>* interactive_array);
void reset_map(Player_t* player, Coord_t player_start, ObjectArray_t<Interactive_t>* interactive_array,
               QuadTreeNode_t<Interactive_t>** interactive_quad_tree, ArrowArray_t* arrow_array);
void setup_map(Player_t* player, Coord_t player_start, ObjectArray_t<Interactive_t>* interactive_array,
               QuadTreeNode_t<Interactive_t>** interactive_quad_tree, ObjectArray_t<Block_t>* block_array,
               QuadTreeNode_t<Block_t>** block_quad_tree, Undo_t* undo, TileMap_t* tilemap, ArrowArray_t* arrow_array);

void activate(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, Coord_t coord);
void toggle_electricity(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, Coord_t coord,
                        Direction_t direction, bool from_wire, bool activated_by_door);

Vec_t move_player_position_through_world(Position_t position, Vec_t pos_delta, Direction_t player_face, Coord_t* skip_coord,
                                         Player_t* player, TileMap_t* tilemap,
                                         QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array,
                                         Block_t** block_to_push, Direction_t* last_block_pushed_direction,
                                         bool* collided_with_interactive, bool* resetting);
S8 teleport_position_across_portal(Position_t* position, Vec_t* pos_delta, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                   TileMap_t* tilemap, Coord_t premove_coord, Coord_t postmove_coord);

void illuminate_line(Coord_t start, Coord_t end, U8 value, TileMap_t* tilemap,
                     QuadTreeNode_t<Interactive_t>* interactive_quad_tree, QuadTreeNode_t<Block_t>* block_quad_tree,
                     Coord_t from_portal = Coord_t{-1, -1});
void illuminate(Coord_t coord, U8 value, TileMap_t* tilemap,
                QuadTreeNode_t<Interactive_t>* interactive_quad_tree, QuadTreeNode_t<Block_t>* block_quad_tree,
                Coord_t from_portal = Coord_t{-1, -1});

void spread_ice(Coord_t center, S16 radius, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                QuadTreeNode_t<Block_t>* block_quad_tree, bool teleported);
void melt_ice(Coord_t center, S16 radius, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
              QuadTreeNode_t<Block_t>* block_quad_tree, bool teleported);

void describe_coord(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, QuadTreeNode_t<Block_t>* block_quad_tree);
