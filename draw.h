#pragma once

#include "bitmap.h"
#include "vec.h"
#include "block.h"
#include "interactive.h"
#include "quad_tree.h"
#include "tile.h"
#include "player.h"

#include <SDL2/SDL_opengl.h>

#define THEME_FRAMES_WIDE 16
#define THEME_FRAMES_TALL 32
#define THEME_FRAME_WIDTH 0.0625f
#define THEME_FRAME_HEIGHT 0.03125f

#define ARROW_FRAME_WIDTH 0.25f
#define ARROW_FRAME_HEIGHT 0.0625f
#define ARROW_FRAMES_TALL 16
#define ARROW_FRAMES_WIDE 4

#define PLAYER_FRAME_WIDTH 0.25f
#define PLAYER_FRAME_HEIGHT 0.03125f
#define PLAYER_FRAMES_WIDE 4
#define PLAYER_FRAMES_TALL 32

struct Quad_t{
     F32 left;
     F32 bottom;
     F32 right;
     F32 top;
};

Vec_t theme_frame(S16 x, S16 y);
Vec_t arrow_frame(S8 x, S8 y);
Vec_t player_frame(S8 x, S8 y);

GLuint create_texture_from_bitmap(AlphaBitmap_t* bitmap);
GLuint transparent_texture_from_file(const char* filepath);

void draw_screen_texture(Vec_t pos, Vec_t tex, Vec_t dim, Vec_t tex_dim);
void draw_theme_frame(Vec_t tex_vec, Vec_t pos_vec);
void draw_double_theme_frame(Vec_t tex_vec, Vec_t pos_vec);
void draw_tile_id(U8 id, Vec_t pos);
void draw_tile_flags(U16 flags, Vec_t tile_pos);
void draw_block(Block_t* block, Vec_t pos_vec);
void draw_interactive(Interactive_t* interactive, Vec_t pos_vec, Coord_t coord,
                      TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);
void draw_flats(Vec_t pos, Tile_t* tile, Interactive_t* interactive, GLuint theme_texture,
                U8 portal_rotations);
void draw_solids(Vec_t pos, Interactive_t* interactive, Block_t** blocks, S16 block_count, Player_t* player,
                 Position_t screen_camera, GLuint theme_texture, GLuint player_texture,
                 Coord_t source_coord, Coord_t destination_coord, U8 portal_rotations,
                 TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);
void draw_quad_wireframe(const Quad_t* quad, F32 red, F32 green, F32 blue);
void draw_quad_filled(const Quad_t* quad, F32 red, F32 green, F32 blue);

void draw_selection(Coord_t selection_start, Coord_t selection_end, Position_t camera, F32 red, F32 green, F32 blue);
