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

void calculate_world_changes(World_t* world, ObjectArray_t<DiffEntry_t>* diff);
void apply_diff_to_world(World_t* world, ObjectArray_t<DiffEntry_t>* diff);
bool world_rect_has_changed(World_t* world, Rect_t rect);

// need to free this !
char* build_diff_filename_from_map_filename(const char* map_filename);

bool save_diffs_to_file(const char* filepath, const ObjectArray_t<DiffEntry_t>* diff, Coord_t player_start);
bool load_diffs_from_file(const char* filepath, ObjectArray_t<DiffEntry_t>* diff, Coord_t* player_start);
