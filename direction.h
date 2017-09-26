#pragma once

#include "types.h"

enum Direction_t : U8{
     DIRECTION_LEFT = 0,
     DIRECTION_UP = 1,
     DIRECTION_RIGHT = 2,
     DIRECTION_DOWN = 3,
     DIRECTION_COUNT = 4,
};

enum DirectionMask_t : U8{
     DIRECTION_MASK_NONE = 0,
     DIRECTION_MASK_LEFT = 1,
     DIRECTION_MASK_UP = 2,
     DIRECTION_MASK_RIGHT = 4,
     DIRECTION_MASK_DOWN = 8,
     DIRECTION_MASK_ALL = 15,
};

// 0  none
// 1  left
// 2  up
// 3  left | up
// 4  right
// 5  right | left
// 6  right | up
// 7  right | left | up
// 8  down
// 9  down | left
// 10 down | up
// 11 down | left | up
// 12 down | right
// 13 down | right | left
// 14 down | right | up
// 15 down | left | up | right

bool direction_in_mask(DirectionMask_t mask, Direction_t dir);
DirectionMask_t direction_to_direction_mask(Direction_t dir);
DirectionMask_t direction_mask_add(DirectionMask_t a, DirectionMask_t b);
DirectionMask_t direction_mask_add(DirectionMask_t a, int b);
DirectionMask_t direction_mask_add(DirectionMask_t mask, Direction_t dir);
DirectionMask_t direction_mask_remove(DirectionMask_t a, DirectionMask_t b);
DirectionMask_t direction_mask_remove(DirectionMask_t a, int b);
DirectionMask_t direction_mask_remove(DirectionMask_t mask, Direction_t dir);
Direction_t direction_opposite(Direction_t dir);
bool direction_is_horizontal(Direction_t dir);
U8 direction_rotations_between(Direction_t a, Direction_t b);
Direction_t direction_rotate_clockwise(Direction_t dir);
Direction_t direction_rotate_clockwise(Direction_t dir, U8 times);
DirectionMask_t direction_mask_rotate_clockwise(DirectionMask_t mask);
DirectionMask_t direction_mask_flip_horizontal(DirectionMask_t mask);
DirectionMask_t direction_mask_flip_vertical(DirectionMask_t mask);
const char* direction_to_string(Direction_t dir);
