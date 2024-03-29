#include "map_format.h"
#include "defines.h"

// only here for tag generation
#include "block_utils.h"
#include "portal_exit.h"
#include "quad_tree.h"
#include "tags.h"
#include "utils.h"

#include <string.h>

// Cant wait to get rid of this garbahj
static void convert_deprecated_id_to_id_and_rotation(U8 deprecated_id, U8* new_id, U8* rotation){
     switch(deprecated_id){
     case 16:
          *new_id = 0;
          *rotation = 0;
          break;
     case 17:
          *new_id = 1;
          *rotation = 2;
          break;
     case 18:
          *new_id = 0;
          *rotation = 3;
          break;
     case 19:
          *new_id = 1;
          *rotation = 3;
          break;
     case 20:
          *new_id = 0;
          *rotation = 3;
          break;
     case 21:
          *new_id = 2;
          *rotation = 0;
          break;
     case 22:
          *new_id = 3;
          *rotation = 1;
          break;
     case 23:
          *new_id = 0;
          *rotation = 0;
          break;
     case 24:
          *new_id = 0;
          *rotation = 0;
          break;
     case 25:
          *new_id = 3;
          *rotation = 2;
          break;
     case 26:
          *new_id = 2;
          *rotation = 3;
          break;
     case 27:
          *new_id = 0;
          *rotation = 1;
          break;
     case 28:
          *new_id = 1;
          *rotation = 1;
          break;
     case 29:
          *new_id = 4;
          *rotation = 0;
          break;
     case 30:
          *new_id = 5;
          *rotation = 1;
          break;
     case 31:
          *new_id = 1;
          *rotation = 2;
          break;
     case 32:
          *new_id = 1;
          *rotation = 0;
          break;
     case 33:
          *new_id = 0;
          *rotation = 2;
          break;
     case 34:
          *new_id = 1;
          *rotation = 1;
          break;
     case 35:
          *new_id = 0;
          *rotation = 1;
          break;
     case 36:
          *new_id = 3;
          *rotation = 0;
          break;
     case 37:
          *new_id = 0;
          *rotation = 2;
          break;
     case 38:
          *new_id = 0;
          *rotation = 3;
          break;
     case 39:
          *new_id = 2;
          *rotation = 1;
          break;
     case 40:
          *new_id = 2;
          *rotation = 2;
          break;
     case 41:
          *new_id = 0;
          *rotation = 1;
          break;
     case 42:
          *new_id = 0;
          *rotation = 2;
          break;
     case 43:
          *new_id = 3;
          *rotation = 3;
          break;
     case 44:
          *new_id = 5;
          *rotation = 0;
          break;
     case 45:
          *new_id = 1;
          *rotation = 0;
          break;
     case 46:
          *new_id = 1;
          *rotation = 1;
          break;
     case 47:
          *new_id = 4;
          *rotation = 1;
          break;
     case 48:
          *new_id = 1;
          *rotation = 2;
          break;
     case 49:
          *new_id = 5;
          *rotation = 2;
          break;
     case 50:
          *new_id = 4;
          *rotation = 2;
          break;
     case 51:
          *new_id = 1;
          *rotation = 3;
          break;
     case 52:
          *new_id = 4;
          *rotation = 3;
          break;
     case 53:
          *new_id = 1;
          *rotation = 3;
          break;
     case 54:
          *new_id = 1;
          *rotation = 0;
          break;
     case 55:
          *new_id = 5;
          *rotation = 3;
          break;
     default:
          new_id = 0;
          rotation = 0;
          break;
     }
}

bool save_map_to_file(FILE* file, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                      ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
                      ObjectArray_t<Exit_t>* exit_array, bool* tags, Raw_t* thumbnail){
     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(tilemap->width) * (S32)(tilemap->height);
     MapTileV2_t* map_tiles = (MapTileV2_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_array->count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_array->count);
          return false;
     }

     MapInteractiveV4_t* map_interactives = (MapInteractiveV4_t*)(calloc((size_t)(interactive_array->count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_array->count);
          return false;
     }

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               map_tiles[index].id = tilemap->tiles[y][x].id;
               map_tiles[index].flags = tilemap->tiles[y][x].flags;
               map_tiles[index].rotation = tilemap->tiles[y][x].rotation;
               index++;
          }
     }

     for(S16 i = 0; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          build_map_block_from_block(map_blocks + i, block);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          build_map_interactive_from_interactive(map_interactives + i, interactive_array->elements + i);
     }

     U8 map_version = MAP_VERSION;
     fwrite(&map_version, sizeof(map_version), 1, file);
     fwrite(&player_start, sizeof(player_start), 1, file);
     fwrite(&tilemap->width, sizeof(tilemap->width), 1, file);
     fwrite(&tilemap->height, sizeof(tilemap->height), 1, file);
     fwrite(&block_array->count, sizeof(block_array->count), 1, file);
     fwrite(&interactive_array->count, sizeof(interactive_array->count), 1, file);
     fwrite(&room_array->count, sizeof(room_array->count), 1, file);
     fwrite(&exit_array->count, sizeof(exit_array->count), 1, file);
     fwrite(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fwrite(map_blocks, sizeof(*map_blocks), (size_t)(block_array->count), file);
     fwrite(map_interactives, sizeof(*map_interactives), (size_t)(interactive_array->count), file);
     fwrite(room_array->elements, sizeof(*room_array->elements), (size_t)(room_array->count), file);

     for(S16 i = 0; i < exit_array->count; i++){
          Exit_t* exit = exit_array->elements + i;
          write_exit(file, exit);
     }

     if(thumbnail){
          fwrite(&thumbnail->byte_count, sizeof(thumbnail->byte_count), 1, file);
          fwrite(thumbnail->bytes, thumbnail->byte_count, 1, file);
     }else{
          U64 thumbnail_size = 0;
          fwrite(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     }

     U16 tags_count = 0;

     if(tags){
          for(U16 t = 0; t < TAG_COUNT; t++){
               if(tags[t]) tags_count++;
          }
          fwrite(&tags_count, sizeof(tags_count), 1, file);
          for(U16 t = 0; t < TAG_COUNT; t++){
               if(tags[t]) fwrite(&t, sizeof(t), 1, file);
          }
     }else{
          fwrite(&tags_count, sizeof(tags_count), 1, file);
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool save_map(const char* filepath, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
              ObjectArray_t<Exit_t>* exit_array, bool* tags, Raw_t* thumbnail){
     // write to file
     FILE* f = fopen(filepath, "wb");
     if(!f){
          LOG("%s: fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = save_map_to_file(f, player_start, tilemap, block_array, interactive_array, room_array, exit_array,
                                     tags, thumbnail);
     LOG("saved map %s %s thumbnail and %stags\n", filepath, thumbnail == nullptr ? "without" : "with",
         tags == nullptr ? "no " : "");
     fclose(f);
     return success;
}

bool load_map_from_file_v4(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     U64 thumbnail_size;
     U16 tag_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc((size_t)(interactive_count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fread(map_blocks, sizeof(*map_blocks), (size_t)(block_count), file);
     fread(map_interactives, sizeof(*map_interactives), (size_t)(interactive_count), file);

     // mostly skip over the extra thumbnail and tags
     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);
     fread(&tag_count, sizeof(tag_count), 1, file);
     fseek(file, tag_count * sizeof(Tag_t), SEEK_CUR);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               auto* tile = tilemap->tiles[y] + x;
               tile->flags = map_tiles[index].flags;
               if(map_tiles[index].id >= OLD_TILE_ID_SOLID_START){
                    tile->flags |= TILE_FLAG_SOLID;
                    convert_deprecated_id_to_id_and_rotation(map_tiles[index].id, &tile->id, &tile->rotation);
               }else{
                    tile->id = map_tiles[index].id;
                    tile->rotation = 0;
               }
               tile->light = BASE_LIGHT;
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          default_block(block);
          build_block_from_map_block(block, map_blocks + i);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          interactive->coord = map_interactives[i].coord;
          interactive->type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactives[i].popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactives[i].popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactives[i].door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.lift.ticks = DOOR_MAX_HEIGHT;
               interactive->door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactives[i].portal.face;
               interactive->portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactives[i].stairs.up;
               interactive->stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_CHECKPOINT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          case INTERACTIVE_TYPE_PIT:
               interactive->pit.id = 0;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v6(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     U64 thumbnail_size;
     U16 tag_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV2_t* map_interactives = (MapInteractiveV2_t*)(calloc((size_t)(interactive_count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fread(map_blocks, sizeof(*map_blocks), (size_t)(block_count), file);
     fread(map_interactives, sizeof(*map_interactives), (size_t)(interactive_count), file);

     // mostly skip over the extra thumbnail and tags
     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);
     fread(&tag_count, sizeof(tag_count), 1, file);
     fseek(file, tag_count * sizeof(Tag_t), SEEK_CUR);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               auto* tile = tilemap->tiles[y] + x;
               tile->flags = map_tiles[index].flags;
               if(map_tiles[index].id >= OLD_TILE_ID_SOLID_START){
                    tile->flags |= TILE_FLAG_SOLID;
                    convert_deprecated_id_to_id_and_rotation(map_tiles[index].id, &tile->id, &tile->rotation);
               }else{
                    tile->id = map_tiles[index].id;
                    tile->rotation = 0;
               }
               tile->light = BASE_LIGHT;
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          default_block(block);
          build_block_from_map_block(block, map_blocks + i);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          interactive->coord = map_interactives[i].coord;
          interactive->type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactives[i].popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactives[i].popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactives[i].door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.lift.ticks = DOOR_MAX_HEIGHT;
               interactive->door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactives[i].portal.face;
               interactive->portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactives[i].stairs.up;
               interactive->stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_CHECKPOINT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          case INTERACTIVE_TYPE_PIT:
               interactive->pit.id = map_interactives[i].pit.id;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v7(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     U64 thumbnail_size;
     U16 tag_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV3_t* map_interactives = (MapInteractiveV3_t*)(calloc((size_t)(interactive_count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fread(map_blocks, sizeof(*map_blocks), (size_t)(block_count), file);
     fread(map_interactives, sizeof(*map_interactives), (size_t)(interactive_count), file);

     // mostly skip over the extra thumbnail and tags
     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);
     fread(&tag_count, sizeof(tag_count), 1, file);
     fseek(file, tag_count * sizeof(Tag_t), SEEK_CUR);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               auto* tile = tilemap->tiles[y] + x;
               tile->flags = map_tiles[index].flags;
               if(map_tiles[index].id >= OLD_TILE_ID_SOLID_START){
                    tile->flags |= TILE_FLAG_SOLID;
                    convert_deprecated_id_to_id_and_rotation(map_tiles[index].id, &tile->id, &tile->rotation);
               }else{
                    tile->id = map_tiles[index].id;
                    tile->rotation = 0;
               }
               tile->light = BASE_LIGHT;
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          default_block(block);
          build_block_from_map_block(block, map_blocks + i);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          interactive->coord = map_interactives[i].coord;
          interactive->type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactives[i].popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactives[i].popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactives[i].door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.lift.ticks = DOOR_MAX_HEIGHT;
               interactive->door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactives[i].portal.face;
               interactive->portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactives[i].stairs.up;
               interactive->stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_CHECKPOINT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          case INTERACTIVE_TYPE_PIT:
               interactive->pit.id = map_interactives[i].pit.id;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v8(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     S16 room_count;
     U64 thumbnail_size;
     U16 tag_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);
     fread(&room_count, sizeof(room_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV2_t* map_tiles = (MapTileV2_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV3_t* map_interactives = (MapInteractiveV3_t*)(calloc((size_t)(interactive_count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     if(!resize(room_array, room_count)){
          LOG("%s(): failed to allocate %d rooms\n", __FUNCTION__, room_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fread(map_blocks, sizeof(*map_blocks), (size_t)(block_count), file);
     fread(map_interactives, sizeof(*map_interactives), (size_t)(interactive_count), file);
     fread(room_array->elements, sizeof(*room_array->elements), (size_t)(room_count), file);

     // mostly skip over the extra thumbnail and tags
     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);
     fread(&tag_count, sizeof(tag_count), 1, file);
     fseek(file, tag_count * sizeof(Tag_t), SEEK_CUR);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               tilemap->tiles[y][x].id = map_tiles[index].id;
               tilemap->tiles[y][x].flags = map_tiles[index].flags;
               tilemap->tiles[y][x].light = BASE_LIGHT;
               tilemap->tiles[y][x].rotation = map_tiles[index].rotation;
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          default_block(block);
          build_block_from_map_block(block, map_blocks + i);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          interactive->coord = map_interactives[i].coord;
          interactive->type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactives[i].popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactives[i].popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactives[i].door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.lift.ticks = DOOR_MAX_HEIGHT;
               interactive->door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactives[i].portal.face;
               interactive->portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactives[i].stairs.up;
               interactive->stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_CHECKPOINT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          case INTERACTIVE_TYPE_PIT:
               interactive->pit.id = map_interactives[i].pit.id;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v9(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
                           ObjectArray_t<Exit_t>* exit_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     S16 room_count;
     S16 exit_count;
     U64 thumbnail_size;
     U16 tag_count;

     fread(player_start, sizeof(*player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);
     fread(&room_count, sizeof(room_count), 1, file);
     fread(&exit_count, sizeof(exit_count), 1, file);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(map_width) * (S32)(map_height);
     MapTileV2_t* map_tiles = (MapTileV2_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV4_t* map_interactives = (MapInteractiveV4_t*)(calloc((size_t)(interactive_count), sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_count);
          return false;
     }

     if(!resize(room_array, room_count)){
          LOG("%s(): failed to allocate %d rooms\n", __FUNCTION__, room_count);
          return false;
     }

     if(!resize(exit_array, exit_count)){
          LOG("%s(): failed to allocate %d exit_array\n", __FUNCTION__, exit_count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fread(map_blocks, sizeof(*map_blocks), (size_t)(block_count), file);
     fread(map_interactives, sizeof(*map_interactives), (size_t)(interactive_count), file);
     fread(room_array->elements, sizeof(*room_array->elements), (size_t)(room_count), file);

     for(S16 i = 0; i < exit_count; i++){
          read_exit(file, exit_array->elements + i);
     }

     // mostly skip over the extra thumbnail and tags
     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);
     fread(&tag_count, sizeof(tag_count), 1, file);
     fseek(file, tag_count * sizeof(Tag_t), SEEK_CUR);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     S32 index = 0;
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               tilemap->tiles[y][x].id = map_tiles[index].id;
               tilemap->tiles[y][x].flags = map_tiles[index].flags;
               tilemap->tiles[y][x].light = BASE_LIGHT;
               tilemap->tiles[y][x].rotation = map_tiles[index].rotation;
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          default_block(block);
          build_block_from_map_block(block, map_blocks + i);
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          auto* interactive = interactive_array->elements + i;
          auto* map_interactive = map_interactives + i;

          interactive->coord = map_interactive->coord;
          interactive->type = map_interactive->type;

          switch(map_interactive->type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive->pressure_plate = map_interactive->pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive->detector = map_interactive->detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = map_interactive->popup.up;
               interactive->popup.lift.timer = 0.0f;
               interactive->popup.iced = map_interactive->popup.iced;
               if(interactive->popup.lift.up){
                    interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive->popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = map_interactive->door.up;
               interactive->door.lift.timer = 0.0f;
               interactive->door.lift.ticks = map_interactive->door.ticks;
               interactive->door.face = map_interactive->door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive->portal.face = map_interactive->portal.face;
               interactive->portal.on = map_interactive->portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive->stairs.up = map_interactive->stairs.up;
               interactive->stairs.face = map_interactive->stairs.face;
               interactive->stairs.exit_index = map_interactive->stairs.exit_index;
               break;
          case INTERACTIVE_TYPE_CHECKPOINT:
               interactive->checkpoint = false;
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactive->wire_cross.on;
               interactive->wire_cross.mask = map_interactive->wire_cross.mask;
               break;
          case INTERACTIVE_TYPE_PIT:
               interactive->pit.id = map_interactive->pit.id;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                        ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
                        ObjectArray_t<Exit_t>* exit_array, const char* filepath){
     U8 map_version = 0;
     fread(&map_version, sizeof(map_version), 1, file);
     bool result = false;
     switch(map_version){
     default:
          LOG("%s(): mismatched version loading '%s', actual %d, expected %d\n", __FUNCTION__, filepath, map_version, MAP_VERSION);
          break;
     case 1:
     case 2:
     case 3:
          LOG("%s(): deprecated version %d, use git to find the code to load this map if ya really want bro\n",
              __FUNCTION__, map_version);
          break;
     case 4:
     case 5:
          result = load_map_from_file_v4(file, player_start, tilemap, block_array, interactive_array);
          break;
     case 6:
          result = load_map_from_file_v6(file, player_start, tilemap, block_array, interactive_array);
          break;
     case 7:
          result = load_map_from_file_v7(file, player_start, tilemap, block_array, interactive_array);
          break;
     case 8:
          result = load_map_from_file_v8(file, player_start, tilemap, block_array, interactive_array, room_array);
          break;
     case 9:
          result = load_map_from_file_v9(file, player_start, tilemap, block_array, interactive_array, room_array, exit_array);
          break;
     }

     for(S16 y = 0; y < tilemap->height; y++){
          for(S16 x = 0; x < tilemap->width; x++){
               Tile_t* tile = tilemap->tiles[y] + x;
               if(tile->flags & TILE_FLAG_ICED) add_global_tag(TAG_ICE);
          }
     }

     auto* block_qt = quad_tree_build(block_array);
     auto* interactive_qt = quad_tree_build(interactive_array);

     // TODO: We use 1 because there is always a mystery first block, we should fix that, then fix this
     if(block_array->count > 1) add_global_tag(TAG_BLOCK);
     for(S16 i = 1; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          if(block->element == ELEMENT_FIRE) add_global_tag(TAG_FIRE_BLOCK);
          if(block->element == ELEMENT_ICE) add_global_tag(TAG_ICE_BLOCK);
          if(block->element == ELEMENT_ONLY_ICED) add_global_tag(TAG_ICED_BLOCK);
          if(block->cut != BLOCK_CUT_WHOLE){ add_global_tag(TAG_SPLIT_BLOCK); }
          if(block->entangle_index >= 0){
               add_global_tag(TAG_ENTANGLED_BLOCK);
               Block_t* entangled_block = block_array->elements + block->entangle_index;
               S8 rot_diff = 0;
               if(entangled_block->rotation > block->rotation){
                    rot_diff = entangled_block->rotation - block->rotation;
               }else{
                    rot_diff = block->rotation - entangled_block->rotation;
               }

               rot_diff %= DIRECTION_COUNT;

               if(rot_diff == 1){
                    add_global_tag(TAG_ENTANGLED_BLOCK_ROT_90);
               }else if(rot_diff == 2){
                    add_global_tag(TAG_ENTANGLED_BLOCK_ROT_180);
               }else if(rot_diff == 3){
                    add_global_tag(TAG_ENTANGLED_BLOCK_ROT_270);
               }

               if(block->cut == BLOCK_CUT_WHOLE && entangled_block->cut != BLOCK_CUT_WHOLE){
                    add_global_tag(TAG_ENTANGLED_BLOCKS_OF_DIFFERENT_SPLITS);
               }

               if(entangled_block->entangle_index != i){
                    add_global_tag(TAG_THREE_PLUS_BLOCKS_ENTANGLED);
               }
          }
          auto held_up_result = block_held_up_by_another_block(block, block_qt, interactive_qt, tilemap);
          if(held_up_result.held()){
               add_global_tag(TAG_BLOCKS_STACKED);
          }
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          Interactive_t* interactive = interactive_array->elements + i;
          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               add_global_tag(TAG_PRESSURE_PLATE);
               if(tilemap_is_iced(tilemap, interactive->coord)){
                    add_global_tag(TAG_ICED_PRESSURE_PLATE);
               }
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
               add_global_tag(TAG_LIGHT_DETECTOR);
               break;
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               add_global_tag(TAG_ICE_DETECTOR);
               break;
          case INTERACTIVE_TYPE_POPUP:
               add_global_tag(TAG_POPUP);
               if(interactive->popup.iced){
                    add_global_tag(TAG_ICED_POPUP);
               }
               break;
          case INTERACTIVE_TYPE_LEVER:
               add_global_tag(TAG_LEVER);
               break;
          case INTERACTIVE_TYPE_DOOR:
               break;
          case INTERACTIVE_TYPE_PORTAL:
          {
               add_global_tag(TAG_PORTAL);
               // TODO: detect different portal rotations

               PortalExit_t portal_exits = find_portal_exits(interactive->coord, tilemap, interactive_qt);

               for(S8 d = 0; d < DIRECTION_COUNT; d++){
                    Direction_t current_portal_dir = (Direction_t)(d);
                    auto portal_exit = portal_exits.directions + d;

                    for(S8 p = 0; p < portal_exit->count; p++){
                         auto portal_dst_coord = portal_exit->coords[p];
                         if(portal_dst_coord == interactive->coord) continue;

                         auto rotations_between_portals = portal_rotations_between(interactive->portal.face, current_portal_dir);

                         if(rotations_between_portals == 1){
                              add_global_tag(TAG_PORTAL_ROT_90);
                         }else if(rotations_between_portals == 2){
                              add_global_tag(TAG_PORTAL_ROT_180);
                         }else if(rotations_between_portals == 3){
                              add_global_tag(TAG_PORTAL_ROT_270);
                         }
                    }
               }

               break;
          }
          case INTERACTIVE_TYPE_BOMB:
               break;
          case INTERACTIVE_TYPE_PIT:
               add_global_tag(TAG_PIT);
               break;
          }
     }

     quad_tree_free(block_qt);
     quad_tree_free(interactive_qt);

     return result;
}

bool load_map(const char* filepath, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
              ObjectArray_t<Exit_t>* exit_array){
     LOG("load map %s\n", filepath);
     FILE* f = fopen(filepath, "rb");
     if(!f){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = load_map_from_file(f, player_start, tilemap, block_array, interactive_array, room_array,
                                       exit_array, filepath);
     fclose(f);
     return success;
}

bool load_map_thumbnail(const char* filepath, Raw_t* thumbnail){
     FILE* file = fopen(filepath, "rb");
     if(!file){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }

     U8 map_version = 0;
     fread(&map_version, sizeof(map_version), 1, file);

     if(map_version < 4){
         LOG("map version %u does not support thumbnail\n", map_version);
         return false;
     }

     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     S16 room_count;
     S16 exit_count;
     U64 thumbnail_size;
     Coord_t player_start;

     fread(&player_start, sizeof(player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     if(map_version <= 7){
          fseek(file, sizeof(MapTileV1_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV1_t) * interactive_count, SEEK_CUR);
     }else if(map_version == 8){
          fread(&room_count, sizeof(room_count), 1, file);

          fseek(file, sizeof(MapTileV2_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV3_t) * interactive_count, SEEK_CUR);
          fseek(file, sizeof(Rect_t) * room_count, SEEK_CUR);
     }else if(map_version == 9){
          fread(&room_count, sizeof(room_count), 1, file);
          fread(&exit_count, sizeof(exit_count), 1, file);

          fseek(file, sizeof(MapTileV2_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV4_t) * interactive_count, SEEK_CUR);
          fseek(file, sizeof(Rect_t) * room_count, SEEK_CUR);

          ObjectArray_t<Exit_t> exits{};
          init(&exits, exit_count);
          for(S16 i = 0; i < exit_count; i++){
               read_exit(file, exits.elements + i);
          }
          destroy(&exits);
     }

     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);

     if(thumbnail_size > 0){
         thumbnail->byte_count = thumbnail_size;
         thumbnail->bytes = (U8*)(malloc(thumbnail_size));
         if(!thumbnail->bytes){
             LOG("Failed to allocate memory for thumbnail of size %lu in file: %s version %d\n", thumbnail_size, filepath, map_version);
             return false;
         }
         fread(thumbnail->bytes, thumbnail_size, 1, file);
     }else{
         LOG("map does not contain a thumbnail\n");
         return false;
     }

     fclose(file);
     return true;
}

bool load_map_tags(const char* filepath, bool* tags){
     FILE* file = fopen(filepath, "rb");
     if(!file){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }

     U8 map_version = 0;
     fread(&map_version, sizeof(map_version), 1, file);

     if(map_version < 5){
         LOG("warning: map version %u on map %s does not support tags\n", map_version, filepath);
         return false;
     }

     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;
     U64 thumbnail_size;
     U16 tag_count;
     S16 room_count;
     S16 exit_count;
     Coord_t player_start;

     fread(&player_start, sizeof(player_start), 1, file);
     fread(&map_width, sizeof(map_width), 1, file);
     fread(&map_height, sizeof(map_height), 1, file);
     fread(&block_count, sizeof(block_count), 1, file);
     fread(&interactive_count, sizeof(interactive_count), 1, file);

     if(map_version <= 7){
          fseek(file, sizeof(MapTileV1_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV1_t) * interactive_count, SEEK_CUR);
     }else if(map_version == 8){
          fread(&room_count, sizeof(room_count), 1, file);

          fseek(file, sizeof(MapTileV2_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV3_t) * interactive_count, SEEK_CUR);
          fseek(file, sizeof(Rect_t) * room_count, SEEK_CUR);
     }else if(map_version == 9){
          fread(&room_count, sizeof(room_count), 1, file);
          fread(&exit_count, sizeof(exit_count), 1, file);

          fseek(file, sizeof(MapTileV2_t) * map_width * map_height, SEEK_CUR);
          fseek(file, sizeof(MapBlockV3_t) * block_count, SEEK_CUR);
          fseek(file, sizeof(MapInteractiveV4_t) * interactive_count, SEEK_CUR);
          fseek(file, sizeof(Rect_t) * room_count, SEEK_CUR);

          ObjectArray_t<Exit_t> exits{};
          init(&exits, exit_count);
          for(S16 i = 0; i < exit_count; i++){
               read_exit(file, exits.elements + i);
          }
          destroy(&exits);
     }

     fread(&thumbnail_size, sizeof(thumbnail_size), 1, file);
     fseek(file, thumbnail_size, SEEK_CUR);

     fread(&tag_count, sizeof(tag_count), 1, file);

     if(tag_count > 0){
          memset(tags, 0, TAG_COUNT * sizeof(*tags));
          U16 value;
          for(U16 t = 0; t < tag_count; t++){
               fread(&value, sizeof(value), 1, file);
               if(value >= 0 && value < TAG_COUNT){
                    tags[value] = true;
               }
          }
     }

     fclose(file);
     return true;
}

void build_map_interactive_from_interactive(MapInteractiveV4_t* map_interactive, const Interactive_t* interactive){
     map_interactive->coord = interactive->coord;
     map_interactive->type = interactive->type;

     switch(map_interactive->type){
     default:
     case INTERACTIVE_TYPE_LEVER:
     case INTERACTIVE_TYPE_BOW:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
          map_interactive->pressure_plate = interactive->pressure_plate;
          break;
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
     case INTERACTIVE_TYPE_ICE_DETECTOR:
          map_interactive->detector = interactive->detector;
          break;
     case INTERACTIVE_TYPE_POPUP:
          map_interactive->popup.up = interactive->popup.lift.up;
          map_interactive->popup.iced = interactive->popup.iced;
          break;
     case INTERACTIVE_TYPE_DOOR:
          map_interactive->door.up = interactive->door.lift.up;
          map_interactive->door.face = interactive->door.face;
          map_interactive->door.ticks = interactive->door.lift.ticks;
          break;
     case INTERACTIVE_TYPE_PORTAL:
          map_interactive->portal.face = interactive->portal.face;
          map_interactive->portal.on = interactive->portal.on;
          break;
     case INTERACTIVE_TYPE_STAIRS:
          map_interactive->stairs.up = interactive->stairs.up;
          map_interactive->stairs.face = interactive->stairs.face;
          map_interactive->stairs.exit_index = interactive->stairs.exit_index;
          break;
     case INTERACTIVE_TYPE_CHECKPOINT:
          break;
     case INTERACTIVE_TYPE_WIRE_CROSS:
          map_interactive->wire_cross.mask = interactive->wire_cross.mask;
          map_interactive->wire_cross.on = interactive->wire_cross.on;
          break;
     case INTERACTIVE_TYPE_PIT:
          map_interactive->pit.id = interactive->pit.id;
          map_interactive->pit.iced = interactive->pit.iced;
          break;
     }
}

void build_interactive_from_map_interactive(Interactive_t* interactive, const MapInteractiveV4_t* map_interactive){
     interactive->coord = map_interactive->coord;
     interactive->type = map_interactive->type;

     switch(map_interactive->type){
     default:
     case INTERACTIVE_TYPE_LEVER:
     case INTERACTIVE_TYPE_BOW:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
          interactive->pressure_plate = map_interactive->pressure_plate;
          break;
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
     case INTERACTIVE_TYPE_ICE_DETECTOR:
          interactive->detector = map_interactive->detector;
          break;
     case INTERACTIVE_TYPE_POPUP:
          interactive->popup.lift.up = map_interactive->popup.up;
          interactive->popup.lift.timer = 0.0f;
          interactive->popup.iced = map_interactive->popup.iced;
          if(interactive->popup.lift.up){
               interactive->popup.lift.ticks = HEIGHT_INTERVAL + 1;
          }else{
               interactive->popup.lift.ticks = 1;
          }
          break;
     case INTERACTIVE_TYPE_DOOR:
          interactive->door.lift.up = map_interactive->door.up;
          interactive->door.lift.timer = 0.0f;
          interactive->door.lift.ticks = map_interactive->door.ticks;
          interactive->door.face = map_interactive->door.face;
          break;
     case INTERACTIVE_TYPE_PORTAL:
          interactive->portal.face = map_interactive->portal.face;
          interactive->portal.on = map_interactive->portal.on;
          break;
     case INTERACTIVE_TYPE_STAIRS:
          interactive->stairs.up = map_interactive->stairs.up;
          interactive->stairs.face = map_interactive->stairs.face;
          interactive->stairs.exit_index = map_interactive->stairs.exit_index;
          break;
     case INTERACTIVE_TYPE_CHECKPOINT:
          break;
     case INTERACTIVE_TYPE_WIRE_CROSS:
          interactive->wire_cross.on = map_interactive->wire_cross.on;
          interactive->wire_cross.mask = map_interactive->wire_cross.mask;
          break;
     case INTERACTIVE_TYPE_PIT:
          interactive->pit.id = map_interactive->pit.id;
          break;
     }
}

void build_map_block_from_block(MapBlockV3_t* map_block, const Block_t* block){
     map_block->pixel = block->pos.pixel;
     map_block->z = block->pos.z;
     map_block->rotation = block->rotation;
     map_block->element = block->element;
     map_block->entangle_index = block->entangle_index;
     map_block->cut = block->cut;
}

void build_block_from_map_block(Block_t* block, const MapBlockV3_t* map_block){
     block->pos.pixel = map_block->pixel;
     block->pos.z = map_block->z;
     block->rotation = map_block->rotation;
     block->element = map_block->element;
     block->entangle_index = map_block->entangle_index;
     block->cut = map_block->cut;
}
