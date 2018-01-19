#ifndef ARROW_H
#define ARROW_H

#include "position.h"
#include "direction.h"
#include "element.h"

#define ARROW_DISINTEGRATE_DELAY 4.0f
#define ARROW_SHOOT_HEIGHT 7
#define ARROW_FALL_DELAY 2.0f

class Block_t;
class Interactive_t;
class Player_t;

enum StickType_t{
     STICK_TYPE_NONE,
     STICK_TYPE_POPUP,
     STICK_TYPE_BLOCK,
     STICK_TYPE_PLAYER,
};

struct Arrow_t{
     Position_t pos;
     Direction_t face;
     Element_t element;
     F32 vel;
     S16 element_from_block;

     bool alive;
     F32 fall_time;

     F32 stick_time; // TODO: track objects we are stuck in
     Position_t stick_offset;
     StickType_t stick_type;
     union{
          Block_t* stick_to_block;
          Interactive_t* stick_to_popup;
          Player_t* stick_to_player;
     };
};

#define ARROW_ARRAY_MAX 32

struct ArrowArray_t{
     Arrow_t arrows[ARROW_ARRAY_MAX];
};

bool init(ArrowArray_t* arrow_array);
Arrow_t* arrow_spawn(ArrowArray_t* arrow_array, Position_t pos, Direction_t face);

#endif
