#include "map_format.h"
#include "defines.h"

bool save_map_to_file(FILE* file, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                      ObjectArray_t<Interactive_t>* interactive_array){
     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(tilemap->width) * (S32)(tilemap->height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc((size_t)(map_tile_count), sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV3_t* map_blocks = (MapBlockV3_t*)(calloc((size_t)(block_array->count), sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_array->count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc((size_t)(interactive_array->count), sizeof(*map_interactives)));
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
               index++;
          }
     }

     for(S16 i = 0; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          map_blocks[i].pixel = block->pos.pixel;
          map_blocks[i].z = block->pos.z;
          map_blocks[i].rotation = block->rotation;
          map_blocks[i].element = block->element;
          map_blocks[i].entangle_index = block->entangle_index;
          map_blocks[i].cut = block->cut;
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          map_interactives[i].coord = interactive_array->elements[i].coord;
          map_interactives[i].type = interactive_array->elements[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               map_interactives[i].pressure_plate = interactive_array->elements[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               map_interactives[i].detector = interactive_array->elements[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               map_interactives[i].popup.up = interactive_array->elements[i].popup.lift.up;
               map_interactives[i].popup.iced = interactive_array->elements[i].popup.iced;
               break;
          case INTERACTIVE_TYPE_DOOR:
               map_interactives[i].door.up = interactive_array->elements[i].door.lift.up;
               map_interactives[i].door.face = interactive_array->elements[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               map_interactives[i].portal.face = interactive_array->elements[i].portal.face;
               map_interactives[i].portal.on = interactive_array->elements[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               map_interactives[i].stairs.up = interactive_array->elements[i].stairs.up;
               map_interactives[i].stairs.face = interactive_array->elements[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_PROMPT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               map_interactives[i].wire_cross.mask = interactive_array->elements[i].wire_cross.mask;
               map_interactives[i].wire_cross.on = interactive_array->elements[i].wire_cross.on;
               break;
          }
     }

     U8 map_version = MAP_VERSION;
     fwrite(&map_version, sizeof(map_version), 1, file);
     fwrite(&player_start, sizeof(player_start), 1, file);
     fwrite(&tilemap->width, sizeof(tilemap->width), 1, file);
     fwrite(&tilemap->height, sizeof(tilemap->height), 1, file);
     fwrite(&block_array->count, sizeof(block_array->count), 1, file);
     fwrite(&interactive_array->count, sizeof(interactive_array->count), 1, file);
     fwrite(map_tiles, sizeof(*map_tiles), (size_t)(map_tile_count), file);
     fwrite(map_blocks, sizeof(*map_blocks), (size_t)(block_array->count), file);
     fwrite(map_interactives, sizeof(*map_interactives), (size_t)(interactive_array->count), file);

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool save_map(const char* filepath, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array){
     // write to file
     FILE* f = fopen(filepath, "wb");
     if(!f){
          LOG("%s: fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = save_map_to_file(f, player_start, tilemap, block_array, interactive_array);
     LOG("saved map %s\n", filepath);
     fclose(f);
     return success;
}

bool load_map_from_file_v1(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

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

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
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
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          *block = Block_t{};
          block->pos.pixel = map_blocks[i].pixel;
          block->pos.z = map_blocks[i].z;
          block->rotation = map_blocks[i].rotation;
          block->element = map_blocks[i].element;
          block->entangle_index = -1;
          block->clone_start = Coord_t{};
          block->clone_id = 0;
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
          case INTERACTIVE_TYPE_PROMPT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v2(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

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

     MapBlockV2_t* map_blocks = (MapBlockV2_t*)(calloc((size_t)(block_count), sizeof(*map_blocks)));
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
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          *block = Block_t{};
          block->pos.pixel = map_blocks[i].pixel;
          block->pos.z = map_blocks[i].z;
          block->rotation = map_blocks[i].rotation;
          block->element = map_blocks[i].element;
          block->entangle_index = map_blocks[i].entangle_index;
          block->clone_start = Coord_t{};
          block->clone_id = 0;
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
          case INTERACTIVE_TYPE_PROMPT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file_v3(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                           ObjectArray_t<Interactive_t>* interactive_array){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

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
               index++;
          }
     }

     // TODO: a lot of maps have -16, -16 as the first block
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = block_array->elements + i;
          *block = Block_t{};
          block->pos.pixel = map_blocks[i].pixel;
          block->pos.z = map_blocks[i].z;
          block->rotation = map_blocks[i].rotation;
          block->element = map_blocks[i].element;
          block->entangle_index = map_blocks[i].entangle_index;
          block->cut = map_blocks[i].cut;
          block->clone_start = Coord_t{};
          block->clone_id = 0;
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
          case INTERACTIVE_TYPE_PROMPT:
               break;
          case INTERACTIVE_TYPE_WIRE_CROSS:
               interactive->wire_cross.on = map_interactives[i].wire_cross.on;
               interactive->wire_cross.mask = map_interactives[i].wire_cross.mask;
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                        ObjectArray_t<Interactive_t>* interactive_array, const char* filepath){
     U8 map_version = 0;
     fread(&map_version, sizeof(map_version), 1, file);
     switch(map_version){
     default:
          LOG("%s(): mismatched version loading '%s', actual %d, expected %d\n", __FUNCTION__, filepath, map_version, MAP_VERSION);
          break;
     case 1:
          return load_map_from_file_v1(file, player_start, tilemap, block_array, interactive_array);
     case 2:
          return load_map_from_file_v2(file, player_start, tilemap, block_array, interactive_array);
     case 3:
          return load_map_from_file_v3(file, player_start, tilemap, block_array, interactive_array);
     }

     return false;
}

bool load_map(const char* filepath, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array){
     FILE* f = fopen(filepath, "rb");
     if(!f){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }
     bool success = load_map_from_file(f, player_start, tilemap, block_array, interactive_array, filepath);
     fclose(f);
     return success;
}

