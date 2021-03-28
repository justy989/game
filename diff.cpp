#include "diff.h"
#include "utils.h"

#include <string.h>

void calculate_world_changes(World_t* world, ObjectArray_t<DiffEntry_t>* diff){
     assert(world->tilemap.width == world->initial_shallow_world.tilemap.width);
     assert(world->tilemap.height == world->initial_shallow_world.tilemap.height);

     // calculate how many diffs we need
     size_t diff_count = 0;
     for(S16 y = 0; y < world->tilemap.height; y++){
          for(S16 x = 0; x < world->tilemap.width; x++){
               if(world->tilemap.tiles[y][x].flags != world->initial_shallow_world.tilemap.tiles[y][x].flags){
                    diff_count++;
               }
          }
     }

     assert(world->interactives.count == world->initial_shallow_world.interactives.count);

     for(S16 i = 0; i < world->interactives.count; i++){
          auto* interactive_a = world->interactives.elements + i;
          auto* interactive_b = world->initial_shallow_world.interactives.elements + i;

          if(!interactive_equal(interactive_a, interactive_b)){
               diff_count++;
          }
     }

     // TODO: In the cloning map we will hit this, and need to support adding/removing blocks !
     assert(world->blocks.count == world->initial_shallow_world.blocks.count);
     for(S16 i = 0; i < world->blocks.count; i++){
          auto* block_a = world->blocks.elements + i;
          auto* block_b = world->initial_shallow_world.blocks.elements + i;

          if(!block_equal(block_a, block_b)){
               diff_count++;
          }
     }

     destroy(diff);
     if(!init(diff, diff_count)){
          return;
     }

     diff_count = 0;
     for(S16 y = 0; y < world->tilemap.height; y++){
          for(S16 x = 0; x < world->tilemap.width; x++){
               if(world->tilemap.tiles[y][x].flags != world->initial_shallow_world.tilemap.tiles[y][x].flags){
                    auto* diff_entry = diff->elements + diff_count;
                    diff_entry->type = DIFF_TILE_FLAGS;
                    diff_entry->index = y * world->tilemap.width + x;
                    diff_entry->tile_flags = world->tilemap.tiles[y][x].flags;
                    diff_count++;
               }
          }
     }

     assert(world->interactives.count == world->initial_shallow_world.interactives.count);

     for(S16 i = 0; i < world->interactives.count; i++){
          auto* interactive_a = world->interactives.elements + i;
          auto* interactive_b = world->initial_shallow_world.interactives.elements + i;

          if(!interactive_equal(interactive_a, interactive_b)){
               auto* diff_entry = diff->elements + diff_count;
               diff_entry->type = DIFF_INTERACTIVE;
               diff_entry->index = i;
               build_map_interactive_from_interactive(&diff_entry->interactive, interactive_a);
               diff_count++;
          }
     }

     // TODO: In the cloning map we will hit this, and need to support adding/removing blocks !
     assert(world->blocks.count == world->initial_shallow_world.blocks.count);
     for(S16 i = 0; i < world->blocks.count; i++){
          auto* block_a = world->blocks.elements + i;
          auto* block_b = world->initial_shallow_world.blocks.elements + i;

          if(!block_equal(block_a, block_b)){
               auto* diff_entry = diff->elements + diff_count;
               diff_entry->type = DIFF_BLOCK;
               diff_entry->index = i;
               build_map_block_from_block(&diff_entry->block, block_a);
               diff_count++;
          }
     }
}

void apply_diff_to_world(World_t* world, ObjectArray_t<DiffEntry_t>* diff){
     for(S16 i = 0; i < diff->count; i++){
          auto* entry = diff->elements + i;
          switch(entry->type){
          case DIFF_TILE_FLAGS:
          {
               int x = entry->index % world->tilemap.width;
               int y = entry->index / world->tilemap.width;

               world->tilemap.tiles[y][x].flags = entry->tile_flags;
          } break;
          case DIFF_INTERACTIVE:
               build_interactive_from_map_interactive(world->interactives.elements + entry->index, &entry->interactive);
               break;
          case DIFF_BLOCK:
               default_block(world->blocks.elements + entry->index);
               build_block_from_map_block(world->blocks.elements + entry->index, &entry->block);
               break;
          default:
               break;
          }
     }
}

bool world_rect_has_changed(World_t* world, Rect_t rect){
     // in the case where we don't cache the initial shallow world, just say it has changed
     if(world->initial_shallow_world.tilemap.width == 0){
          return true;
     }

     for(S16 y = rect.bottom; y <= rect.top; y++){
          for(S16 x = rect.left; x <= rect.right; x++){
               Coord_t coord{x, y};

               if(world->tilemap.tiles[y][x].flags & TILE_FLAG_RESET_IMMUNE){
                    // ignore flag changes and interactive changes on reset immune tiles
               }else{
                    if(world->tilemap.tiles[y][x].flags != world->initial_shallow_world.tilemap.tiles[y][x].flags){
                         return true;
                    }

                    auto* interactive_a = quad_tree_interactive_find_at(world->interactive_qt, coord);
                    S16 interactive_index = interactive_a - world->interactives.elements;
                    auto* interactive_b = world->initial_shallow_world.interactives.elements + interactive_index;
                    if(interactive_a && interactive_b && !interactive_equal(interactive_a, interactive_b)){
                         return true;
                    }
               }

               auto query_rect = rect_surrounding_coord(coord);

               S16 block_count = 0;
               Block_t* blocks[16];
               quad_tree_find_in(world->block_qt, query_rect, blocks, &block_count, 16);
               for(S16 i = 0; i < block_count; i++){
                    Block_t* block_a = blocks[i];
                    S16 block_index = block_a - world->blocks.elements;
                    auto* block_b = world->initial_shallow_world.blocks.elements + block_index;
                    if(block_a && block_b && !block_equal(block_a, block_b)){
                         return true;
                    }
               }
          }
     }

     return false;
}

char* build_diff_filename_from_map_filename(const char* map_filename){
     // find the base filename, finding the last slash and last period
     const char* itr = map_filename;
     size_t slash_offset = 0;
     size_t period_offset = 0;
     while(*itr){
          if(*itr == '/'){
               slash_offset = itr - map_filename;
          }else if(*itr == '.'){
               period_offset = itr - map_filename;
          }
          itr++;
     }

     size_t basename_length = (period_offset - slash_offset) - 1;
     size_t built_string_length = basename_length + 5;
     char* built_string = (char*)malloc(built_string_length);
     if(!built_string) return built_string;

     strncpy(built_string, map_filename + slash_offset + 1, (period_offset - slash_offset) - 1);
     strcpy(built_string + basename_length, ".bms");
     return built_string;
}

bool save_diffs_to_file(const char* filepath, const ObjectArray_t<DiffEntry_t>* diff, Coord_t player_coord){
     FILE* file = fopen(filepath, "wb");
     if(!file){
          LOG("%s: fopen() failed\n", __FUNCTION__);
          return false;
     }

     fwrite(&player_coord, sizeof(player_coord), 1, file);
     fwrite(&diff->count, sizeof(diff->count), 1, file);
     for(S16 i = 0; i < diff->count; i++){
          fwrite(&diff->elements[i], sizeof(diff->elements[i]), 1, file);
     }

     fclose(file);
     return true;
}

bool load_diffs_from_file(const char* filepath, ObjectArray_t<DiffEntry_t>* diff, Coord_t* player_start){
     FILE* file = fopen(filepath, "rb");
     if(!file){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }

     fread(player_start, sizeof(*player_start), 1, file);

     S16 diff_entries = 0;
     fread(&diff_entries, sizeof(diff_entries), 1, file);

     destroy(diff);
     if(!init(diff, diff_entries)){
          LOG("%s() failed\n", __FUNCTION__);
          fclose(file);
          return false;
     }

     for(S16 i = 0; i < diff_entries; i++){
          fread(diff->elements + i, sizeof(diff->elements[i]), 1, file);
     }

     fclose(file);
     return true;
}
