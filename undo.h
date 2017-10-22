#pragma once

#include "tile.h"
#include "block.h"
#include "interactive.h"
#include "object_array.h"
#include "player.h"

struct UndoBlock_t{
     Pixel_t pixel;
     S8 z;
     Element_t element;
     Vec_t accel;
     Vec_t vel;
};

struct UndoPlayer_t{
     Pixel_t pixel;
     S8 z;
     Direction_t face;
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
     ObjectArray_t<UndoBlock_t> block_array;
     ObjectArray_t<Interactive_t> interactive_array;
     UndoPlayer_t player;

     UndoHistory_t history;
};

bool init(UndoHistory_t* undo_history, U32 history_size);
void undo_history_add(UndoHistory_t* undo_history, UndoDiffType_t type, S32 index);
bool init(Undo_t* undo, U32 history_size, S16 map_width, S16 map_height, S16 block_count, S16 interactive_count);
void destroy(Undo_t* undo);
void undo_snapshot(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                   ObjectArray_t<Interactive_t>* interactive_array);
void undo_commit(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                 ObjectArray_t<Interactive_t>* interactive_array);
void undo_revert(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                 ObjectArray_t<Interactive_t>* interactive_array);