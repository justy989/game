#include "demo.h"
#include "map_format.h"

#include <stdlib.h>
#include <string.h>

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
          LOG("playing demo %s: version %d with %" PRId64 " actions across %" PRId64 " frames\n", demo->filepath,
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

bool demo_play_frame(Demo_t* demo, PlayerAction_t* player_action, ObjectArray_t<Player_t>* players, S64 frame_count,
                     Demo_t* record_demo){
     if(demo->entries.entries[demo->entry_index].player_action_type == PLAYER_ACTION_TYPE_END_DEMO){
          if(frame_count > demo->entries.entries[demo->entry_index].frame){
               return true;
          }
     }else{
          while(frame_count == demo->entries.entries[demo->entry_index].frame){
               player_action_perform(player_action, players,
                                     demo->entries.entries[demo->entry_index].player_action_type, record_demo->mode,
                                     record_demo->file, frame_count);
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

FILE* load_demo_number(S32 map_number, const char** demo_filepath){
     char filepath[64] = {};
     snprintf(filepath, 64, "content/%03d.bd", map_number);
     *demo_filepath = strdup(filepath);
     return fopen(*demo_filepath, "rb");
}

void cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                         ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(&world->tilemap, demo_starting_tilemap);
     deep_copy(&world->blocks, demo_starting_blocks);
     deep_copy(&world->interactives, demo_starting_interactives);
}

void fetch_cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                               ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(demo_starting_tilemap, &world->tilemap);
     deep_copy(demo_starting_blocks, &world->blocks);
     deep_copy(demo_starting_interactives, &world->interactives);
}

bool load_map_number_demo(Demo_t* demo, S16 map_number, S64* frame_count){
     if(demo->file) fclose(demo->file);
     demo->file = load_demo_number(map_number, &demo->filepath);
     if(!demo->file){
          LOG("missing map %d corresponding demo.\n", map_number);
          return false;
     }

     free(demo->entries.entries);
     demo->entry_index = 0;
     fread(&demo->version, sizeof(demo->version), 1, demo->file);
     demo->entries = demo_entries_get(demo->file);
     *frame_count = 0;
     demo->last_frame = demo->entries.entries[demo->entries.count - 1].frame;
     LOG("testing demo %s: version %d with %" PRId64 " actions across %" PRId64 " frames\n", demo->filepath,
         demo->version, demo->entries.count, demo->last_frame);
     return true;
}

#define LOG_MISMATCH(name, fmt_spec, chk, act)\
     {                                                                                                                     \
          char mismatch_fmt_string[128];                                                                                   \
          snprintf(mismatch_fmt_string, 128, "mismatched '%s' value. demo '%s', actual '%s'\n", name, fmt_spec, fmt_spec); \
          LOG(mismatch_fmt_string, chk, act);                                                                              \
          test_passed = false;                                                                                             \
     }

bool test_map_end_state(World_t* world, Demo_t* demo){
     bool test_passed = true;

     TileMap_t check_tilemap = {};
     ObjectArray_t<Block_t> check_block_array = {};
     ObjectArray_t<Interactive_t> check_interactives = {};
     ObjectArray_t<Rect_t> check_rooms = {};
     ObjectArray_t<Exit_t> check_exits = {};
     Coord_t check_player_start;

#define NAME_LEN 64
     char name[NAME_LEN];

     if(!load_map_from_file(demo->file, &check_player_start, &check_tilemap, &check_block_array, &check_interactives,
                            &check_rooms, &check_exits, demo->filepath)){
          LOG("failed to load map state from end of file\n");
          return false;
     }

     if(check_tilemap.width != world->tilemap.width){
          LOG_MISMATCH("tilemap width", "%d", check_tilemap.width, world->tilemap.width);
     }else if(check_tilemap.height != world->tilemap.height){
          LOG_MISMATCH("tilemap height", "%d", check_tilemap.height, world->tilemap.height);
     }else{
          for(S16 j = 0; j < check_tilemap.height; j++){
               for(S16 i = 0; i < check_tilemap.width; i++){
                    if(check_tilemap.tiles[j][i].flags != world->tilemap.tiles[j][i].flags){
                         snprintf(name, NAME_LEN, "tile %d, %d flags", i, j);
                         LOG_MISMATCH(name, "%d", check_tilemap.tiles[j][i].flags, world->tilemap.tiles[j][i].flags);
                    }
               }
          }
     }

     S16 check_player_pixel_count = 0;
     Pixel_t* check_player_pixels = nullptr;

     switch(demo->version){
     default:
          break;
     case 1:
          check_player_pixel_count = 1;
          check_player_pixels = (Pixel_t*)(malloc(sizeof(*check_player_pixels)));
          break;
     case 2:
          fread(&check_player_pixel_count, sizeof(check_player_pixel_count), 1, demo->file);
          check_player_pixels = (Pixel_t*)(malloc(sizeof(*check_player_pixels) * check_player_pixel_count));
          break;
     }

     for(S16 i = 0; i < check_player_pixel_count; i++){
          fread(check_player_pixels + i, sizeof(check_player_pixels[i]), 1, demo->file);
     }

     for(S16 i = 0; i < world->players.count; i++){
          Player_t* player = world->players.elements + i;

          if(check_player_pixels[i].x != player->pos.pixel.x){
               LOG_MISMATCH("player pixel x", "%d", check_player_pixels[i].x, player->pos.pixel.x);
          }

          if(check_player_pixels[i].y != player->pos.pixel.y){
               LOG_MISMATCH("player pixel y", "%d", check_player_pixels[i].y, player->pos.pixel.y);
          }
     }

     free(check_player_pixels);

     if(check_block_array.count != world->blocks.count){
          LOG_MISMATCH("block count", "%d", check_block_array.count, world->blocks.count);
     }else{
          for(S16 i = 0; i < check_block_array.count; i++){
               // TODO: consider checking other things
               Block_t* check_block = check_block_array.elements + i;
               Block_t* block = world->blocks.elements + i;
               if(check_block->pos.pixel.x != block->pos.pixel.x){
                    snprintf(name, NAME_LEN, "block %d pos x", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.pixel.x, block->pos.pixel.x);
               }

               if(check_block->pos.pixel.y != block->pos.pixel.y){
                    snprintf(name, NAME_LEN, "block %d pos y", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.pixel.y, block->pos.pixel.y);
               }

               if(check_block->pos.z != block->pos.z){
                    snprintf(name, NAME_LEN, "block %d pos z", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.z, block->pos.z);
               }

               if(check_block->element != block->element){
                    snprintf(name, NAME_LEN, "block %d element", i);
                    LOG_MISMATCH(name, "%d", check_block->element, block->element);
               }

               if(check_block->entangle_index != block->entangle_index){
                    snprintf(name, NAME_LEN, "block %d entangle_index", i);
                    LOG_MISMATCH(name, "%d", check_block->entangle_index, block->entangle_index);
               }
          }
     }

     if(check_interactives.count != world->interactives.count){
          LOG_MISMATCH("interactive count", "%d", check_interactives.count, world->interactives.count);
     }else{
          for(S16 i = 0; i < check_interactives.count; i++){
               // TODO: consider checking other things
               Interactive_t* check_interactive = check_interactives.elements + i;
               Interactive_t* interactive = world->interactives.elements + i;

               if(check_interactive->type != interactive->type){
                    LOG_MISMATCH("interactive type", "%d", check_interactive->type, interactive->type);
               }else{
                    switch(check_interactive->type){
                    default:
                         break;
                    case INTERACTIVE_TYPE_PRESSURE_PLATE:
                         if(check_interactive->pressure_plate.down != interactive->pressure_plate.down){
                              snprintf(name, NAME_LEN, "interactive at %d, %d pressure plate down",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->pressure_plate.down,
                                           interactive->pressure_plate.down);
                         }
                         break;
                    case INTERACTIVE_TYPE_ICE_DETECTOR:
                    case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                         if(check_interactive->detector.on != interactive->detector.on){
                              snprintf(name, NAME_LEN, "interactive at %d, %d detector on",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->detector.on,
                                           interactive->detector.on);
                         }
                         break;
                    case INTERACTIVE_TYPE_POPUP:
                         if(check_interactive->popup.iced != interactive->popup.iced){
                              snprintf(name, NAME_LEN, "interactive at %d, %d popup iced",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->popup.iced,
                                           interactive->popup.iced);
                         }
                         if(check_interactive->popup.lift.up != interactive->popup.lift.up){
                              snprintf(name, NAME_LEN, "interactive at %d, %d popup lift up",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->popup.lift.up,
                                           interactive->popup.lift.up);
                         }
                         break;
                    case INTERACTIVE_TYPE_DOOR:
                         if(check_interactive->door.lift.up != interactive->door.lift.up){
                              snprintf(name, NAME_LEN, "interactive at %d, %d door lift up",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->door.lift.up,
                                           interactive->door.lift.up);
                         }
                         break;
                    case INTERACTIVE_TYPE_PORTAL:
                         if(check_interactive->portal.on != interactive->portal.on){
                              snprintf(name, NAME_LEN, "interactive at %d, %d portal on",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->portal.on,
                                           interactive->portal.on);
                         }
                         break;
                    }
               }
          }
     }

     return test_passed;
}
