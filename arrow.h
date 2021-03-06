#pragma once

#include "position.h"
#include "direction.h"
#include "element.h"

#define ARROW_DISINTEGRATE_DELAY 12.0f
#define ARROW_SHOOT_HEIGHT 7
#define ARROW_FALL_DELAY 6.0f

struct Block_t;
struct Interactive_t;

enum StuckType_t{
     STUCK_NONE,
     STUCK_BLOCK,
     STUCK_POPUP,
     STUCK_DOOR,
};

struct Arrow_t{
     Position_t pos;
     Direction_t face;
     Element_t element;
     F32 vel;
     S16 element_from_block;

     bool alive;
     F32 fall_time;

     StuckType_t stuck_type;
     F32 stuck_time; // TODO: track objects we are stuck in
     Position_t stuck_offset;
     S16 stuck_index = 0;

     bool spawned_this_frame = false;
     S16 entangle_index = -1;
};

#define ARROW_ARRAY_MAX 32

struct ArrowArray_t{
     Arrow_t arrows[ARROW_ARRAY_MAX];
};

bool init(ArrowArray_t* arrow_array);
Arrow_t* arrow_spawn(ArrowArray_t* arrow_array, Position_t pos, Direction_t face);
void init_arrow(Arrow_t* arrow);
