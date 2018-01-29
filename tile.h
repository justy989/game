#ifndef TILE_H
#define TILE_H

#include "types.h"
#include "coord.h"
#include "half.h"

#define TILE_ID_SOLID_START 16

enum TileFlag_t : U32{
     TILE_FLAG_CHECKPOINT = 1,
     TILE_FLAG_RESET_IMMUNE = 1 << 1,

     TILE_FLAG_ICED_TOP_LEFT = 1 << 2,
     TILE_FLAG_ICED_TOP_RIGHT = 1 << 3,
     TILE_FLAG_ICED_BOTTOM_LEFT = 1 << 4,
     TILE_FLAG_ICED_BOTTOM_RIGHT = 1 << 5,

     TILE_FLAG_WIRE_LEFT = 1 << 6,
     TILE_FLAG_WIRE_UP = 1 << 7,
     TILE_FLAG_WIRE_RIGHT = 1 << 8,
     TILE_FLAG_WIRE_DOWN = 1 << 9,

     TILE_FLAG_WIRE_STATE = 1 << 10,

     TILE_FLAG_WIRE_CLUSTER_LEFT = 1 << 11,
     TILE_FLAG_WIRE_CLUSTER_MID = 1 << 12,
     TILE_FLAG_WIRE_CLUSTER_RIGHT = 1 << 13,

     TILE_FLAG_WIRE_CLUSTER_LEFT_ON = 1 << 14,
     TILE_FLAG_WIRE_CLUSTER_MID_ON = 1 << 15,
     TILE_FLAG_WIRE_CLUSTER_RIGHT_ON = 1 << 16,

     // direction bits combine to specify wire cluster direction
     // 00 left
     // 10 right
     // 11 up
     // 01 down

     TILE_FLAG_FIRST_DIRECTION_BIT = 1 << 17,
     TILE_FLAG_SECOND_DIRECTION_BIT = 1 << 18,
};

struct Tile_t{
     U8 id;
     U32 flags;
};

struct TileMap_t{
     S16 width;
     S16 height;
     Tile_t** tiles;
     U8** light;
};

bool init(TileMap_t* tilemap, S16 width, S16 height);
void destroy(TileMap_t* tilemap);
bool tile_is_solid(Tile_t* tile);
bool tile_is_iced(Tile_t* tile);
bool tile_has_ice(Tile_t* tile);
Tile_t* tilemap_get_tile(TileMap_t* tilemap, Coord_t coord);
Tile_t* tilemap_get_tile(TileMap_t* tilemap, Half_t coord);
U8* tilemap_get_light(TileMap_t* tilemap, Half_t half);
bool tilemap_is_solid(TileMap_t* tilemap, Coord_t coord);
bool tilemap_is_iced(TileMap_t* tilemap, Coord_t coord);
void tilemap_set_ice(TileMap_t* tilemap, Half_t half, bool value);
bool tilemap_get_ice(TileMap_t* tilemap, Half_t half);
bool tile_flags_have_ice(U32 flags);
Direction_t tile_flags_cluster_direction(U32 flags);
void tile_flags_set_cluster_direction(U32* flags, Direction_t dir);
bool tile_flags_cluster_all_on(U32 flags);

#endif
