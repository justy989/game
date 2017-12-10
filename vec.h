#ifndef VEC_H
#define VEC_H

#include "types.h"

struct Vec_t{
     F32 x;
     F32 y;
};

Vec_t operator+(Vec_t a, Vec_t b);
Vec_t operator-(Vec_t a, Vec_t b);

void operator+=(Vec_t& a, Vec_t b);
void operator-=(Vec_t& a, Vec_t b);

Vec_t operator*(Vec_t a, F32 s);
void operator*=(Vec_t& a, F32 s);

float vec_dot(Vec_t a, Vec_t b);

Vec_t vec_negate(Vec_t a);
F32 vec_magnitude(Vec_t v);

Vec_t vec_normalize(Vec_t a);

Vec_t vec_zero();
Vec_t vec_left();
Vec_t vec_right();
Vec_t vec_up();
Vec_t vec_down();

void vec_set(Vec_t* v, float x, float y);
void vec_move(Vec_t* v, float dx, float dy);
void vec_move_x(Vec_t* v, float dx);
void vec_move_y(Vec_t* v, float dy);

Vec_t vec_project_onto(Vec_t a, Vec_t b);

#endif
