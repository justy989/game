#include "axis_line.h"

bool axis_lines_intersect(AxisLine_t a, AxisLine_t b){
     if(a.vertical){
          if(b.vertical){
               if(a.offset != b.offset) return false;

               if(a.min >= b.min && a.min <= b.max) return true;
               if(a.max >= b.min && a.max <= b.max) return true;
          }else{
               if(a.offset >= b.min && a.offset <= b.max &&
                  b.offset >= a.min && b.offset <= a.max){
                    return true;
               }
          }
     }else{
          if(b.vertical){
               if(a.offset >= b.min && a.offset <= b.max &&
                  b.offset >= a.min && b.offset <= a.max){
                    return true;
               }
          }else{
               if(a.offset != b.offset) return false;

               if(a.min >= b.min && a.min <= b.max) return true;
               if(a.max >= b.min && a.max <= b.max) return true;
          }
     }

     return false;
}
