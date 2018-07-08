#include "axis_line.h"

static bool same_intersect(AxisLine_t a, AxisLine_t b){
     if(a.offset == b.offset){
          if(a.min >= b.min && a.min <= b.max) return true;
          if(a.max >= b.min && a.max <= b.max) return true;
     }

     return false;
}

static bool different_intersect(AxisLine_t a, AxisLine_t b){
     return (a.offset >= b.min && a.offset <= b.max &&
             b.offset >= a.min && b.offset <= a.max);
}

bool axis_lines_intersect(AxisLine_t a, AxisLine_t b){
     if(a.vertical){
          if(b.vertical){
               return same_intersect(a, b);
          }

          return different_intersect(a, b);
     }else{
          if(b.vertical){
               return different_intersect(a, b);
          }
     }

     return same_intersect(a, b);
}
