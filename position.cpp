#include "position.h"
#include "defines.h"

#include <cmath>

Position_t pixel_pos(Pixel_t pixel){
     Position_t p {};
     p.pixel = pixel;
     p.decimal = {0.0f, 0.0f};
     p.z = 0;
     return p;
}

void canonicalize(Position_t* position){
     if(position->decimal.x >= PIXEL_SIZE){
          F32 pixels = (F32)(floor(position->decimal.x / PIXEL_SIZE));
          position->pixel.x += (S16)(pixels);
          position->decimal.x = (F32)(fmod(position->decimal.x, PIXEL_SIZE));
     }else if(position->decimal.x < 0.0f){
          F32 pixels = (F32)(floor(position->decimal.x / PIXEL_SIZE));
          position->pixel.x += (S16)(pixels);
          position->decimal.x = (F32)(fmod(position->decimal.x, PIXEL_SIZE));
          if(position->decimal.x < 0.0f) position->decimal.x += PIXEL_SIZE;
          else if(position->decimal.x == -0.0f) position->decimal.x = 0.0f;
     }

     if(position->decimal.y >= PIXEL_SIZE){
          F32 pixels = (F32)(floor(position->decimal.y / PIXEL_SIZE));
          position->pixel.y += (S16)(pixels);
          position->decimal.y = (F32)(fmod(position->decimal.y, PIXEL_SIZE));
     }else if(position->decimal.y < 0.0f){
          F32 pixels = (F32)(floor(position->decimal.y / PIXEL_SIZE));
          position->pixel.y += (S16)(pixels);
          position->decimal.y = (F32)(fmod(position->decimal.y, PIXEL_SIZE));
          if(position->decimal.y < 0.0f) position->decimal.y += PIXEL_SIZE;
          else if(position->decimal.y == -0.0f) position->decimal.y = 0.0f;
     }
}

void negate(Position_t* position){
     position->pixel.x = -position->pixel.x;
     position->pixel.y = -position->pixel.y;
     position->decimal.x = -position->decimal.x;
     position->decimal.y = -position->decimal.y;
     canonicalize(position);
}

Position_t operator+(Position_t p, Vec_t v){
     p.decimal += v;
     canonicalize(&p);
     return p;
}

Position_t operator-(Position_t p, Vec_t v){
     p.decimal -= v;
     canonicalize(&p);
     return p;
}

Position_t operator+(Position_t p, Pixel_t i){
     p.pixel += i;
     return p;
}

Position_t operator-(Position_t p, Pixel_t i){
     p.pixel -= i;
     return p;
}

void operator+=(Position_t& p, Vec_t v){
     p.decimal += v;
     canonicalize(&p);
}

void operator-=(Position_t& p, Vec_t v){
     p.decimal -= v;
     canonicalize(&p);
}

Position_t operator+(Position_t a, Position_t b){
     Position_t p {};

     p.pixel = a.pixel + b.pixel;
     p.decimal = a.decimal + b.decimal;
     p.z = a.z + b.z;

     canonicalize(&p);
     return p;
}

Position_t operator-(Position_t a, Position_t b){
     Position_t p {};

     p.pixel = a.pixel - b.pixel;
     p.decimal = a.decimal - b.decimal;
     p.z = a.z - b.z;

     canonicalize(&p);
     return p;
}

void operator+=(Position_t& a, Position_t b){
     a.pixel += b.pixel;
     a.decimal += b.decimal;
     canonicalize(&a);
}

void operator-=(Position_t& a, Position_t b){
     a.pixel -= b.pixel;
     a.decimal -= b.decimal;
     canonicalize(&a);
}

// NOTE: only call with small positions < 1
Position_t operator*(Position_t p, float scale){
     float x_value = (float)(p.pixel.x) * PIXEL_SIZE + p.decimal.x;
     x_value *= scale;
     p.pixel.x = 0;
     p.decimal.x = x_value;

     float y_value = (float)(p.pixel.y) * PIXEL_SIZE + p.decimal.y;
     y_value *= scale;
     p.pixel.y = 0;
     p.decimal.y = y_value;

     canonicalize(&p);
     return p;
}

F32 pos_x_unit(Position_t p){return (F32)(p.pixel.x) * PIXEL_SIZE + p.decimal.x;}
F32 pos_y_unit(Position_t p){return (F32)(p.pixel.y) * PIXEL_SIZE + p.decimal.y;}

F32 distance_between(Position_t a, Position_t b){
     Position_t diff = b - a;
     F32 x_diff = (diff.pixel.x) * PIXEL_SIZE + diff.decimal.x;
     F32 y_diff = (diff.pixel.y) * PIXEL_SIZE + diff.decimal.y;
     return sqrt((x_diff * x_diff) + (y_diff * y_diff));
}

static void position_decanonicalize_negatives(Positon_t* p) {
     // decanonicalize negative numbers, so that lengths work
     if (p->pixel.x < 0) {
          p->pixel.x++;
          p->decimal.x = PIXEL_SIZE - p->decimal.x;
     }
     if (p->pixel.y < 0) {
          p->pixel.y++;
          p->decimal.y = PIXEL_SIZE - p->decimal.x;
     }
}

PositionLength_t position_length(Position_t p){
     position_decanonicalize_negatives(&p);
     // do everything in F64 before finally converting to F32 at the end to maximize precision during calculations
     F64 c_squared = sqrt((F64)(p.pixel.x * p.pixel.x) + (F64)(p.pixel.y * p.pixel.y));
     S16 integer_length = (S16)(c_squared);
     F64 decimal_length = fmod(c_squared, 1.0) * PIXEL_SIZE;
     decimal_length += sqrt((F64)(p.decimal.x * p.decimal.x) + (F64)(p.decimal.y * p.decimal.y));
     if (decimal_length >= PIXEL_SIZE) {
          F64 pixels = floor(decimal_length / PIXEL_SIZE);
          integer_length += (S16)(pixels);
          decimal_length = fmod(decimal_length, PIXEL_SIZE);
     }
     return PositionLength_t{integer_length, (F32)(decimal_length)};
}

PositionLength_t position_length(F32 p){
     S16 integer = floor(p / PIXEL_SIZE);
     F64 decimal = fmod(p, PIXEL_SIZE);
     return PositionLength_t{integer, (F32)(decimal)};
}

F32 position_length_length(PositionLength_t l){
     return (l.integer * PIXEL_SIZE) + l.decimal;
}

bool position_x_less_than(Position_t a, Position_t b){
     if(a.pixel.x < b.pixel.x) return true;
     if(a.pixel.x == b.pixel.x){
          if(a.decimal.x < b.decimal.x) return true;
     }
     return false;
}

bool position_x_greater_than(Position_t a, Position_t b){
     if(a.pixel.x > b.pixel.x) return true;
     if(a.pixel.x == b.pixel.x){
          if(a.decimal.x > b.decimal.x) return true;
     }
     return false;
}

bool position_y_less_than(Position_t a, Position_t b){
     if(a.pixel.y < b.pixel.y) return true;
     if(a.pixel.y == b.pixel.y){
          if(a.decimal.y < b.decimal.y) return true;
     }
     return false;
}

bool position_y_greater_than(Position_t a, Position_t b){
     if(a.pixel.y > b.pixel.y) return true;
     if(a.pixel.y == b.pixel.y){
          if(a.decimal.y > b.decimal.y) return true;
     }
     return false;
}

bool operator==(Position_t a, Position_t b){
     return a.pixel == b.pixel && a.decimal == b.decimal && a.z == b.z;
}

bool operator!=(Position_t a, Position_t b){
     return a.pixel != b.pixel && a.decimal != b.decimal && a.z != b.z;
}

Position_t position_zero() {
     return Position_t{Pixel_t{0, 0}, Vec_t{0, 0}, 0};
}

bool operator>(PositionLength_t a, PositionLength_t b){
     if(a.integer > b.integer){
          return true;
     }else if(a.integer == b.integer){
          if(a.decimal > b.decimal){
               return true;
          }
     }
     return false;
}

bool operator<(PositionLength_t a, PositionLength_t b){
     if(a.integer < b.integer){
          return true;
     }else if(a.integer == b.integer){
          if(a.decimal < b.decimal){
               return true;
          }
     }
     return false;
}
