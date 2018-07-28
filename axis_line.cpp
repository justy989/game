#include "axis_line.h"
#include "defines.h"

#include <math.h>

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

bool axis_line_intersects_circle(AxisLine_t a, Pixel_t p, F32 radius){
     Pixel_t a_p {};

     if(a.vertical){
          a_p.x = a.offset;
          a_p.y = p.y;
          CLAMP(a_p.y, a.min, a.max);
     }else{
          a_p.y = a.offset;
          a_p.x = p.x;
          CLAMP(a_p.x, a.min, a.max);
     }

     F32 x_diff = p.x - a_p.x;
     F32 y_diff = p.y - a_p.y;
     F32 distance = sqrt((x_diff * x_diff) + (y_diff * y_diff));

     return radius > distance;
}
