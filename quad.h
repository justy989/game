#pragma once

#include "types.h"

struct Quad_t{
     F32 left;
     F32 bottom;
     F32 right;
     F32 top;
};

bool quad_in_quad(const Quad_t* a, const Quad_t* b);
