#pragma once

#include "types.h"
#include "direction.h"

struct Coord_t{
     S16 x;
     S16 y;
};

Coord_t coord_zero();
Coord_t coord_move(Coord_t c, Direction_t dir, S16 distance);
Coord_t coord_clamp_zero_to_dim(Coord_t c, S16 width, S16 height);

Coord_t operator+(Coord_t a, Coord_t b);
Coord_t operator-(Coord_t a, Coord_t b);

void operator+=(Coord_t& a, Coord_t b);
void operator-=(Coord_t& a, Coord_t b);

bool operator==(Coord_t a, Coord_t b);
bool operator!=(Coord_t a, Coord_t b);

Coord_t operator+(Coord_t c, Direction_t dir);
Coord_t operator-(Coord_t c, Direction_t dir);
void operator+=(Coord_t& c, Direction_t dir);
void operator-=(Coord_t& c, Direction_t dir);

void coord_set(Coord_t* c, S16 x, S16 y);
void coord_move(Coord_t* c, S16 dx, S16 dy);
void coord_move_x(Coord_t* c, S16 dx);
void coord_move_y(Coord_t* c, S16 dy);

bool coord_after(Coord_t a, Coord_t b);
