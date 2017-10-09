#pragma once

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

template <typename T>
bool resize(ObjectArray_t<T>* object_array, S16 new_count){
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
          resize(object_array, object_array->count - 1);
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
