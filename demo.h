#pragma once

#include "world.h"

#include <stdio.h>

enum PlayerActionType_t{
     PLAYER_ACTION_TYPE_MOVE_LEFT_START,
     PLAYER_ACTION_TYPE_MOVE_LEFT_STOP,
     PLAYER_ACTION_TYPE_MOVE_UP_START,
     PLAYER_ACTION_TYPE_MOVE_UP_STOP,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_START,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP,
     PLAYER_ACTION_TYPE_MOVE_DOWN_START,
     PLAYER_ACTION_TYPE_MOVE_DOWN_STOP,
     PLAYER_ACTION_TYPE_ACTIVATE_START,
     PLAYER_ACTION_TYPE_ACTIVATE_STOP,
     PLAYER_ACTION_TYPE_SHOOT_START,
     PLAYER_ACTION_TYPE_SHOOT_STOP,
     PLAYER_ACTION_TYPE_END_DEMO,
     PLAYER_ACTION_TYPE_UNDO,
};

struct PlayerAction_t{
     bool move[DIRECTION_COUNT];
     bool activate;
     bool last_activate;
     bool shoot;
     bool undo;
};

enum DemoMode_t{
     DEMO_MODE_NONE,
     DEMO_MODE_PLAY,
     DEMO_MODE_RECORD,
};

struct DemoEntry_t{
     S64 frame;
     PlayerActionType_t player_action_type;
};

struct DemoEntries_t{
     DemoEntry_t* entries = nullptr;
     S64 count = 0;
};

struct Demo_t{
     DemoMode_t mode = DEMO_MODE_NONE;

     const char* filepath = nullptr;
     FILE* file = nullptr;

     S32 version = 1;
     S64 last_frame = 0;
     S64 entry_index = 0;
     S64 seek_frame = -1;
     F32 dt_scalar = 1.0f;
     bool paused = false;
     DemoEntries_t entries;
};

bool demo_begin(Demo_t* demo);
DemoEntries_t demo_entries_get(FILE* file);
bool demo_play_frame(Demo_t* demo, PlayerAction_t* player_action, ObjectArray_t<Player_t>* players, S64 frame_count,
                     Demo_t* record_demo);

void player_action_perform(PlayerAction_t* player_action, ObjectArray_t<Player_t>* players, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count);

FILE* load_demo_number(S32 map_number, const char** demo_filepath);
void cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                         ObjectArray_t<Interactive_t>* demo_starting_interactives);
void fetch_cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                               ObjectArray_t<Interactive_t>* demo_starting_interactives);
bool load_map_number_demo(Demo_t* demo, S16 map_number, S64* frame_count);

bool test_map_end_state(World_t* world, Demo_t* demo);
