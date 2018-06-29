#pragma once

#include "player.h"

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
     bool move_left;
     bool move_right;
     bool move_up;
     bool move_down;
     bool activate;
     bool last_activate;
     bool shoot;
     bool reface;
     bool undo;
     S8 move_left_rotation;
     S8 move_right_rotation;
     S8 move_up_rotation;
     S8 move_down_rotation;
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

void demo_entry_get(DemoEntry_t* demo_entry, FILE* file);
void player_action_perform(PlayerAction_t* player_action, Player_t* player, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count);
