#pragma once

#include "pixel.h"
#include "vec.h"

struct Position_t{
     Pixel_t pixel;
     S8 z; // TODO: rename to height
     Vec_t decimal;
};

Position_t pixel_pos(S16 x, S16 y);
Position_t pixel_pos(Pixel_t pixel);
void canonicalize(Position_t* position);
Position_t operator+(Position_t p, Vec_t v);
Position_t operator-(Position_t p, Vec_t v);
void operator+=(Position_t& p, Vec_t v);
void operator-=(Position_t& p, Vec_t v);
Position_t operator+(Position_t a, Position_t b);
Position_t operator-(Position_t a, Position_t b);
void operator+=(Position_t& a, Position_t b);
void operator-=(Position_t& a, Position_t b);
Position_t operator*(Position_t p, float scale);
