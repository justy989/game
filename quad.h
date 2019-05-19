#pragma once

#include "types.h"

struct Quad_t{
     F32 left;
     F32 bottom;
     F32 right;
     F32 top;
};

bool quad_in_quad(const Quad_t* a, const Quad_t* b);

// both quads have the property where the right and top parts exceed the area that we
// care to check, but any value below those boundaries are inside the quad
bool quad_in_quad_high_range_exclusive(const Quad_t* a, const Quad_t* b);

bool operator==(const Quad_t& a, const Quad_t& b);
