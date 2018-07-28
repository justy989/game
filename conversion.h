#pragma once

#include "position.h"
#include "coord.h"

Coord_t vec_to_coord(Vec_t v);
Vec_t coord_to_vec(Coord_t c);
Vec_t pos_to_vec(Position_t p);
Coord_t pixel_to_coord(Pixel_t p);
Coord_t pos_to_coord(Position_t p);
Pixel_t coord_to_pixel(Coord_t c);
Pixel_t coord_to_pixel_at_center(Coord_t c);
Position_t coord_to_pos_at_tile_center(Coord_t c);
Position_t coord_to_pos(Coord_t c);
Position_t vec_to_pos(Vec_t v);
Position_t pixel_to_pos(Pixel_t v);
DirectionMask_t directions_between(Coord_t a, Coord_t b);
Direction_t relative_quadrant(Pixel_t a, Pixel_t b);
