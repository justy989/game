#include "demo.h"

#include <stdlib.h>

struct DemoEntryNode_t{
     DemoEntry_t entry;
     DemoEntryNode_t* next;
};

DemoEntries_t demo_entries_get(FILE* file){
     S64 entry_count = 0;

     // read into a linked list
     DemoEntryNode_t* head = nullptr;
     DemoEntryNode_t* itr = nullptr;
     DemoEntryNode_t* prev = nullptr;

     while(!itr || itr->entry.player_action_type != PLAYER_ACTION_TYPE_END_DEMO){
          itr = (DemoEntryNode_t*)(malloc(sizeof(*itr)));
          itr->entry.frame = -1;
          itr->entry.player_action_type = PLAYER_ACTION_TYPE_MOVE_LEFT_START;
          itr->next = nullptr;

          size_t read_count = fread(&itr->entry, sizeof(itr->entry), 1, file);
          if(prev){
               prev->next = itr;
          }else{
               head = itr;
          }

          prev = itr;
          entry_count++;

          if(read_count != 1){
               break;
          }
     }

     // allocate array
     DemoEntries_t entries = {};
     entries.entries = (DemoEntry_t*)(malloc(entry_count * sizeof(*entries.entries)));
     if(entries.entries == nullptr) return entries;
     entries.count = entry_count;

     // convert linked list to array
     S64 index = 0;
     itr = head;
     while(itr){
          entries.entries[index] = itr->entry;
          index++;
          DemoEntryNode_t* tmp = itr;
          itr = itr->next;
          free(tmp);
     }

     return entries;
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
