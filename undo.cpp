#include "undo.h"
#include "log.h"

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

#define ASSERT_BELOW_HISTORY_SIZE(history) assert((char*)(history->current) - (char*)(history->start) < history->size)

void undo_history_add(UndoHistory_t* undo_history, UndoDiffType_t type, S32 index){
     switch(type){
     default:
          assert(!"unsupported diff type");
          return;
     case UNDO_DIFF_TYPE_PLAYER:
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
     case UNDO_DIFF_TYPE_BLOCK_REMOVE:
     case UNDO_DIFF_TYPE_INTERACTIVE_REMOVE:
          // NOTE: no need to add any more data
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
     undo->tile_flags = (U16**)calloc(map_height, sizeof(*undo->tile_flags));
     if(!undo->tile_flags) return false;
     for(S16 i = 0; i < map_height; i++){
          undo->tile_flags[i] = (U16*)calloc(map_width, sizeof(*undo->tile_flags[i]));
          if(!undo->tile_flags[i]) return false;
     }
     undo->width = map_width;
     undo->height = map_height;

     if(!init(&undo->block_array, block_count)) return false;
     if(!init(&undo->interactive_array, interactive_count)) return false;
     if(!init(&undo->history, history_size)) return false;

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
     destroy(&undo->block_array);
     destroy(&undo->interactive_array);
     destroy(&undo->history);
}

void undo_snapshot(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                   ObjectArray_t<Interactive_t>* interactive_array){
     undo->player.pixel = player->pos.pixel;
     undo->player.z = player->pos.z;
     undo->player.face = player->face;

     for(S16 y = 0; y < tilemap->height; y++){
          for(S16 x = 0; x < tilemap->width; x++){
               undo->tile_flags[y][x] = tilemap->tiles[y][x].flags;
          }
     }

     if(undo->block_array.count != block_array->count){
          resize(&undo->block_array, block_array->count);
     }

     for(S16 i = 0; i < block_array->count; i++){
          UndoBlock_t* undo_block = undo->block_array.elements + i;
          Block_t* block = block_array->elements + i;
          undo_block->pixel = block->pos.pixel;
          undo_block->z = block->pos.z;
          undo_block->element = block->element;
          undo_block->accel = block->accel;
          undo_block->vel = block->vel;
          undo_block->entangle_index = block->entangle_index;
     }

     if(undo->interactive_array.count != interactive_array->count){
          resize(&undo->interactive_array, interactive_array->count);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          undo->interactive_array.elements[i] = interactive_array->elements[i];
     }
}

void undo_commit(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                 ObjectArray_t<Interactive_t>* interactive_array){
     // don't save undo if any blocks are moving
     for(S16 i = 0; i < block_array->count; i++){
          if(block_array->elements[i].vel.x != 0.0f ||
             block_array->elements[i].vel.y != 0.0f) return;
     }

     U32 diff_count = 0;

     // player
     if(player->pos.pixel != undo->player.pixel ||
        player->pos.z != undo->player.z ||
        player->face != undo->player.face){
          auto* undo_player = (UndoPlayer_t*)(undo->history.current);
          *undo_player = undo->player;
          undo_history_add(&undo->history, UNDO_DIFF_TYPE_PLAYER, 0);
          diff_count++;
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

     // check for differences between block count in previous state and new state
     S16 min_block_count = block_array->count;
     if(block_array->count > undo->block_array.count){
          min_block_count = undo->block_array.count;
          // remove a new blocks
          for(S16 i = undo->block_array.count; i < block_array->count; i++){
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK_REMOVE, i);
               diff_count++;
          }
     }else if(block_array->count < undo->block_array.count){
          // insert a new blocks
          S16 diff = undo->block_array.count - block_array->count;
          for(S16 d = 0; d < diff; d++){
               S16 last_index = (undo->block_array.count - 1) - d;
               UndoBlock_t* last_block = undo->block_array.elements + last_index;
               bool found = false;

               for(S16 i = 0; i < block_array->count; i++){
                    Block_t* block = block_array->elements + i;

                    if(last_block->pixel == block->pos.pixel &&
                       last_block->z == block->pos.z &&
                       last_block->element == block->element &&
                       last_block->entangle_index == block->entangle_index){
                         found = true;
                         auto* undo_block_entry = (UndoBlock_t*)(undo->history.current);
                         *undo_block_entry = undo->block_array.elements[i];
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
     }else{
          // blocks
          for(S16 i = 0; i < min_block_count; i++){
               UndoBlock_t* undo_block = undo->block_array.elements + i;
               Block_t* block = block_array->elements + i;

               if(undo_block->pixel != block->pos.pixel ||
                  undo_block->z != block->pos.z ||
                  undo_block->element != block->element ||
                  undo_block->entangle_index != block->entangle_index){
                    auto* undo_block_entry = (UndoBlock_t*)(undo->history.current);
                    *undo_block_entry = *undo_block;
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_BLOCK, i);
                    diff_count++;
               }
          }
     }

     // check for differences between interactive count in previous state and new state
     S16 min_interactive_count = interactive_array->count;
     if(interactive_array->count > undo->interactive_array.count){
          min_interactive_count = undo->interactive_array.count;

          // remove a new interactive
          for(S16 i = undo->interactive_array.count; i < interactive_array->count; i++){
               undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE_REMOVE, i);
               diff_count++;
          }
     }else if(interactive_array->count < undo->interactive_array.count){
          // insert a new interactives
          S16 diff = undo->interactive_array.count - interactive_array->count;
          for(S16 d = 0; d < diff; d++){
               S16 last_index = (undo->interactive_array.count - 1) - d;
               Interactive_t* last_interactive = undo->interactive_array.elements + last_index;
               bool found = false;

               for(S16 i = 0; i < interactive_array->count; i++){
                    Interactive_t* interactive = interactive_array->elements + i;

                    if(interactive_equal(last_interactive, interactive)){
                         found = true;
                         auto* undo_interactive_entry = (Interactive_t*)(undo->history.current);
                         *undo_interactive_entry = undo->interactive_array.elements[i];
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
     }else{
          // interactives
          for(S16 i = 0; i < min_interactive_count; i++){
               Interactive_t* undo_interactive = undo->interactive_array.elements + i;
               Interactive_t* interactive = interactive_array->elements + i;
               assert(undo_interactive->type == interactive->type);

               if(!interactive_equal(undo_interactive, interactive)){
                    auto* undo_interactive_entry = (Interactive_t*)(undo->history.current);
                    *undo_interactive_entry = *undo_interactive;
                    undo_history_add(&undo->history, UNDO_DIFF_TYPE_INTERACTIVE, i);
                    diff_count++;
               }
          }
     }

     // finally, write number of diffs
     if(diff_count){
          auto* count_entry = (S32*)(undo->history.current);
          *count_entry = diff_count;
          undo->history.current = (char*)(undo->history.current) + sizeof(S32);
          ASSERT_BELOW_HISTORY_SIZE((&undo->history));

          undo_snapshot(undo, player, tilemap, block_array, interactive_array);
     }
}

void undo_revert(Undo_t* undo, Player_t* player, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                 ObjectArray_t<Interactive_t>* interactive_array){
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
               *player = {};
               // TODO fix these numbers as they are important
               player->walk_frame_delta = 1;
               player->pos.pixel = player_entry->pixel;
               player->pos.z = player_entry->z;
               player->face = player_entry->face;
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
               Block_t* block = block_array->elements + diff_header->index;
               *block = {};
               block->pos.pixel = block_entry->pixel;
               block->pos.z = block_entry->z;
               block->element = block_entry->element;
               block->accel = block_entry->accel;
               block->vel = block_entry->vel;
               block->entangle_index = block_entry->entangle_index;
          } break;
          case UNDO_DIFF_TYPE_BLOCK_INSERT:
          {
               ptr -= sizeof(UndoBlock_t);
               auto* block_entry = (UndoBlock_t*)(ptr);
               S16 last_index = block_array->count;
               resize(block_array, block_array->count + 1);

               // move the block at that index back to the end of the list
               block_array->elements[last_index] = block_array->elements[diff_header->index];

               // override it with the insert
               Block_t* block = block_array->elements + diff_header->index;
               *block = {};
               block->pos.pixel = block_entry->pixel;
               block->pos.z = block_entry->z;
               block->element = block_entry->element;
               block->accel = block_entry->accel;
               block->vel = block_entry->vel;
               block->entangle_index = block_entry->entangle_index;
          } break;
          case UNDO_DIFF_TYPE_BLOCK_REMOVE:
          {
               remove(block_array, diff_header->index);
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE:
          {
               ptr -= sizeof(Interactive_t);
               auto* interactive_entry = (Interactive_t*)(ptr);
               interactive_array->elements[diff_header->index] = *interactive_entry;
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE_INSERT:
          {
               ptr -= sizeof(Interactive_t);
               auto* interactive_entry = (Interactive_t*)(ptr);
               S16 last_index = interactive_array->count;
               resize(interactive_array, interactive_array->count + 1);
               interactive_array->elements[last_index] = interactive_array->elements[diff_header->index];
               interactive_array->elements[diff_header->index] = *interactive_entry;
          } break;
          case UNDO_DIFF_TYPE_INTERACTIVE_REMOVE:
          {
               remove(interactive_array, diff_header->index);
          } break;
          }
     }

     undo->history.current = ptr;
     undo_snapshot(undo, player, tilemap, block_array, interactive_array);
}
