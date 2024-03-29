#pragma once

#include "tile.h"
#include "block.h"
#include "interactive.h"
#include "object_array.h"
#include "player.h"

struct UndoBlock_t{
     Pixel_t pixel;
     Vec_t decimal;
     S8 z;
     Element_t element;
     Vec_t accel;
     Vec_t vel;
     S16 entangle_index;
     S8 rotation;
     Move_t horizontal_move;
     Move_t vertical_move;
     BlockCut_t cut;
};

struct UndoPlayer_t{
     Pixel_t pixel;
     Vec_t decimal;
     S8 z;
     Direction_t face;
     U8 rotation;
};

struct UndoMapResize_t{
     bool horizontal;
     S16 old_dimension;
     S16 new_dimension;
};

enum UndoDiffType_t : U8{
     UNDO_DIFF_TYPE_PLAYER,
     UNDO_DIFF_TYPE_TILE_FLAGS,
     UNDO_DIFF_TYPE_BLOCK,
     UNDO_DIFF_TYPE_INTERACTIVE,
     UNDO_DIFF_TYPE_BLOCK_INSERT,
     UNDO_DIFF_TYPE_BLOCK_REMOVE,
     UNDO_DIFF_TYPE_INTERACTIVE_INSERT,
     UNDO_DIFF_TYPE_INTERACTIVE_REMOVE,
     UNDO_DIFF_TYPE_PLAYER_INSERT,
     UNDO_DIFF_TYPE_PLAYER_REMOVE,
     UNDO_DIFF_TYPE_MAP_RESIZE,
};

struct UndoDiffHeader_t{
     S32 index;
     UndoDiffType_t type;
};

struct UndoHistory_t{
     U32 size;
     void* start;
     void* current;
};

struct Undo_t{
     S16 width;
     S16 height;
     U16** tile_flags;
     ObjectArray_t<UndoBlock_t> blocks;
     ObjectArray_t<Interactive_t> interactives;
     ObjectArray_t<UndoPlayer_t> players;

     UndoHistory_t history;

     bool snapshot_from_revert = false;
};

bool init(UndoHistory_t* undo_history, U32 history_size);
void destroy(UndoHistory_t* undo_history);
void undo_history_add(UndoHistory_t* undo_history, UndoDiffType_t type, S32 index);
bool init(Undo_t* undo, U32 history_size, S16 map_width, S16 map_height, S16 block_count, S16 interactive_count);
bool undo_resize_width(Undo_t* undo, S16 new_width);
bool undo_resize_height(Undo_t* undo, S16 new_height);
void destroy(Undo_t* undo);
void undo_snapshot(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                   ObjectArray_t<Interactive_t>* interactives);
void undo_commit(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                 ObjectArray_t<Interactive_t>* interactives, bool ignore_moving_stuff = false);
void undo_revert(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                 ObjectArray_t<Interactive_t>* interactives, bool has_bow);
bool undo_revert_would_move_player_to_a_different_room(Undo_t* undo, Position_t player_pos, Direction_t player_face,
                                                       ObjectArray_t<Rect_t>* rooms);
