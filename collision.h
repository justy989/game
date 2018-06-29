#pragma once

#include "rect.h"
#include "position.h"

Vec_t collide_circle_with_line(Vec_t circle_center, F32 circle_radius, Vec_t a, Vec_t b, bool* collided);
void position_collide_with_rect(Position_t pos, Position_t rect_pos, Vec_t* pos_delta, bool* collide_with_block);
void position_slide_against_rect(Position_t pos, Coord_t coord, F32 player_radius, Vec_t* pos_delta, bool* collide_with_coord);
