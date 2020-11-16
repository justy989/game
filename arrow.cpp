#include "arrow.h"

bool init(ArrowArray_t* arrow_array){
     for (auto &arrow : arrow_array->arrows) {
          init_arrow(&arrow);
     }

     return true;
}

Arrow_t* arrow_spawn(ArrowArray_t* arrow_array, Position_t pos, Direction_t face){
     for(auto& arrow : arrow_array->arrows){
          if(!arrow.alive){
               init_arrow(&arrow);
               arrow.pos = pos;
               arrow.face = face;
               arrow.alive = true;
               return &arrow;
          }
     }

     return nullptr;
}

void init_arrow(Arrow_t* arrow){
     arrow->pos = Position_t{};
     arrow->face = DIRECTION_COUNT;
     arrow->alive = false;
     arrow->spawned_this_frame = false;
     arrow->entangle_index = -1;
     arrow->fall_time = 0.0f;
     arrow->stuck_time = 0.0f;
     arrow->stuck_type = STUCK_NONE;
     arrow->stuck_offset = {};
     arrow->element = ELEMENT_NONE;
     arrow->vel = 1.25f;
}
