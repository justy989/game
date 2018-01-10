#include "pixel.h"

#include <math.h>

Pixel_t operator+(Pixel_t a, Pixel_t b){
     Pixel_t p;
     p.x = a.x + b.x;
     p.y = a.y + b.y;
     return p;
}

Pixel_t operator-(Pixel_t a, Pixel_t b){
     Pixel_t p;
     p.x = a.x - b.x;
     p.y = a.y - b.y;
     return p;
}

void operator+=(Pixel_t& a, Pixel_t b){
     a.x += b.x;
     a.y += b.y;
}

void operator-=(Pixel_t& a, Pixel_t b){
     a.x -= b.x;
     a.y -= b.y;
}

bool operator!=(Pixel_t a, Pixel_t b){
     return (a.x != b.x || a.y != b.y);
}

bool operator==(Pixel_t a, Pixel_t b){
     return (a.x == b.x && a.y == b.y);
}

double pixel_distance_between(Pixel_t a, Pixel_t b){
     S16 diff_x = a.x - b.x;
     S16 diff_y = a.y - b.y;
     return sqrt(diff_x * diff_x + diff_y * diff_y);
}
