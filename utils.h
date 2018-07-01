#pragma once

#include "direction.h"
#include "coord.h"
#include "vec.h"
#include "position.h"
#include "quad_tree.h"
#include "tile.h"
#include "interactive.h"

Direction_t direction_between(Coord_t a, Coord_t b);
bool directions_meet_expectations(Direction_t a, Direction_t b, Direction_t first_expectation, Direction_t second_expectation);
DirectionMask_t vec_direction_mask(Vec_t vec);
U8 portal_rotations_between(Direction_t a, Direction_t b);
Vec_t direction_to_vec(Direction_t d);
Pixel_t direction_to_pixel(Direction_t d);

Vec_t vec_rotate_quadrants_clockwise(Vec_t vec, S8 rotations_between);
Pixel_t pixel_rotate_quadrants_clockwise(Pixel_t pixel, S8 rotations_between);
Position_t position_rotate_quadrants_clockwise(Position_t pos, S8 rotations_between);
Vec_t vec_rotate_quadrants_counter_clockwise(Vec_t vec, S8 rotations_between);
Pixel_t pixel_rotate_quadrants_counter_clockwise(Pixel_t pixel, S8 rotations_between);
Position_t position_rotate_quadrants_counter_clockwise(Position_t pos, S8 rotations_between);

Vec_t rotate_vec_between_dirs_clockwise(Direction_t a, Direction_t b, Vec_t vec);

Rect_t rect_surrounding_adjacent_coords(Coord_t coord);

Interactive_t* quad_tree_interactive_find_at(QuadTreeNode_t<Interactive_t>* root, Coord_t coord);
Interactive_t* quad_tree_interactive_solid_at(QuadTreeNode_t<Interactive_t>* root, TileMap_t* tilemap, Coord_t coord);

Rect_t rect_to_check_surrounding_blocks(Pixel_t center);

void find_portal_adjacents_to_skip_collision_check(Coord_t coord, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                                                   Coord_t* skip_coord);
bool portal_has_destination(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);

S16 range_passes_tile_boundary(S16 a, S16 b, S16 ignore);

Pixel_t mouse_select_pixel(Vec_t mouse_screen);
Pixel_t mouse_select_world_pixel(Vec_t mouse_screen, Position_t camera);
Coord_t mouse_select_coord(Vec_t mouse_screen);
Coord_t mouse_select_world(Vec_t mouse_screen, Position_t camera);
S32 mouse_select_index(Vec_t mouse_screen);
Vec_t coord_to_screen_position(Coord_t coord);
