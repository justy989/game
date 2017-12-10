#ifndef TILE_H
#define TILE_H

#include "types.h"
#include "coord.h"

#define TILE_ID_SOLID_START 16

enum TileFlag_t : U16{
     TILE_FLAG_ICED = 1,
     TILE_FLAG_CHECKPOINT = 1 << 1,
     TILE_FLAG_RESET_IMMUNE = 1 << 2,

     TILE_FLAG_WIRE_STATE = 1 << 3,

     TILE_FLAG_WIRE_LEFT = 1 << 4,
     TILE_FLAG_WIRE_UP = 1 << 5,
     TILE_FLAG_WIRE_RIGHT = 1 << 6,
     TILE_FLAG_WIRE_DOWN = 1 << 7,

     TILE_FLAG_WIRE_CLUSTER_LEFT = 1 << 8,
     TILE_FLAG_WIRE_CLUSTER_MID = 1 << 9,
     TILE_FLAG_WIRE_CLUSTER_RIGHT = 1 << 10,

     TILE_FLAG_WIRE_CLUSTER_LEFT_ON = 1 << 11,
     TILE_FLAG_WIRE_CLUSTER_MID_ON = 1 << 12,
     TILE_FLAG_WIRE_CLUSTER_RIGHT_ON = 1 << 13,

     // last 2 bits combine to say which direction the cluster is facing
     // 00 left
     // 10 right
     // 11 up
     // 01 down
};

struct Tile_t{
     U8 id;
     U8 light;
     U16 flags;
};

struct TileMap_t{
     S16 width;
     S16 height;
     Tile_t** tiles;
};

bool init(TileMap_t* tilemap, S16 width, S16 height);
void destroy(TileMap_t* tilemap);
bool tile_is_solid(Tile_t* tile);
bool tile_is_iced(Tile_t* tile);
Tile_t* tilemap_get_tile(TileMap_t* tilemap, Coord_t coord);
bool tilemap_is_solid(TileMap_t* tilemap, Coord_t coord);
bool tilemap_is_iced(TileMap_t* tilemap, Coord_t coord);
Direction_t tile_flags_cluster_direction(U16 flags);
void tile_flags_set_cluster_direction(U16* flags, Direction_t dir);
bool tile_flags_cluster_all_on(U16 flags);

#endif
