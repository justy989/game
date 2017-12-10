#ifndef RECT_H
#define RECT_H

#include "pixel.h"
#include "coord.h"

struct Rect_t{
     S16 left;
     S16 bottom;

     S16 right;
     S16 top;
};

bool xy_in_rect(const Rect_t& rect, S16 x, S16 y);
Rect_t pixel_range(Pixel_t bottom_left, Pixel_t top_right);
bool pixel_in_rect(Pixel_t p, Rect_t r);
Rect_t coord_range(Coord_t bottom_left, Coord_t top_right);
bool coord_in_rect(Coord_t c, Rect_t r);
bool rect_in_rect(Rect_t a, Rect_t b);

#endif
