#pragma once

#include <stdlib.h>

#include "types.h"
#include "log.h"

template <typename T>
struct ObjectArray_t{
     T* elements;
     S16 count;
};

template <typename T>
bool init(ObjectArray_t<T>* object_array, S16 count){
     object_array->elements = (T*)(calloc(count, sizeof(*object_array->elements)));
     if(!object_array->elements){
          LOG("%s() failed to calloc %d objects\n", __FUNCTION__, count);
          return false;
     }
     object_array->count = count;
     return true;
}

// this only works when T is shallow copy-able
template <typename T>
void deep_copy(ObjectArray_t<T>* a, ObjectArray_t<T>* b){
     destroy(b);
     init(b, a->count);
     for(S16 i = 0; i < a->count; i++){
          b->elements[i] = a->elements[i];
     }
}

template <typename T>
bool resize(ObjectArray_t<T>* object_array, S16 new_count){
     if(new_count == 0){
          destroy(object_array);
          return true;
     }
     object_array->elements = (T*)(realloc(object_array->elements, new_count * sizeof(*object_array->elements)));
     if(!object_array->elements){
          LOG("%s() failed to realloc %d objects\n", __FUNCTION__, new_count);
          return false;
     }
     object_array->count = new_count;
     return true;
}

template <typename T>
bool remove(ObjectArray_t<T>* object_array, S16 index){
     if(index < 0 || index >= object_array->count) return false;
     if(object_array->count > 1){
          // move the last element to the index of the element that we want to remove
          object_array->elements[index] = object_array->elements[object_array->count - 1];
          resize(object_array, object_array->count - (S16)(1));
     }else if(object_array->count == 1){
          destroy(object_array);
     }
     return true;
}

template <typename T>
void destroy(ObjectArray_t<T>* object_array){
     free(object_array->elements);
     object_array->elements = nullptr;
     object_array->count = 0;
}

template <typename T>
bool shallow_copy(ObjectArray_t<T>* a, ObjectArray_t<T>* b){
     b->elements = (T*)(malloc(a->count * sizeof(*b->elements)));
     if(!b->elements){
          LOG("%s() failed to realloc %d objects\n", __FUNCTION__, a->count);
          return false;
     }
     for(S16 i = 0; i < a->count; i++){
          b->elements[i] = a->elements[i];
     }
     b->count = a->count;
     return true;
}
