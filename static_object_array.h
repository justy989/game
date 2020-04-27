#pragma once

#include "types.h"

template < typename T, S32 N >
struct StaticObjectArray_t{
     T objects[N];
     S32 count = 0;

     bool insert(const T* obj){
          if(count >= N){
               assert(!"ya dun messed up A-ARON");
               return false;
          }
          objects[count] = *obj;
          count++;
          return true;
     }
};
