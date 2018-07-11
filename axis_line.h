#pragma once

#include "pixel.h"

struct AxisLine_t{
     bool vertical; // or horizontal
     S16 offset;
     S16 min;
     S16 max;
};

bool axis_lines_intersect(AxisLine_t a, AxisLine_t b);
bool axis_line_intersects_circle(AxisLine_t a, Pixel_t p, F32 radius);
