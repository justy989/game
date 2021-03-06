#pragma once

#include "camera.h"
#include "direction.h"
#include "coord.h"
#include "vec.h"
#include "position.h"
#include "quad_tree.h"
#include "tile.h"
#include "interactive.h"
#include "quad.h"
#include "player.h"
#include "block_cut.h"

Direction_t direction_between(Coord_t a, Coord_t b);
bool directions_meet_expectations(Direction_t a, Direction_t b, Direction_t first_expectation, Direction_t second_expectation);
DirectionMask_t vec_direction_mask(Vec_t vec);
DirectionMask_t pixel_direction_mask_between(Pixel_t a, Pixel_t b);
DirectionMask_t coord_direction_mask_between(Coord_t a, Coord_t b);
Direction_t vec_direction(Vec_t vec); // returns the first direction it finds, only use this with one component vectors
U8 portal_rotations_between(Direction_t a, Direction_t b);
Vec_t direction_to_vec(Direction_t d);

Vec_t vec_rotate_quadrants_clockwise(Vec_t vec, S8 rotations_between);
Pixel_t pixel_rotate_quadrants_clockwise(Pixel_t pixel, S8 rotations_between);
Coord_t coord_rotate_quadrants_clockwise(Coord_t pixel, S8 rotations_between);
Position_t position_rotate_quadrants_clockwise(Position_t pos, S8 rotations_between);
Vec_t vec_rotate_quadrants_counter_clockwise(Vec_t vec, S8 rotations_between);
Pixel_t pixel_rotate_quadrants_counter_clockwise(Pixel_t pixel, S8 rotations_between);
Coord_t coord_rotate_quadrants_counter_clockwise(Coord_t pixel, S8 rotations_between);
Position_t position_rotate_quadrants_counter_clockwise(Position_t pos, S8 rotations_between);

Vec_t rotate_vec_between_dirs_clockwise(Direction_t a, Direction_t b, Vec_t vec);

Rect_t rect_surrounding_coord(Coord_t coord);
Rect_t rect_surrounding_adjacent_coords(Coord_t coord);
Rect_t rect_to_check_surrounding_blocks(Pixel_t center);

Interactive_t* quad_tree_interactive_find_at(QuadTreeNode_t<Interactive_t>* root, Coord_t coord);
Interactive_t* quad_tree_interactive_solid_at(QuadTreeNode_t<Interactive_t>* root, TileMap_t* tilemap, Coord_t coord, S8 check_height, bool player = false);

bool portal_has_destination(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);

Interactive_t* player_is_teleporting(const Player_t* player, QuadTreeNode_t<Interactive_t>* interactive_qt);

S16 range_passes_boundary(S16 a, S16 b, S16 boundary_size, S16 ignore);
S16 range_passes_solid_boundary(S16 a, S16 b, BlockCut_t cut, bool x, S16 alternate_pixel_start, S16 alternate_pixel_end,
                                S16 z, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt);

Pixel_t mouse_select_world_pixel(Vec_t mouse_screen, Camera_t* camera);
Coord_t mouse_select_world_coord(Vec_t mouse_screen, Camera_t* camera);
Vec_t coord_to_screen_position(Coord_t coord);

bool vec_in_quad(const Quad_t* q, Vec_t v);

S16 closest_grid_center_pixel(S16 grid_width, S16 v);
S16 closest_pixel(S16 pixel, F32 decimal);
S16 passes_over_grid_pixel(S16 pixel_a, S16 pixel_b);
S16 passes_over_pixel(S16 pixel_a, S16 pixel_b);

void get_rect_coords(Rect_t rect, Coord_t* coords);

Pixel_t closest_pixel_in_rect(Pixel_t pixel, Rect_t rect);
Vec_t closest_vec_in_quad(Vec_t v, Quad_t quad);

bool direction_in_vec(Vec_t vec, Direction_t direction);
F32 get_block_normal_pushed_velocity(BlockCut_t cut, S16 mass, F32 force = 1.0);
F32 rotate_vec_clockwise_to_see_if_negates(F32 value, bool x, S8 rotations);
F32 rotate_vec_counter_clockwise_to_see_if_negates(F32 value, bool x, S8 rotations);

S16 get_boundary_from_coord(Coord_t coord, Direction_t direction);
Pixel_t get_corner_pixel_from_pos(Position_t pos, DirectionMask_t from_center);
F32 lerp(F32 a, F32 b, F32 t);
