#pragma once

#include "position.h"
#include "tile.h"

struct Camera_t{
     Position_t pos;
     Vec_t view_dimensions;

     Position_t bottom_left();
     void center_on_tilemap(TileMap_t* tilemap);
};
