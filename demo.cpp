#include "demo.h"

#include <stdlib.h>

struct DemoEntryNode_t{
     DemoEntry_t entry;
     DemoEntryNode_t* next;
};

bool demo_begin(Demo_t* demo){
     switch(demo->mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          demo->file = fopen(demo->filepath, "w");
          if(!demo->file){
               LOG("failed to open demo file: %s\n", demo->filepath);
               return false;
          }
          demo->version = 2;
          fwrite(&demo->version, sizeof(demo->version), 1, demo->file);
          break;
     case DEMO_MODE_PLAY:
          demo->file = fopen(demo->filepath, "r");
          if(!demo->file){
               LOG("failed to open demo file: %s\n", demo->filepath);
               return false;
          }
          fread(&demo->version, sizeof(demo->version), 1, demo->file);
          demo->entries = demo_entries_get(demo->file);
          demo->last_frame = demo->entries.entries[demo->entries.count - 1].frame;
          LOG("playing demo %s: version %d with %ld actions across %ld frames\n", demo->filepath,
              demo->version, demo->entries.count, demo->last_frame);
          break;
     }

     return true;
}

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

bool demo_play_frame(Demo_t* demo, PlayerAction_t* player_action, ObjectArray_t<Player_t>* players, S64 frame_count){
     if(demo->entries.entries[demo->entry_index].player_action_type == PLAYER_ACTION_TYPE_END_DEMO){
          if(frame_count > demo->entries.entries[demo->entry_index].frame){
               return true;
          }
     }else{
          while(frame_count == demo->entries.entries[demo->entry_index].frame){
               player_action_perform(player_action, players,
                                     demo->entries.entries[demo->entry_index].player_action_type, demo->mode,
                                     demo->file, frame_count);
               demo->entry_index++;
          }
     }

     return false;
}

void player_action_perform(PlayerAction_t* player_action, ObjectArray_t<Player_t>* players, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count){
     switch(player_action_type){
     default:
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_START:
          if(player_action->move[DIRECTION_LEFT]) return;
          player_action->move[DIRECTION_LEFT] = true;
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               player->face = direction_rotate_clockwise(DIRECTION_LEFT, player->rotation);
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_STOP:
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               if(player->face == direction_rotate_clockwise(DIRECTION_LEFT, player->move_rotation[DIRECTION_LEFT])){
                    player->reface = true;
               }

               player->move_rotation[DIRECTION_LEFT] = 0;
          }
          player_action->move[DIRECTION_LEFT] = false;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_START:
          if(player_action->move[DIRECTION_UP]) return;
          player_action->move[DIRECTION_UP] = true;
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               player->face = direction_rotate_clockwise(DIRECTION_UP, player->rotation);
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_STOP:
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               if(player->face == direction_rotate_clockwise(DIRECTION_UP, player->move_rotation[DIRECTION_UP])){
                    player->reface = true;
               }

               player->move_rotation[DIRECTION_UP] = 0;
          }
          player_action->move[DIRECTION_UP] = false;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_START:
          if(player_action->move[DIRECTION_RIGHT]) return;
          player_action->move[DIRECTION_RIGHT] = true;
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               player->face = direction_rotate_clockwise(DIRECTION_RIGHT, player->rotation);
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP:
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               if(player->face == direction_rotate_clockwise(DIRECTION_RIGHT, player->move_rotation[DIRECTION_RIGHT])){
                    player->reface = true;
               }

               player->move_rotation[DIRECTION_RIGHT] = 0;
          }
          player_action->move[DIRECTION_RIGHT] = false;
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_START:
          if(player_action->move[DIRECTION_DOWN]) return;
          player_action->move[DIRECTION_DOWN] = true;
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               player->face = direction_rotate_clockwise(DIRECTION_DOWN, player->rotation);
          }
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_STOP:
          for(S16 i = 0; i < players->count; i++){
               Player_t* player = players->elements + i;
               if(player->face == direction_rotate_clockwise(DIRECTION_DOWN, player->move_rotation[DIRECTION_DOWN])){
                    player->reface = true;
               }

               player->move_rotation[DIRECTION_DOWN] = 0;
          }
          player_action->move[DIRECTION_DOWN] = false;
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
