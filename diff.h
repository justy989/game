#pragma once

#include "world.h"
#include "map_format.h"

enum DiffType_t{
     DIFF_TILE_FLAGS,
     DIFF_INTERACTIVE,
     DIFF_BLOCK,
     DIFF_BLOCK_REMOVED,
     DIFF_BLOCK_ADDED,
};

struct DiffEntry_t{
     DiffType_t type;
     S16 index;
     union {
          S16 tile_flags;
          MapInteractiveV4_t interactive;
          MapBlockV3_t block;
          S16 block_removed_index;
     };
};

void diff_worlds(World_t* world_a, World_t* world_b, ObjectArray_t<DiffEntry_t>* diff);
void apply_diff_to_world(World_t* world, ObjectArray_t<DiffEntry_t>* diff);
