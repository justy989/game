#ifndef HALF_H
#define HALF_H

#include "types.h"
#include "direction.h"

struct Half_t{
     S16 x;
     S16 y;
};

Half_t operator+(Half_t a, Half_t b);
Half_t operator-(Half_t a, Half_t b);

void operator+=(Half_t& a, Half_t b);
void operator-=(Half_t& a, Half_t b);

bool operator==(Half_t a, Half_t b);
bool operator!=(Half_t a, Half_t b);

Half_t operator+(Half_t c, Direction_t dir);
Half_t operator-(Half_t c, Direction_t dir);
void operator+=(Half_t& c, Direction_t dir);
void operator-=(Half_t& c, Direction_t dir);

#endif
