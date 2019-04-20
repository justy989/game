#include "direction.h"
#include "defines.h"

#include <cassert>

DirectionMask_t g_direction_mask_conversion[] = {
     DIRECTION_MASK_LEFT,
     DIRECTION_MASK_UP,
     DIRECTION_MASK_RIGHT,
     DIRECTION_MASK_DOWN,
     DIRECTION_MASK_NONE,
};

bool direction_in_mask(DirectionMask_t mask, Direction_t dir){
     if(g_direction_mask_conversion[dir] & mask){
          return true;
     }

     return false;
}

DirectionMask_t direction_to_direction_mask(Direction_t dir){
     assert(dir <= DIRECTION_COUNT);
     return g_direction_mask_conversion[dir];
}

DirectionMask_t direction_mask_add(DirectionMask_t a, DirectionMask_t b){
     return (DirectionMask_t)(a | b); // C++ makes this annoying
}

DirectionMask_t direction_mask_add(DirectionMask_t mask, Direction_t dir){
     return (DirectionMask_t)(mask | direction_to_direction_mask(dir)); // C++ makes this annoying
}

DirectionMask_t direction_mask_opposite(DirectionMask_t mask){
     DirectionMask_t result = DIRECTION_MASK_NONE;

     if(mask & DIRECTION_MASK_LEFT) result = direction_mask_add(result, DIRECTION_MASK_RIGHT);
     if(mask & DIRECTION_MASK_RIGHT) result = direction_mask_add(result, DIRECTION_MASK_LEFT);
     if(mask & DIRECTION_MASK_UP) result = direction_mask_add(result, DIRECTION_MASK_DOWN);
     if(mask & DIRECTION_MASK_DOWN) result = direction_mask_add(result, DIRECTION_MASK_UP);

     return result;
}

Direction_t direction_opposite(Direction_t dir){
     if(dir == DIRECTION_COUNT) return DIRECTION_COUNT;
     return (Direction_t)(((int)(dir) + 2) % DIRECTION_COUNT);
}

U8 direction_rotations_between(Direction_t a, Direction_t b){
     if(a < b){
          return (U8)((a + DIRECTION_COUNT) - b);
     }

     return (U8)(a - b);
}

Direction_t direction_rotate_clockwise(Direction_t dir, U8 times){
     if(dir >= DIRECTION_COUNT) return dir;
     return (Direction_t)((U8)(dir + times) % DIRECTION_COUNT);
}

Direction_t direction_rotate_counter_clockwise(Direction_t dir, U8 times){
     if(dir >= DIRECTION_COUNT) return dir;
     S8 real_rot = DIRECTION_COUNT - times;
     return (Direction_t)((U8)(dir + real_rot) % DIRECTION_COUNT);
}

DirectionMask_t direction_mask_rotate_clockwise(DirectionMask_t mask){
     // TODO: could probably just shift?
     S8 rot = DIRECTION_MASK_NONE;

     if(mask & DIRECTION_MASK_LEFT) rot |= DIRECTION_MASK_UP;
     if(mask & DIRECTION_MASK_UP) rot |= DIRECTION_MASK_RIGHT;
     if(mask & DIRECTION_MASK_RIGHT) rot |= DIRECTION_MASK_DOWN;
     if(mask & DIRECTION_MASK_DOWN) rot |= DIRECTION_MASK_LEFT;

     return (DirectionMask_t)(rot);
}

const char* direction_to_string(Direction_t dir){
     switch(dir){
     default:
          break;
     CASE_ENUM_RET_STR(DIRECTION_LEFT)
     CASE_ENUM_RET_STR(DIRECTION_UP)
     CASE_ENUM_RET_STR(DIRECTION_RIGHT)
     CASE_ENUM_RET_STR(DIRECTION_DOWN)
     CASE_ENUM_RET_STR(DIRECTION_COUNT)
     }

     return "DIRECTION_UNKNOWN";
}

Direction_t direction_from_single_mask(DirectionMask_t mask){
     if(mask & DIRECTION_MASK_UP) return DIRECTION_UP;
     if(mask & DIRECTION_MASK_DOWN) return DIRECTION_DOWN;
     if(mask & DIRECTION_MASK_LEFT) return DIRECTION_LEFT;
     if(mask & DIRECTION_MASK_RIGHT) return DIRECTION_RIGHT;

     return DIRECTION_COUNT;
}

bool direction_is_horizontal(Direction_t dir){
     return dir == DIRECTION_LEFT || dir == DIRECTION_RIGHT;
}
