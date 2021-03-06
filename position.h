#pragma once

#include "pixel.h"
#include "vec.h"

struct Position_t{
     Pixel_t pixel;
     Vec_t decimal;
     S8 z; // TODO: rename to height
};

Position_t pixel_pos(Pixel_t pixel);
void canonicalize(Position_t* position);
void negate(Position_t* position);
Position_t operator+(Position_t p, Vec_t v);
Position_t operator-(Position_t p, Vec_t v);
Position_t operator+(Position_t p, Pixel_t i);
Position_t operator-(Position_t p, Pixel_t i);
void operator+=(Position_t& p, Vec_t v);
void operator-=(Position_t& p, Vec_t v);
Position_t operator+(Position_t a, Position_t b);
Position_t operator-(Position_t a, Position_t b);
void operator+=(Position_t& a, Position_t b);
void operator-=(Position_t& a, Position_t b);
bool operator==(Position_t a, Position_t b);
Position_t operator*(Position_t p, float scale);
F32 pos_x_unit(Position_t p);
F32 pos_y_unit(Position_t p);
F32 distance_between(Position_t a, Position_t b);

bool position_x_less_than(Position_t a, Position_t b);
bool position_x_greater_than(Position_t a, Position_t b);
bool position_y_less_than(Position_t a, Position_t b);
bool position_y_greater_than(Position_t a, Position_t b);
