#include "centroid.h"
#include "utils.h"

Coord_t find_centroid(CentroidStart_t a, CentroidStart_t b){
     // find the missing corners of the rectangle
     Coord_t rect_corner_a;
     rect_corner_a.x = a.coord.x;
     rect_corner_a.y = b.coord.y;

     Coord_t rect_corner_b;
     rect_corner_b.x = b.coord.x;
     rect_corner_b.y = a.coord.y;

     // find the formula for the diagonals that cross of those corners
     // y = mx + b where m = 1 and -1

     // b1 = y - mx
     // b2 = y + mx

     S16 a_b_one = rect_corner_a.y - rect_corner_a.x;
     S16 a_b_two = rect_corner_a.y + rect_corner_a.x;

     S16 b_b_one = rect_corner_b.y - rect_corner_b.x;
     S16 b_b_two = rect_corner_b.y + rect_corner_b.x;

     // find the intersection of our 2 formulas
     // ay = max + ab
     // by = mbx + bb

     // 1x + ab1 = -1x + bb2
     // 2x = bb2 - ab1
     // x = (bb2 - ab1) / 2

     // there are 2 intersections, calculate them both
     Coord_t first_option {};
     first_option.x = (b_b_two - a_b_one) / 2;
     first_option.y = first_option.x + a_b_one;

     // -1x + ab2 = 1x + bb2
     // 2x = ab2 - bb1
     // x = (ab2 - bb1) / 2

     Coord_t second_option {};
     second_option.x = (a_b_two - b_b_one) / 2;
     second_option.y = -second_option.x + a_b_two;

     DirectionMask_t a_mask = coord_direction_mask_between(a.coord, first_option);
     DirectionMask_t b_mask = coord_direction_mask_between(b.coord, first_option);

     // find the intersection that they are both facing towards or away from
     bool a_facing = direction_in_mask(a_mask, a.direction);
     bool b_facing = direction_in_mask(b_mask, b.direction);

     bool a_facing_away = direction_in_mask(a_mask, direction_opposite(a.direction));
     bool b_facing_away = direction_in_mask(b_mask, direction_opposite(b.direction));

     char a_m[128];
     char b_m[128];
     direction_mask_to_string(a_mask, a_m, 128);
     direction_mask_to_string(b_mask, b_m, 128);

     if((a_facing && b_facing) || (a_facing_away && b_facing_away)){
          return first_option;
     }

     a_mask = coord_direction_mask_between(a.coord, second_option);
     b_mask = coord_direction_mask_between(b.coord, second_option);

     a_facing = direction_in_mask(a_mask, a.direction);
     b_facing = direction_in_mask(b_mask, b.direction);

     a_facing_away = direction_in_mask(a_mask, direction_opposite(a.direction));
     b_facing_away = direction_in_mask(b_mask, direction_opposite(b.direction));

     direction_mask_to_string(a_mask, a_m, 128);
     direction_mask_to_string(b_mask, b_m, 128);

     if((a_facing && b_facing) || (a_facing_away && b_facing_away)){
          return second_option;
     }

     // if we still haven't found it, check if we are on the same level as any of them
     bool a_on_same_level = false;
     bool b_on_same_level = false;

     if(direction_is_horizontal(a.direction)){
          a_on_same_level = (a.coord.x == first_option.x);
     }else{
          a_on_same_level = (a.coord.y == first_option.y);
     }

     if(direction_is_horizontal(b.direction)){
          b_on_same_level = (b.coord.x == first_option.x);
     }else{
          b_on_same_level = (b.coord.y == first_option.y);
     }

     if(a_on_same_level && b_on_same_level) return first_option;

     if(direction_is_horizontal(a.direction)){
          a_on_same_level = (a.coord.x == second_option.x);
     }else{
          a_on_same_level = (a.coord.y == second_option.y);
     }

     if(direction_is_horizontal(b.direction)){
          b_on_same_level = (b.coord.x == second_option.x);
     }else{
          b_on_same_level = (b.coord.y == second_option.y);
     }

     if(a_on_same_level && b_on_same_level) return second_option;

     return Coord_t{-1, -1};
}

