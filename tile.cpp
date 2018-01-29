#include "tile.h"
#include "defines.h"
#include "conversion.h"

#include <cstdlib>
#include <cstring>

bool init(TileMap_t* tilemap, S16 width, S16 height){
     S16 light_width = width * 2;
     S16 light_height = height * 2;

     tilemap->tiles = (Tile_t**)calloc(height, sizeof(*tilemap->tiles));
     if(!tilemap->tiles) return false;

     tilemap->light = (U8**)calloc(light_height, sizeof(*tilemap->light));
     if(!tilemap->light) return false;

     for(S16 i = 0; i < height; i++){
          tilemap->tiles[i] = (Tile_t*)calloc(width, sizeof(*tilemap->tiles[i]));
          if(!tilemap->tiles[i]) return false;
     }

     for(S16 i = 0; i < light_height; i++){
          tilemap->light[i] = (U8*)calloc(light_width, sizeof(*tilemap->light[i]));
          if(!tilemap->light[i]) return false;
     }

     tilemap->width = width;
     tilemap->height = height;

     return true;
}

void destroy(TileMap_t* tilemap){
     for(S16 i = 0; i < tilemap->height; i++){
          free(tilemap->tiles[i]);
     }

     S16 light_height = tilemap->height * 2;
     for(S16 i = 0; i < light_height; i++){
          free(tilemap->light[i]);
     }

     free(tilemap->tiles);
     free(tilemap->light);
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

void tilemap_set_ice(TileMap_t* tilemap, Half_t half, bool value){
     Coord_t coord = half_to_coord(half);
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return;
     S16 x_off = half.x % 2;
     S16 y_off = half.y % 2;

     if(value){
          if(x_off){
               if(y_off){
                    tile->flags |= TILE_FLAG_ICED_TOP_RIGHT;
               }else{
                    tile->flags |= TILE_FLAG_ICED_BOTTOM_RIGHT;
               }
          }else{
               if(y_off){
                    tile->flags |= TILE_FLAG_ICED_TOP_LEFT;
               }else{
                    tile->flags |= TILE_FLAG_ICED_BOTTOM_LEFT;
               }
          }
     }else{
          if(x_off){
               if(y_off){
                    tile->flags &= ~TILE_FLAG_ICED_TOP_RIGHT;
               }else{
                    tile->flags &= ~TILE_FLAG_ICED_BOTTOM_RIGHT;
               }
          }else{
               if(y_off){
                    tile->flags &= ~TILE_FLAG_ICED_TOP_LEFT;
               }else{
                    tile->flags &= ~TILE_FLAG_ICED_BOTTOM_LEFT;
               }
          }
     }

}

bool tilemap_get_ice(TileMap_t* tilemap, Half_t half){
     Coord_t coord = half_to_coord(half);
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(!tile) return false;
     S16 x_off = half.x % 2;
     S16 y_off = half.y % 2;

     if(x_off){
          if(y_off){
               return tile->flags & TILE_FLAG_ICED_TOP_RIGHT;
          }else{
               return tile->flags & TILE_FLAG_ICED_BOTTOM_RIGHT;
          }
     }

     if(y_off){
          return tile->flags & TILE_FLAG_ICED_TOP_LEFT;
     }

     return tile->flags & TILE_FLAG_ICED_BOTTOM_LEFT;
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

Tile_t* tilemap_get_tile(TileMap_t* tilemap, Half_t half){
     if(half.x < 0 || half.x >= (tilemap->width * 2)) return NULL;
     if(half.y < 0 || half.y >= (tilemap->height * 2)) return NULL;

     Coord_t coord = half_to_coord(half);

     return tilemap->tiles[coord.y] + coord.x;
}

U8* tilemap_get_light(TileMap_t* tilemap, Half_t half){
     if(half.x < 0 || half.x >= (tilemap->width * 2)) return NULL;
     if(half.y < 0 || half.y >= (tilemap->height * 2)) return NULL;

     return tilemap->light[half.y] + half.x;
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
