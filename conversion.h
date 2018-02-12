#ifndef CONVERSION_H
#define CONVERSION_H

#include "position.h"
#include "coord.h"
#include "half.h"

Coord_t vec_to_coord(Vec_t v);
Vec_t coord_to_vec(Coord_t c);
Vec_t pos_to_vec(Position_t p);
Vec_t half_to_vec(Half_t c);
Coord_t pixel_to_coord(Pixel_t p);
Coord_t pos_to_coord(Position_t p);
Coord_t half_to_coord(Half_t h);
Pixel_t coord_to_pixel(Coord_t c);
Pixel_t half_to_pixel(Half_t h);
Pixel_t coord_to_pixel_at_center(Coord_t c);
Half_t pos_to_half(Position_t p);
Half_t vec_to_half(Vec_t v);
Half_t coord_to_half(Coord_t c);
Half_t pixel_to_half(Pixel_t c);
Position_t coord_to_pos_at_tile_center(Coord_t c);
Position_t coord_to_pos(Coord_t c);
Position_t half_to_pos(Half_t h);
Position_t vec_to_pos(Vec_t v);
Position_t pixel_to_pos(Pixel_t v);
DirectionMask_t directions_between(Coord_t a, Coord_t b);
DirectionMask_t directions_between(Pixel_t a, Pixel_t b);
Direction_t relative_quadrant(Pixel_t a, Pixel_t b);

#endif
