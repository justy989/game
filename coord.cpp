#include "coord.h"
#include "defines.h"

#include <cmath>
#include <cassert>

Coord_t coord_zero(){return Coord_t{0, 0};}
Coord_t coord_move(Coord_t c, Direction_t dir, S16 distance){
     switch ( dir ) {
     default:
          assert(!"invalid direction");
          break;
     case DIRECTION_LEFT:
          c.x -= distance;
          break;
     case DIRECTION_UP:
          c.y += distance;
          break;
     case DIRECTION_RIGHT:
          c.x += distance;
          break;
     case DIRECTION_DOWN:
          c.y -= distance;
          break;
     }

     return c;
}

Coord_t coord_clamp_zero_to_dim(Coord_t c, S16 width, S16 height){
     CLAMP(c.x, 0, width);
     CLAMP(c.y, 0, height);
     return c;
}

Coord_t operator+(Coord_t a, Coord_t b){return Coord_t{(S16)(a.x + b.x), (S16)(a.y + b.y)};}
Coord_t operator-(Coord_t a, Coord_t b){return Coord_t{(S16)(a.x - b.x), (S16)(a.y - b.y)};}

void operator+=(Coord_t& a, Coord_t b){a.x += b.x; a.y += b.y;}
void operator-=(Coord_t& a, Coord_t b){a.x -= b.x; a.y -= b.y;}

bool operator==(Coord_t a, Coord_t b){return (a.x == b.x && a.y == b.y);}
bool operator!=(Coord_t a, Coord_t b){return (a.x != b.x || a.y != b.y);}

Coord_t operator+(Coord_t c, Direction_t dir){return coord_move(c, dir, 1);}
Coord_t operator-(Coord_t c, Direction_t dir){return coord_move(c, direction_opposite(dir), 1);}
void operator+=(Coord_t& c, Direction_t dir){c = coord_move(c, dir, 1);}
void operator-=(Coord_t& c, Direction_t dir){c = coord_move(c, direction_opposite(dir), 1);}

void coord_set(Coord_t* c, S16 x, S16 y){c->x = x; c->y = y;}
void coord_move(Coord_t* c, S16 dx, S16 dy){c->x += dx; c->y += dy;}
void coord_move_x(Coord_t* c, S16 dx){c->x += dx;};
void coord_move_y(Coord_t* c, S16 dy){c->y += dy;}

bool coord_after(Coord_t a, Coord_t b){return b.y < a.y || (b.y == a.y && b.x < a.x);}

void coords_surrounding(Coord_t* coords, S16 coord_count, Coord_t center){
     assert(coord_count >= SURROUNDING_COORD_COUNT);
     S8 index = 0;
     for(S8 x = -1; x <= 1; x++){
          for(S8 y = -1; y <= 1; y++){
               if(x == 0 && y == 0) continue;
               coords[index] = center + Coord_t{x, y};
               index++;
          }
     }
}
