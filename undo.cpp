#include "undo.h"
#include "log.h"
#include "defines.h"
#include "conversion.h"

#include <assert.h>
#include <string.h>

bool init(UndoHistory_t* undo_history, U32 history_size){
     undo_history->start = malloc(history_size);
     if(!undo_history->start) return false;
     undo_history->current = undo_history->start;
     undo_history->size = history_size;
     return true;
}

void destroy(UndoHistory_t* undo_history){
     free(undo_history->start);
     memset(undo_history, 0, sizeof(*undo_history));
}

#define ASSERT_BELOW_HISTORY_SIZE(history) assert(((char*)(history->current) - (char*)(history->start)) < history->size)

void undo_history_add(UndoHistory_t* undo_history, UndoDiffType_t type, S32 index){
     switch(type){
     default:
          assert(!"unsupported diff type");
          return;
     case UNDO_DIFF_TYPE_PLAYER:
     case UNDO_DIFF_TYPE_PLAYER_INSERT:
          undo_history->current = (char*)(undo_history->current) + sizeof(UndoPlayer_t);
          break;
     case UNDO_DIFF_TYPE_TILE_FLAGS:
          undo_history->current = (char*)(undo_history->current) + sizeof(U16);
          break;
     case UNDO_DIFF_TYPE_BLOCK:
     case UNDO_DIFF_TYPE_BLOCK_INSERT:
          undo_history->current = (char*)(undo_history->current) + sizeof(UndoBlock_t);
          break;
     case UNDO_DIFF_TYPE_INTERACTIVE:
     case UNDO_DIFF_TYPE_INTERACTIVE_INSERT:
          undo_history->current = (char*)(undo_history->current) + sizeof(Interactive_t);
          break;
     case UNDO_DIFF_TYPE_PLAYER_REMOVE:
     case UNDO_DIFF_TYPE_BLOCK_REMOVE:
     case UNDO_DIFF_TYPE_INTERACTIVE_REMOVE:
          // NOTE: no need to add any more data
          break;
     case UNDO_DIFF_TYPE_MAP_RESIZE:
          undo_history->current = (char*)(undo_history->current) + sizeof(UndoMapResize_t);
          break;
     }

     ASSERT_BELOW_HISTORY_SIZE(undo_history);

     auto* undo_header = (UndoDiffHeader_t*)(undo_history->current);
     undo_header->type = type;
     undo_header->index = index;
     undo_history->current = (char*)(undo_history->current) + sizeof(*undo_header);

     ASSERT_BELOW_HISTORY_SIZE(undo_history);
}

bool init(Undo_t* undo, U32 history_size, S16 map_width, S16 map_height, S16 block_count, S16 interactive_count){
     undo->tile_flags = (U16**)calloc((size_t)(map_height), sizeof(*undo->tile_flags));
     if(!undo->tile_flags) return false;
     for(S16 i = 0; i < map_height; i++){
          undo->tile_flags[i] = (U16*)calloc((size_t)(map_width), sizeof(*undo->tile_flags[i]));
          if(!undo->tile_flags[i]) return false;
     }
     undo->width = map_width;
     undo->height = map_height;

     if(!init(&undo->players, 1)) return false;
     if(!init(&undo->blocks, block_count)) return false;
     if(!init(&undo->interactives, interactive_count)) return false;
     if(!init(&undo->history, history_size)) return false;

     return true;
}

bool undo_resize_width(Undo_t* undo, S16 new_width){
     // loop over each row and resize it
     for(S16 i = 0; i < undo->height; i++){
          undo->tile_flags[i] = (U16*)realloc(undo->tile_flags[i], (size_t)(new_width) * sizeof(*undo->tile_flags[i]));
          if(!undo->tile_flags[i]){
               LOG("%s(): failed to realloc() tile flags for a map resize of %d\n", __FUNCTION__, new_width);
               return false;
          }
     }

     // if we grew, initialize the new elements to 0 (does realloc() already do this?)
     if(undo->width < new_width){
          for(S16 h = 0; h < undo->height; h++){
               for(S16 t = undo->width; t < new_width; t++){
                    undo->tile_flags[h][t] = 0;
               }
          }
     }
     undo->width = new_width;
     return true;
}

bool undo_resize_height(Undo_t* undo, S16 new_height){
     undo->tile_flags = (U16**)realloc(undo->tile_flags, (size_t)(new_height) * sizeof(*undo->tile_flags));
     if(!undo->tile_flags){
          LOG("%s(): failed to realloc() tile flags for a map resize of %d\n", __FUNCTION__, new_height);
          return false;
     }

     // if we grew, initialize the new rows to empty rows
     if(undo->height < new_height){
          for(S16 i = undo->height; i < new_height; i++){
               undo->tile_flags[i] = (U16*)calloc((size_t)(undo->width), sizeof(*undo->tile_flags[i]));
               if(!undo->tile_flags[i]){
                    LOG("%s(): failed to calloc() new tile flag rows for a map resize of %d\n", __FUNCTION__, new_height);
                    return false;
               }
          }
     }
     undo->height = new_height;
     return true;
}

void destroy(Undo_t* undo){
     for(int i = 0; i < undo->height; i++){
          free(undo->tile_flags[i]);
     }
     free(undo->tile_flags);
     undo->tile_flags = nullptr;
     undo->width = 0;
     undo->height = 0;
     destroy(&undo->players);
     destroy(&undo->blocks);
     destroy(&undo->interactives);
     destroy(&undo->history);
}

void undo_snapshot(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                   ObjectArray_t<Interactive_t>* interactives){
     if(undo->players.count != players->count){
          resize(&undo->players, players->count);
     }

     for(S16 i = 0; i < players->count; i++){
          UndoPlayer_t* undo_player = undo->players.elements + i;
          Player_t* player = players->elements + i;
          undo_player->pixel = player->pos.pixel;
          undo_player->decimal = player->pos.decimal;
          undo_player->z = player->pos.z;
          undo_player->face = player->face;
          undo_player->rotation = player->rotation;
     }

     for(S16 y = 0; y < tilemap->height; y++){
          for(S16 x = 0; x < tilemap->width; x++){
               undo->tile_flags[y][x] = tilemap->tiles[y][x].flags;
          }
     }

     if(undo->blocks.count != blocks->count){
          resize(&undo->blocks, blocks->count);
     }

     for(S16 i = 0; i < blocks->count; i++){
          UndoBlock_t* undo_block = undo->blocks.elements + i;
          Block_t* block = blocks->elements + i;
          undo_block->pixel = block->pos.pixel;
          undo_block->decimal = block->pos.decimal;
          undo_block->z = block->pos.z;
          undo_block->element = block->element;
          undo_block->accel = block->accel;
          undo_block->vel = block->vel;
          undo_block->entangle_index = block->entangle_index;
          undo_block->rotation = block->rotation;
          undo_block->horizontal_move = block->horizontal_move;
          undo_block->vertical_move = block->vertical_move;
          undo_block->cut = block->cut;
     }

     if(undo->interactives.count != interactives->count){
          resize(&undo->interactives, interactives->count);
     }

     for(S16 i = 0; i < interactives->count; i++){
          undo->interactives.elements[i] = interactives->elements[i];
     }
}

void undo_commit(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                 ObjectArray_t<Interactive_t>* interactives, bool ignore_moving_stuff){
     U32 diff_count = 0;

     // don't save undo if any blocks are moving, or doors are opening
     if(!ignore_moving_stuff){
          for(S16 i = 0; i < blocks->count; i++){
               Block_t* block = blocks->elements + i;
               if(block->vel.x != 0.0f || block->vel.y != 0.0f ||
                  block->horizontal_move.state == MOVE_STATE_STARTING ||
                  block->vertical_move.state == MOVE_STATE_STARTING) return;
          }

          for(S16 i = 0; i < interactives->count; i++){
               auto* interactive = interactives->elements + i;
               if(interactive->type == INTERACTIVE_TYPE_DOOR){
                    if(interactive->door.lift.up){
                         if(interactive->door.lift.ticks < DOOR_MAX_HEIGHT) return;
                    }else{
                         if(interactive->door.lift.ticks > 0) return;
                    }
               }else if(interactive->type == INTERACTIVE_TYPE_POPUP){
                    if(interactive->popup.lift.up){
                         if(interactive->popup.lift.ticks < (HEIGHT_INTERVAL + 1)) return;
                    }else{
                         if(interactive->popup.lift.ticks > 1) return;
                    }
               }
          }
     }

     S16 old_undo_width = undo->width;
     S16 old_undo_height = undo->height;

     if(old_undo_width != tilemap->width){
          undo_resize_width(undo, tilemap->width);
     }

     if(old_undo_height != tilemap->height){
          undo_resize_height(undo, tilemap->height);
     }

     // TODO: compress this, interactives, and blocks doin the same damn thing
     // check for differences between player count in previous state and new state
     S16 min_player_count = players->count;
     if(players->count > undo->players.count){
          min_player_count = undo->players.count;
          // remove a new players
          for(S16 i = undo->players.count; i < players->count; i++){
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_PLAYER_REMOVE, i);
               diff_count++;
          }
     }else if(players->count < undo->players.count){
          // insert a new players
          S16 diff = undo->players.count - players->count;
          for(S16 d = 0; d < diff; d++){
               S16 last_index = (undo->players.count - (S16)(1)) - d;
               UndoPlayer_t* last_player = undo->players.elements + last_index;
               bool found = false;

               for(S16 i = 0; i < players->count; i++){
                    Player_t* player = players->elements + i;

                    if(last_player->pixel == player->pos.pixel &&
                       last_player->z == player->pos.z){
                         found = true;
                         auto* undo_player_entry = (UndoPlayer_t*)(undo->history.current);
                         *undo_player_entry = undo->players.elements[i];
                         undo_history_add(&undo->history, UNDO_DIFF_TYPE_PLAYER_INSERT, i);
                         diff_count++;
                         break;
                    }
               }

               // it must have been that last element
               if(!found){
                    auto* undo_player_entry = (UndoPlayer_t*)(undo->history.current);
                    *undo_player_entry = *last_player;
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_PLAYER_INSERT, last_index);
                    diff_count++;
               }
          }
     }

     for(S16 i = 0; i < min_player_count; i++){
          UndoPlayer_t* undo_player = undo->players.elements + i;
          Player_t* player = players->elements + i;
          if(player->pos.pixel != undo_player->pixel ||
             player->pos.z != undo_player->z ||
             player->face != undo_player->face){
               auto* diff_undo_player = (UndoPlayer_t*)(undo->history.current);
               *diff_undo_player = *undo_player;
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_PLAYER, i);
               diff_count++;
          }
     }

     // tile flags
     for(S16 y = 0; y < tilemap->height; y++){
          for(S16 x = 0; x < tilemap->width; x++){
               if(undo->tile_flags[y][x] != tilemap->tiles[y][x].flags){
                    auto* undo_tile_flags = (U16*)(undo->history.current);
                    *undo_tile_flags = undo->tile_flags[y][x];
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_TILE_FLAGS, y * tilemap->width + x);
                    diff_count++;
               }
          }
     }

     // TODO: save diff for tiles that we remove when we shrink the map. this is tricky because we've already resized
     //       undo at this point, maybe we'll need to rethink that

     // check for differences between block count in previous state and new state
     S16 min_block_count = blocks->count;
     if(blocks->count > undo->blocks.count){
          min_block_count = undo->blocks.count;
          // remove a new blocks
          for(S16 i = undo->blocks.count; i < blocks->count; i++){
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK_REMOVE, i);
               diff_count++;
          }
     }else if(blocks->count < undo->blocks.count){
          // insert a new blocks
          S16 diff = undo->blocks.count - blocks->count;
          for(S16 d = 0; d < diff; d++){
               S16 last_index = (undo->blocks.count - (S16)(1)) - d;
               UndoBlock_t* last_block = undo->blocks.elements + last_index;
               bool found = false;

               for(S16 i = 0; i < blocks->count; i++){
                    Block_t* block = blocks->elements + i;

                    if(last_block->pixel == block->pos.pixel &&
                       last_block->decimal == block->pos.decimal &&
                       last_block->z == block->pos.z &&
                       last_block->element == block->element &&
                       last_block->entangle_index == block->entangle_index &&
                       last_block->horizontal_move == block->horizontal_move &&
                       last_block->vertical_move == block->vertical_move &&
                       last_block->cut == block->cut){
                         found = true;
                         auto* undo_block_entry = (UndoBlock_t*)(undo->history.current);
                         *undo_block_entry = undo->blocks.elements[i];
                         undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK_INSERT, i);
                         diff_count++;
                         break;
                    }
               }

               // it must have been that last element
               if(!found){
                    auto* undo_block_entry = (UndoBlock_t*)(undo->history.current);
                    *undo_block_entry = *last_block;
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK_INSERT, last_index);
                    diff_count++;
               }
          }
     }

     // blocks
     for(S16 i = 0; i < min_block_count; i++){
          UndoBlock_t* undo_block = undo->blocks.elements + i;
          Block_t* block = blocks->elements + i;

          if(undo_block->pixel != block->pos.pixel ||
             undo_block->decimal != block->pos.decimal ||
             undo_block->z != block->pos.z ||
             undo_block->element != block->element ||
             undo_block->entangle_index != block->entangle_index ||
             undo_block->horizontal_move != block->horizontal_move ||
             undo_block->vertical_move != block->vertical_move ||
             undo_block->cut != block->cut){
               auto* undo_block_entry = (UndoBlock_t*)(undo->history.current);
               *undo_block_entry = *undo_block;
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK, i);
               diff_count++;
          }
     }

     // check for differences between interactive count in previous state and new state
     S16 min_interactive_count = interactives->count;
     if(interactives->count > undo->interactives.count){
          min_interactive_count = undo->interactives.count;

          // remove a new interactive
          for(S16 i = undo->interactives.count; i < interactives->count; i++){
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE_REMOVE, i);
               diff_count++;
          }
     }else if(interactives->count < undo->interactives.count){
          // insert a new interactives
          S16 diff = undo->interactives.count - interactives->count;
          for(S16 d = 0; d < diff; d++){
               S16 last_index = (undo->interactives.count - (S16)(1)) - d;
               Interactive_t* last_interactive = undo->interactives.elements + last_index;
               bool found = false;

               for(S16 i = 0; i < interactives->count; i++){
                    Interactive_t* interactive = interactives->elements + i;

                    if(interactive_equal(last_interactive, interactive)){
                         found = true;
                         auto* undo_interactive_entry = (Interactive_t*)(undo->history.current);
                         *undo_interactive_entry = undo->interactives.elements[i];
                         undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE_INSERT, i);
                         diff_count++;
                         break;
                    }
               }

               // it must have been that last element
               if(!found){
                    auto* undo_interactive_entry = (Interactive_t*)(undo->history.current);
                    *undo_interactive_entry = *last_interactive;
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE_INSERT, last_index);
                    diff_count++;
               }
          }
     }

     // interactives
     for(S16 i = 0; i < min_interactive_count; i++){
          Interactive_t* undo_interactive = undo->interactives.elements + i;
          Interactive_t* interactive = interactives->elements + i;

          if(!interactive_equal(undo_interactive, interactive)){
               auto* undo_interactive_entry = (Interactive_t*)(undo->history.current);
               *undo_interactive_entry = *undo_interactive;
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE, i);
               diff_count++;
          }
     }

     // save the map resize diffs for last because they need to be the first to run if we revert
     if(old_undo_width != tilemap->width){
          auto* undo_map_resize_entry = (UndoMapResize_t*)(undo->history.current);
          undo_map_resize_entry->horizontal = true;
          undo_map_resize_entry->old_dimension = old_undo_width;
          undo_map_resize_entry->new_dimension = tilemap->width;

          undo_history_add(&undo->history, UNDO_DIFF_TYPE_MAP_RESIZE, 0);
          diff_count++;
     }

     if(old_undo_height != tilemap->height){
          auto* undo_map_resize_entry = (UndoMapResize_t*)(undo->history.current);
          undo_map_resize_entry->horizontal = false;
          undo_map_resize_entry->old_dimension = old_undo_height;
          undo_map_resize_entry->new_dimension = tilemap->height;

          undo_history_add(&undo->history, UNDO_DIFF_TYPE_MAP_RESIZE, 0);
          diff_count++;
     }

     // finally, write number of diffs
     if(diff_count){
          auto* count_entry = (S32*)(undo->history.current);
          *count_entry = diff_count;
          undo->history.current = (char*)(undo->history.current) + sizeof(S32);
          ASSERT_BELOW_HISTORY_SIZE((&undo->history));

          undo_snapshot(undo, players, tilemap, blocks, interactives);
     }
}

void undo_revert(Undo_t* undo, ObjectArray_t<Player_t>* players, TileMap_t* tilemap, ObjectArray_t<Block_t>* blocks,
                 ObjectArray_t<Interactive_t>* interactives, bool has_bow){
     if(undo->history.current <= undo->history.start) return;

     auto* ptr = (char*)(undo->history.current);
     S32 diff_count = 0;
     ptr -= sizeof(diff_count);
     diff_count = *(S32*)(ptr);

     for(S32 i = 0; i < diff_count; i++){
          ptr -= sizeof(UndoDiffHeader_t);
          auto* diff_header = (UndoDiffHeader_t*)(ptr);

          switch(diff_header->type){
          default:
               assert(!"memory probably corrupted, or new unsupported diff type");
               return;
          case UNDO_DIFF_TYPE_PLAYER:
          {
               ptr -= sizeof(UndoPlayer_t);
               auto* player_entry = (UndoPlayer_t*)(ptr);
               Player_t* player = players->elements + diff_header->index;
               *player = {};
               // TODO fix these numbers as they are important
               player->walk_frame_delta = 1;
               player->pos.pixel = player_entry->pixel;
               player->pos.decimal = player_entry->decimal;
               player->pos.z = player_entry->z;
               player->face = player_entry->face;
               player->has_bow = has_bow;
               player->rotation = player_entry->rotation;
          } break;
          case UNDO_DIFF_TYPE_TILE_FLAGS:
          {
               int x = diff_header->index % tilemap->width;
               int y = diff_header->index / tilemap->width;

               ptr -= sizeof(U16);
               auto* tile_flags_entry = (U16*)(ptr);
               tilemap->tiles[y][x].flags = *tile_flags_entry;
          } break;
          case UNDO_DIFF_TYPE_BLOCK:
          {
               ptr -= sizeof(UndoBlock_t);
               auto* block_entry = (UndoBlock_t*)(ptr);
               Block_t* block = blocks->elements + diff_header->index;
               *block = {};
               block->pos.pixel = block_entry->pixel;
               block->pos.decimal = block_entry->decimal;
               block->pos.z = block_entry->z;
               block->element = block_entry->element;
               block->accel = block_entry->accel;
               block->vel = block_entry->vel;
               block->entangle_index = block_entry->entangle_index;
               block->rotation = block_entry->rotation;
               block->horizontal_move = block_entry->horizontal_move;
               block->vertical_move = block_entry->vertical_move;
               block->cut = block_entry->cut;
          } break;
          case UNDO_DIFF_TYPE_BLOCK_INSERT:
          {
               ptr -= sizeof(UndoBlock_t);
               auto* block_entry = (UndoBlock_t*)(ptr);
               S16 last_index = blocks->count;
               resize(blocks, blocks->count + (S16)(1));

               // move the block at that index back to the end of the list
               blocks->elements[last_index] = blocks->elements[diff_header->index];

               // override it with the insert
               Block_t* block = blocks->elements + diff_header->index;
               *block = {};
               block->pos.pixel = block_entry->pixel;
               block->pos.decimal = vec_zero();
               block->pos.z = block_entry->z;
               block->element = block_entry->element;
               block->accel = block_entry->accel;
               block->vel = block_entry->vel;
               block->entangle_index = block_entry->entangle_index;
               block->rotation = block_entry->rotation;
               block->horizontal_move = block_entry->horizontal_move;
               block->vertical_move = block_entry->vertical_move;
               block->cut = block_entry->cut;
          } break;
          case UNDO_DIFF_TYPE_BLOCK_REMOVE:
          {
               remove(blocks, (S16)(diff_header->index));
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE:
          {
               ptr -= sizeof(Interactive_t);
               auto* interactive_entry = (Interactive_t*)(ptr);
               interactives->elements[(S16)(diff_header->index)] = *interactive_entry;
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE_INSERT:
          {
               ptr -= sizeof(Interactive_t);
               auto* interactive_entry = (Interactive_t*)(ptr);
               S16 last_index = interactives->count;
               resize(interactives, interactives->count + (S16)(1));
               interactives->elements[last_index] = interactives->elements[diff_header->index];
               interactives->elements[diff_header->index] = *interactive_entry;
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE_REMOVE:
          {
               remove(interactives, (S16)(diff_header->index));
          } break;
          case UNDO_DIFF_TYPE_PLAYER_INSERT:
          {
               ptr -= sizeof(Player_t);
               auto* player_entry = (Player_t*)(ptr);
               S16 last_index = players->count;
               resize(players, players->count + (S16)(1));
               players->elements[last_index] = players->elements[diff_header->index];
               players->elements[diff_header->index] = *player_entry;
          } break;
          case UNDO_DIFF_TYPE_PLAYER_REMOVE:
          {
               remove(players, (S16)(diff_header->index));
          } break;
          case UNDO_DIFF_TYPE_MAP_RESIZE:
          {
               ptr -= sizeof(UndoMapResize_t);
               auto* map_resize = (UndoMapResize_t*)(ptr);

               // map_resize->old_dimension;
               // map_resize->new_dimension;

               TileMap_t map_copy = {};
               deep_copy(tilemap, &map_copy);
               destroy(tilemap);

               if(map_resize->horizontal){
                    init(tilemap, map_resize->old_dimension, map_copy.height);

                    S16 lower_width = map_copy.width > map_resize->old_dimension ? map_resize->old_dimension : map_copy.width;

                    for(S16 h = 0; h < map_copy.height; h++){
                         for(S16 w = 0; w < lower_width; w++){
                              tilemap->tiles[h][w] = map_copy.tiles[h][w];
                         }
                    }

                    destroy(&map_copy);
                    undo_resize_width(undo, map_resize->old_dimension);
               }else{
                    init(tilemap, map_copy.width, map_resize->old_dimension);

                    S16 lower_height = map_copy.height > map_resize->old_dimension ? map_resize->old_dimension : map_copy.height;

                    for(S16 h = 0; h < lower_height; h++){
                         for(S16 w = 0; w < map_copy.width; w++){
                              tilemap->tiles[h][w] = map_copy.tiles[h][w];
                         }
                    }

                    destroy(&map_copy);
                    undo_resize_height(undo, map_resize->old_dimension);
               }
          } break;
          }
     }

     undo->history.current = ptr;
     undo_snapshot(undo, players, tilemap, blocks, interactives);
}

bool undo_revert_would_move_player_to_a_different_room(Undo_t* undo, Position_t player_pos, Direction_t player_face,
                                                       ObjectArray_t<Rect_t>* rooms){
     if(undo->history.current <= undo->history.start) return false;

     auto player_coord = pos_to_coord(player_pos);
     Rect_t* undo_player_room = nullptr;
     Rect_t* current_player_room = nullptr;
     for(S16 i = 0; i < rooms->count; i++){
          auto* room = rooms->elements + i;
          if(coord_in_rect(player_coord, *room)){
               current_player_room = room;
               break;
          }
     }

     // If the player has not moved and is chaining undos, check the diff chain to see if we will end up in a different
     // room.
     if(player_pos.pixel == undo->players.elements[0].pixel &&
        player_pos.z == undo->players.elements[0].z &&
        player_face == undo->players.elements[0].face){
          auto* ptr = (char*)(undo->history.current);
          S32 diff_count = 0;
          ptr -= sizeof(diff_count);
          diff_count = *(S32*)(ptr);

          for(S32 i = 0; i < diff_count; i++){
               ptr -= sizeof(UndoDiffHeader_t);
               auto* diff_header = (UndoDiffHeader_t*)(ptr);

               switch(diff_header->type){
               default:
                    assert(!"memory probably corrupted, or new unsupported diff type");
                    return false;
               case UNDO_DIFF_TYPE_PLAYER:
               {
                    ptr -= sizeof(UndoPlayer_t);
                    auto* player_entry = (UndoPlayer_t*)(ptr);
                    auto undo_coord = pixel_to_coord(player_entry->pixel);

                    for(S16 r = 0; r < rooms->count; r++){
                         auto* room = rooms->elements + r;
                         if(coord_in_rect(undo_coord, *room)){
                              undo_player_room = room;
                              break;
                         }
                    }
                    break;
               }
               case UNDO_DIFF_TYPE_TILE_FLAGS:
                    ptr -= sizeof(U16);
                    break;
               case UNDO_DIFF_TYPE_BLOCK:
                    ptr -= sizeof(UndoBlock_t);
                    break;
               case UNDO_DIFF_TYPE_BLOCK_INSERT:
                    ptr -= sizeof(UndoBlock_t);
                    break;
               case UNDO_DIFF_TYPE_BLOCK_REMOVE:
                    break;
               case UNDO_DIFF_TYPE_INTERACTIVE:
                    ptr -= sizeof(Interactive_t);
                    break;
               case UNDO_DIFF_TYPE_INTERACTIVE_INSERT:
                    ptr -= sizeof(Interactive_t);
                    break;
               case UNDO_DIFF_TYPE_INTERACTIVE_REMOVE:
                    break;
               case UNDO_DIFF_TYPE_PLAYER_INSERT:
                    ptr -= sizeof(Player_t);
                    break;
               case UNDO_DIFF_TYPE_PLAYER_REMOVE:
                    break;
               case UNDO_DIFF_TYPE_MAP_RESIZE:
                    ptr -= sizeof(UndoMapResize_t);
                    break;
               }
          }

          return current_player_room != undo_player_room;
     }

     // otherwise just check against the snapshot
     auto undo_player_coord = pixel_to_coord(undo->players.elements[0].pixel);
     for(S16 i = 0; i < rooms->count; i++){
          auto* room = rooms->elements + i;
          if(coord_in_rect(undo_player_coord, *room)){
               undo_player_room = room;
               break;
          }
     }

     return undo_player_room != current_player_room;
}
