#pragma once

#include "types.h"

struct Pixel_t{
     S16 x;
     S16 y;
};

Pixel_t operator+(Pixel_t a, Pixel_t b);
Pixel_t operator-(Pixel_t a, Pixel_t b);
void operator+=(Pixel_t& a, Pixel_t b);
void operator-=(Pixel_t& a, Pixel_t b);
bool operator!=(Pixel_t a, Pixel_t b);
bool operator==(Pixel_t a, Pixel_t b);
