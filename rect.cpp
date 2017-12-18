#include "rect.h"

bool xy_in_rect(const Rect_t& rect, S16 x, S16 y){
     return (x >= rect.left && x <= rect.right &&
             y >= rect.bottom && y <= rect.top);
}

Rect_t pixel_range(Pixel_t bottom_left, Pixel_t top_right){
     Rect_t r {bottom_left.x, bottom_left.y, top_right.x, top_right.y};
     return r;
}

bool pixel_in_rect(Pixel_t p, Rect_t r){
     return xy_in_rect(r, p.x, p.y);
}

Rect_t coord_range(Coord_t bottom_left, Coord_t top_right){
     Rect_t r {bottom_left.x, bottom_left.y, top_right.x, top_right.y};
     return r;
}

bool coord_in_rect(Coord_t c, Rect_t r){
     return xy_in_rect(r, c.x, c.y);
}

bool rect_in_rect(Rect_t a, Rect_t b){
     Pixel_t top_left {a.left, a.top};
     Pixel_t top_right {a.right, a.top};
     Pixel_t bottom_left {a.left, a.bottom};
     Pixel_t bottom_right {a.right, a.bottom};
     Pixel_t center {(S16)(a.left + (a.right - a.left) / 2),
                     (S16)(a.bottom + (a.top - a.bottom) / 2),};

     if(pixel_in_rect(bottom_left, b)) return true;
     if(pixel_in_rect(top_left, b)) return true;
     if(pixel_in_rect(bottom_right, b)) return true;
     if(pixel_in_rect(top_right, b)) return true;
     if(pixel_in_rect(center, b)) return true;

     // special case if they line up, are they sliding into each other
     if(a.left == b.left){
          if(a.bottom > b.bottom && a.bottom < b.top){
               return true;
          }else if(a.top > b.bottom && a.top < b.top){
               return true;
          }
     }else if(a.top == b.top){
          if(a.left > b.left && a.left < b.right){
               return true;
          }else if(a.right > b.left && a.right < b.right){
               return true;
          }
     }

     return false;
}
