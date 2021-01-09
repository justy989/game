#pragma once

#include "position.h"
#include "tile.h"
#include "quad.h"
#include "rect.h"

struct Camera_t{
     Position_t pos;
     Position_t view_pos;
     Vec_t world_offset;
     Vec_t center_offset;

     F32 view_dimension;
     Quad_t view;

     Position_t bottom_left();
     void center_on_tilemap(TileMap_t* tilemap);
     void center_on_room(Rect_t* rect);

     Vec_t normalized_to_world(Vec_t v);
};
