#pragma once

#include "vec.h"

struct Block_t;

struct CarriedPosDelta_t{
     Vec_t positive;
     Vec_t negative;
     S16   block_index = -1;
};

void carried_pos_delta_reset(CarriedPosDelta_t* carried_pos_delta);
bool get_carried_noob(CarriedPosDelta_t* carried_pos_delta, Vec_t carry_vec, S16 block_index, bool through_entangler);
