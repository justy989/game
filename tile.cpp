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

bool tile_is_solid(Tile_t* tile){
     return tile->id >= TILE_ID_SOLID_START;
}

bool tile_is_iced(Tile_t* tile){
     return tile->flags & (TILE_FLAG_ICED_TOP_LEFT | TILE_FLAG_ICED_TOP_RIGHT |
                           TILE_FLAG_ICED_BOTTOM_LEFT | TILE_FLAG_ICED_BOTTOM_RIGHT);
}

bool tile_has_ice(Tile_t* tile){
     return tile_flags_have_ice(tile->flags);
}

bool tilemap_is_solid(TileMap_t* tilemap, Coord_t coord){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return false;
     return tile_is_solid(tile);
}

bool tilemap_is_iced(TileMap_t* tilemap, Coord_t coord){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return false;
     return (tile_is_iced(tile));
}

Tile_t* tilemap_get_tile(TileMap_t* tilemap, Coord_t coord){
     if(coord.x < 0 || coord.x >= tilemap->width) return NULL;
     if(coord.y < 0 || coord.y >= tilemap->height) return NULL;

     return tilemap->tiles[coord.y] + coord.x;
}

bool tile_flags_have_ice(U32 flags){
     return (flags & TILE_FLAG_ICED_TOP_LEFT ||
             flags & TILE_FLAG_ICED_TOP_RIGHT ||
             flags & TILE_FLAG_ICED_BOTTOM_LEFT ||
             flags & TILE_FLAG_ICED_BOTTOM_RIGHT);
}

Direction_t tile_flags_cluster_direction(U32 flags){
     bool first_bit = flags & TILE_FLAG_FIRST_DIRECTION_BIT;
     bool second_bit = flags & TILE_FLAG_SECOND_DIRECTION_BIT;

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

void tile_flags_set_cluster_direction(U32* flags, Direction_t dir){
     switch(dir){
     default:
          break;
     case DIRECTION_LEFT:
          *flags &= ~TILE_FLAG_FIRST_DIRECTION_BIT;
          *flags &= ~TILE_FLAG_SECOND_DIRECTION_BIT;
          break;
     case DIRECTION_RIGHT:
          *flags |= TILE_FLAG_FIRST_DIRECTION_BIT;
          *flags &= ~TILE_FLAG_SECOND_DIRECTION_BIT;
          break;
     case DIRECTION_UP:
          *flags |= TILE_FLAG_FIRST_DIRECTION_BIT;
          *flags |= TILE_FLAG_SECOND_DIRECTION_BIT;
          break;
     case DIRECTION_DOWN:
          *flags &= ~TILE_FLAG_FIRST_DIRECTION_BIT;
          *flags |= TILE_FLAG_SECOND_DIRECTION_BIT;
          break;
     }
}

bool tile_flags_cluster_all_on(U32 flags){
     if(flags & TILE_FLAG_WIRE_CLUSTER_LEFT && !(flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON)){
          return false;
     }

     if(flags & TILE_FLAG_WIRE_CLUSTER_MID && !(flags & TILE_FLAG_WIRE_CLUSTER_MID_ON)){
          return false;
     }

     if(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT && !(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON)){
          return false;
     }

     return true;
}
