#include "diff.h"

void diff_worlds(World_t* world_a, World_t* world_b, ObjectArray_t<DiffEntry_t>* diff){
     assert(world_a->tilemap.width == world_b->tilemap.width);
     assert(world_a->tilemap.height == world_b->tilemap.height);

     // calculate how many diffs we need
     size_t diff_count = 0;
     for(S16 y = 0; y < world_a->tilemap.height; y++){
          for(S16 x = 0; x < world_a->tilemap.width; x++){
               if(world_a->tilemap.tiles[y][x].flags != world_b->tilemap.tiles[y][x].flags){
                    diff_count++;
               }
          }
     }

     assert(world_a->interactives.count == world_b->interactives.count);

     for(S16 i = 0; i < world_a->interactives.count; i++){
          auto* interactive_a = world_a->interactives.elements + i;
          auto* interactive_b = world_b->interactives.elements + i;

          if(!interactive_equal(interactive_a, interactive_b)){
               diff_count++;
          }
     }

     // TODO: In the cloning map we will hit this, and need to support adding/removing blocks !
     assert(world_a->blocks.count == world_b->blocks.count);
     for(S16 i = 0; i < world_a->blocks.count; i++){
          auto* block_a = world_a->blocks.elements + i;
          auto* block_b = world_b->blocks.elements + i;

          if(!blocks_equal(block_a, block_b)){
               diff_count++;
          }
     }

     destroy(diff);
     if(!init(diff, diff_count)){
          LOG("diff_worlds() failed\n");
          return;
     }

     diff_count = 0;
     for(S16 y = 0; y < world_a->tilemap.height; y++){
          for(S16 x = 0; x < world_a->tilemap.width; x++){
               if(world_a->tilemap.tiles[y][x].flags != world_b->tilemap.tiles[y][x].flags){
                    auto* diff_entry = diff->elements + diff_count;
                    diff_entry->type = DIFF_TILE_FLAGS;
                    diff_entry->index = y * world_a->tilemap.width + x;
                    diff_entry->tile_flags = world_b->tilemap.tiles[y][x].flags;
                    diff_count++;
               }
          }
     }

     assert(world_a->interactives.count == world_b->interactives.count);

     for(S16 i = 0; i < world_a->interactives.count; i++){
          auto* interactive_a = world_a->interactives.elements + i;
          auto* interactive_b = world_b->interactives.elements + i;

          if(!interactive_equal(interactive_a, interactive_b)){
               auto* diff_entry = diff->elements + diff_count;
               diff_entry->type = DIFF_INTERACTIVE;
               diff_entry->index = i;
               build_map_interactive_from_interactive(&diff_entry->interactive, interactive_b);
               diff_count++;
          }
     }

     // TODO: In the cloning map we will hit this, and need to support adding/removing blocks !
     assert(world_a->blocks.count == world_b->blocks.count);
     for(S16 i = 0; i < world_a->blocks.count; i++){
          auto* block_a = world_a->blocks.elements + i;
          auto* block_b = world_b->blocks.elements + i;

          if(!blocks_equal(block_a, block_b)){
               auto* diff_entry = diff->elements + diff_count;
               diff_entry->type = DIFF_BLOCK;
               diff_entry->index = i;
               build_map_block_from_block(&diff_entry->block, block_b);
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
