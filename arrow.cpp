#include "arrow.h"

bool init(ArrowArray_t* arrow_array){
     for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
          arrow_array->arrows[i].alive = false;
     }

     return true;
}

Arrow_t* arrow_spawn(ArrowArray_t* arrow_array, Position_t pos, Direction_t face){
     for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
          if(!arrow_array->arrows[i].alive){
               arrow_array->arrows[i].pos = pos;
               arrow_array->arrows[i].face = face;
               arrow_array->arrows[i].alive = true;
               arrow_array->arrows[i].fall_time = 0.0f;
               arrow_array->arrows[i].stuck_time = 0.0f;
               arrow_array->arrows[i].stuck_type = STUCK_NONE;
               arrow_array->arrows[i].stuck_offset = {};
               arrow_array->arrows[i].element = ELEMENT_NONE;
               arrow_array->arrows[i].vel = 1.25f;
               return arrow_array->arrows + i;
          }
     }

     return nullptr;
}
