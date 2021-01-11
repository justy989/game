#pragma once

#include "position.h"
#include "tile.h"
#include "quad.h"
#include "rect.h"

struct Camera_t{
     Quad_t view;
     Position_t offset;

     Quad_t initial_view;
     Position_t initial_offset;

     Quad_t target_view;
     Position_t target_offset;

     void center_on_tilemap(TileMap_t* tilemap);
     void center_on_room(Rect_t* rect);

     Position_t normalized_to_world(Vec_t v);

     void move_towards_target(F32 speed);

     Rect_t coords_in_view();
     Rect_t coords_in_target_view();
};
