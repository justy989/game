#include "conversion.h"
#include "defines.h"

#include <cstdlib>
#include <cassert>

Coord_t vec_to_coord(Vec_t v){
     Coord_t c;
     c.x = v.x / PIXEL_SIZE / TILE_SIZE_IN_PIXELS;
     c.y = v.y / PIXEL_SIZE / TILE_SIZE_IN_PIXELS;
     return c;
}

Vec_t coord_to_vec(Coord_t c){
     Vec_t v;
     v.x = (F32)(c.x * TILE_SIZE_IN_PIXELS) * PIXEL_SIZE;
     v.y = (F32)(c.y * TILE_SIZE_IN_PIXELS) * PIXEL_SIZE;
     return v;
}

Vec_t pos_to_vec(Position_t p){
     Vec_t v;

     v.x = (F32)(p.pixel.x) * PIXEL_SIZE + p.decimal.x;
     v.y = (F32)(p.pixel.y) * PIXEL_SIZE + p.decimal.y;

     return v;
}

Coord_t pixel_to_coord(Pixel_t p){
     Coord_t c;
     c.x = p.x / TILE_SIZE_IN_PIXELS;
     c.y = p.y / TILE_SIZE_IN_PIXELS;
     return c;
}

Coord_t pos_to_coord(Position_t p){
     assert(p.decimal.x >= 0.0f && p.decimal.y >= 0.0f);
     return pixel_to_coord(p.pixel);
}

Pixel_t coord_to_pixel(Coord_t c){
     Pixel_t p;
     p.x = c.x * TILE_SIZE_IN_PIXELS;
     p.y = c.y * TILE_SIZE_IN_PIXELS;
     return p;
}

Position_t coord_to_pos_at_tile_center(Coord_t c){
     return pixel_pos(coord_to_pixel(c) + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
}

Position_t coord_to_pos(Coord_t c){
     return pixel_pos(coord_to_pixel(c));
}

DirectionMask_t directions_between(Coord_t a, Coord_t b){
     Coord_t c = b - a;

     DirectionMask_t mask {};
     if(c.x < 0) mask = direction_mask_add(mask, DIRECTION_MASK_LEFT);
     if(c.x > 0) mask = direction_mask_add(mask, DIRECTION_MASK_RIGHT);
     if(c.y < 0) mask = direction_mask_add(mask, DIRECTION_MASK_DOWN);
     if(c.y > 0) mask = direction_mask_add(mask, DIRECTION_MASK_UP);

     return mask;
}

DirectionMask_t directions_between(Pixel_t a, Pixel_t b){
     Pixel_t c = b - a;

     DirectionMask_t mask {};
     if(c.x < 0) mask = direction_mask_add(mask, DIRECTION_MASK_LEFT);
     if(c.x > 0) mask = direction_mask_add(mask, DIRECTION_MASK_RIGHT);
     if(c.y < 0) mask = direction_mask_add(mask, DIRECTION_MASK_DOWN);
     if(c.y > 0) mask = direction_mask_add(mask, DIRECTION_MASK_UP);

     return mask;
}

// TODO: consider reversing the output of this function
Direction_t relative_quadrant(Pixel_t a, Pixel_t b){
     Pixel_t c = b - a;

     if(abs(c.x) > abs(c.y)){
          if(c.x > 0) return DIRECTION_RIGHT;
          return DIRECTION_LEFT;
     }

     if(c.y > 0) return DIRECTION_UP;
     return DIRECTION_DOWN;
}
