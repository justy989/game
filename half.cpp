#include "half.h"

#include <cassert>

static Half_t half_move(Half_t h, Direction_t dir, S16 distance){
     switch(dir){
     default:
          assert(!"invalid direction");
          break;
     case DIRECTION_LEFT:
          h.x -= distance;
          break;
     case DIRECTION_UP:
          h.y += distance;
          break;
     case DIRECTION_RIGHT:
          h.x += distance;
          break;
     case DIRECTION_DOWN:
          h.y -= distance;
          break;
     }

     return h;
}

Half_t operator+(Half_t a, Half_t b){return Half_t{(S16)(a.x + b.x), (S16)(a.y + b.y)};}
Half_t operator-(Half_t a, Half_t b){return Half_t{(S16)(a.x - b.x), (S16)(a.y - b.y)};}

void operator+=(Half_t& a, Half_t b){a.x += b.x; a.y += b.y;}
void operator-=(Half_t& a, Half_t b){a.x -= b.x; a.y -= b.y;}

bool operator==(Half_t a, Half_t b){return (a.x == b.x && a.y == b.y);}
bool operator!=(Half_t a, Half_t b){return (a.x != b.x || a.y != b.y);}

Half_t operator+(Half_t h, Direction_t dir){return half_move(h, dir, 1);}
Half_t operator-(Half_t h, Direction_t dir){return half_move(h, direction_opposite(dir), 1);}

void operator+=(Half_t& h, Direction_t dir){h = half_move(h, dir, 1);}
void operator-=(Half_t& h, Direction_t dir){h = half_move(h, direction_opposite(dir), 1);}
