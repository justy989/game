#pragma once

#include "types.h"
#include "coord.h"

#define TILE_ID_SOLID_START 16

enum TileFlag_t : U16{
     TILE_FLAG_ICED = 1,
     TILE_FLAG_CHECKPOINT = 2,
     TILE_FLAG_RESET_IMMUNE = 4,
     TILE_FLAG_WIRE_LEFT_OFF = 8,
     TILE_FLAG_WIRE_UP_OFF = 16,
     TILE_FLAG_WIRE_RIGHT_OFF = 32,
     TILE_FLAG_WIRE_DOWN_OFF = 64,
     TILE_FLAG_WIRE_LEFT_ON = 128,
     TILE_FLAG_WIRE_UP_ON = 256,
     TILE_FLAG_WIRE_RIGHT_ON = 512,
     TILE_FLAG_WIRE_DOWN_ON = 1024,
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
Tile_t* tilemap_get_tile(TileMap_t* tilemap, Coord_t coord);
bool tilemap_is_solid(TileMap_t* tilemap, Coord_t coord);
bool tilemap_is_iced(TileMap_t* tilemap, Coord_t coord);
