#include "demo.h"

void demo_entry_get(DemoEntry_t* demo_entry, FILE* file){
     size_t read_count = fread(demo_entry, sizeof(*demo_entry), 1, file);
     if(read_count != 1) demo_entry->frame = (S64)(-1);
}

void player_action_perform(PlayerAction_t* player_action, Player_t* player, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count){
     switch(player_action_type){
     default:
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_START:
          if(player_action->move_left) return;
          player_action->move_left = true;
          player->face = DIRECTION_LEFT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_LEFT, player_action->move_left_rotation)){
               player_action->reface = true;
          }
          player_action->move_left = false;
          player_action->move_left_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_START:
          if(player_action->move_up) return;
          player_action->move_up = true;
          player->face = DIRECTION_UP;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_UP, player_action->move_up_rotation)){
               player_action->reface = true;
          }
          player_action->move_up = false;
          player_action->move_up_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_START:
          if(player_action->move_right) return;
          player_action->move_right = true;
          player->face = DIRECTION_RIGHT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP:
     {
          if(player->face == direction_rotate_clockwise(DIRECTION_RIGHT, player_action->move_right_rotation)){
               player_action->reface = true;
          }
          player_action->move_right = false;
          player_action->move_right_rotation = 0;
     } break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_START:
          if(player_action->move_down) return;
          player_action->move_down = true;
          player->face = DIRECTION_DOWN;
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_STOP:
          if(player->face == direction_rotate_clockwise(DIRECTION_DOWN, player_action->move_down_rotation)){
               player_action->reface = true;
          }
          player_action->move_down = false;
          player_action->move_down_rotation = 0;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_START:
          if(player_action->activate) return;
          player_action->activate = true;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_STOP:
          player_action->activate = false;
          break;
     case PLAYER_ACTION_TYPE_SHOOT_START:
          if(player_action->shoot) return;
          player_action->shoot = true;
          break;
     case PLAYER_ACTION_TYPE_SHOOT_STOP:
          player_action->shoot = false;
          break;
     case PLAYER_ACTION_TYPE_UNDO:
          if(player_action->undo) return;
          player_action->undo = true;
          break;
     }

     if(demo_mode == DEMO_MODE_RECORD){
          DemoEntry_t demo_entry {frame_count, player_action_type};
          fwrite(&demo_entry, sizeof(demo_entry), 1, demo_file);
     }
}
