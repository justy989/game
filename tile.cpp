#include "tile.h"
#include "defines.h"

#include <cstdlib>
#include <cstring>

bool init(TileMap_t* tilemap, S16 width, S16 height){
     tilemap->tiles = (Tile_t**)calloc(height, sizeof(*tilemap->tiles));
     if(!tilemap->tiles) return false;

     for(S16 i = 0; i < height; i++){
          tilemap->tiles[i] = (Tile_t*)calloc(width, sizeof(*tilemap->tiles[i]));
          if(!tilemap->tiles[i]) return false;
     }

     tilemap->width = width;
     tilemap->height = height;

     return true;
}

void destroy(TileMap_t* tilemap){
     for(S16 i = 0; i < tilemap->height; i++){
          free(tilemap->tiles[i]);
     }

     free(tilemap->tiles);
     memset(tilemap, 0, sizeof(*tilemap));
}

Tile_t* tilemap_get_tile(TileMap_t* tilemap, Coord_t coord){
     if(coord.x < 0 || coord.x >= tilemap->width) return NULL;
     if(coord.y < 0 || coord.y >= tilemap->height) return NULL;

     return tilemap->tiles[coord.y] + coord.x;
}

bool tilemap_is_solid(TileMap_t* tilemap, Coord_t coord){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return false;
     return tile->id >= TILE_ID_SOLID_START;
}

bool tilemap_is_iced(TileMap_t* tilemap, Coord_t coord){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return false;
     return (tile->flags & TILE_FLAG_ICED);
}

Direction_t tile_flags_cluster_direction(U16 flags){
     bool first_bit = flags & (1 << 14);
     bool second_bit = flags & (1 << 15);
     if(first_bit){
          if(second_bit){
               return DIRECTION_UP;
          }else{
               return DIRECTION_RIGHT;
          }
     }else{
          if(second_bit){
               return DIRECTION_DOWN;
          }else{
               return DIRECTION_LEFT;
          }
     }
     return DIRECTION_COUNT;
}

void tile_flags_set_cluster_direction(U16* flags, Direction_t dir){
     switch(dir){
     default:
          break;
     case DIRECTION_LEFT:
          OFF_BIT(*flags, 14);
          OFF_BIT(*flags, 15);
          break;
     case DIRECTION_RIGHT:
          ON_BIT(*flags, 14);
          OFF_BIT(*flags, 15);
          break;
     case DIRECTION_UP:
          ON_BIT(*flags, 14);
          ON_BIT(*flags, 15);
          break;
     case DIRECTION_DOWN:
          OFF_BIT(*flags, 14);
          ON_BIT(*flags, 15);
          break;
     }
}
