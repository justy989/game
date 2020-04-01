#pragma once

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

F32 vec_magnitude(Vec_t v);

Vec_t vec_normalize(Vec_t a);

Vec_t vec_zero();

Vec_t vec_project_onto(Vec_t a, Vec_t b);

bool operator !=(const Vec_t& a, const Vec_t& b);
bool operator ==(const Vec_t& a, const Vec_t& b);
