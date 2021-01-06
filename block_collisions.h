#pragma once

#include "block_utils.h"

struct CheckBlockCollisions_t{
     CheckBlockCollisionResult_t* collisions = NULL;
     S32 count = 0;
     S32 allocated = 0;

     bool init(S32 block_count);
     bool add_collision(CheckBlockCollisionResult_t* collision);
     void reset();
     void clear();
     void sort_by_time();
};

void apply_block_collision(World_t* world, Block_t* block, F32 dt, CheckBlockCollisionResult_t* collision_result);
