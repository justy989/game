#pragma once

#include "axis_line.h"
#include "coord.h"

struct Rect_t{
     S16 left;
     S16 bottom;

     S16 right;
     S16 top;
};

bool xy_in_rect(const Rect_t& rect, S16 x, S16 y);
bool pixel_in_rect(Pixel_t p, Rect_t r);
bool coord_in_rect(Coord_t c, Rect_t r);
bool rect_in_rect(Rect_t a, Rect_t b);
bool axis_line_intersects_rect(AxisLine_t l, Rect_t r);

S16 rect_area(Rect_t a);
S16 rect_intersecting_area(Rect_t a, Rect_t b);

bool rect_completely_in_rect(Rect_t a, Rect_t b);
