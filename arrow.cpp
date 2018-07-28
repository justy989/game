#include "arrow.h"

bool init(ArrowArray_t* arrow_array){
     for (auto &arrow : arrow_array->arrows) {
          arrow.alive = false;
     }

     return true;
}

Arrow_t* arrow_spawn(ArrowArray_t* arrow_array, Position_t pos, Direction_t face){
     for(auto& arrow : arrow_array->arrows){
          if(!arrow.alive){
               arrow.pos = pos;
               arrow.face = face;
               arrow.alive = true;
               arrow.fall_time = 0.0f;
               arrow.stuck_time = 0.0f;
               arrow.stuck_type = STUCK_NONE;
               arrow.stuck_offset = {};
               arrow.element = ELEMENT_NONE;
               arrow.vel = 1.25f;
               return &arrow;
          }
     }

     return nullptr;
}
