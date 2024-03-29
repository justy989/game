#pragma once

#include "camera.h"
#include "bitmap.h"
#include "vec.h"
#include "block.h"
#include "interactive.h"
#include "quad_tree.h"
#include "tile.h"
#include "player.h"
#include "arrow.h"
#include "quad.h"
#include "ui.h"

#include <SDL2/SDL_opengl.h>

#define THEME_FRAMES_WIDE (S16)(16)
#define THEME_FRAMES_TALL (S16)(32)
#define THEME_FRAME_WIDTH 0.0625f
#define THEME_FRAME_HEIGHT 0.03125f
#define THEME_TEXEL_WIDTH 0.0078125f
#define THEME_TEXEL_HEIGHT 0.00390625f

#define FLOOR_FRAMES_WIDE (S16)(16)
#define FLOOR_FRAMES_TALL (S16)(16)
#define FLOOR_FRAME_WIDTH 0.0625f
#define FLOOR_FRAME_HEIGHT 0.0625f

#define ARROW_FRAME_WIDTH 0.25f
#define ARROW_FRAME_HEIGHT 0.0625f
#define ARROW_FRAMES_TALL (S16)(16)
#define ARROW_FRAMES_WIDE (S16)(4)

#define PLAYER_FRAME_WIDTH 0.25f
#define PLAYER_FRAME_HEIGHT 0.03125f
#define PLAYER_FRAMES_WIDE (S16)(4)
#define PLAYER_FRAMES_TALL (S16)(32)

#define TEXT_CHAR_WIDTH (5.0f * PIXEL_SIZE)
#define TEXT_CHAR_HEIGHT (8.0f * PIXEL_SIZE)
#define TEXT_CHAR_SPACING (1.0f * PIXEL_SIZE)
#define TEXT_CHAR_TEX_WIDTH 0.01953125f
#define TEXT_CHAR_TEX_HEIGHT 1.0f

// forward declarations
struct Editor_t;
struct World_t;

struct Color3f_t{
     F32 red;
     F32 green;
     F32 blue;
};

struct BlockToTintIndex_t{
     Block_t* block;
     U32 index;
};

struct EntangleTints_t{
     ObjectArray_t<BitmapPixel_t> tints;

     // TODO: linear search could be improved
     ObjectArray_t<BlockToTintIndex_t> block_to_tint_index;
};

Vec_t theme_frame(S16 x, S16 y);
Vec_t floor_or_solid_frame(S16 x, S16 y);
Vec_t arrow_frame(S16 x, S16 y);
Vec_t player_frame(S16 x, S16 y);

GLuint create_texture_from_bitmap(AlphaBitmap_t* bitmap);
GLuint transparent_texture_from_file(const char* filepath);
GLuint transparent_texture_from_raw_bitmap(Raw_t* raw);

void draw_screen_texture(Vec_t pos, Vec_t tex, Vec_t dim, Vec_t tex_dim);
void draw_theme_frame(Vec_t tex_vec, Vec_t pos_vec);
void draw_double_theme_frame(Vec_t tex_vec, Vec_t pos_vec);
void draw_tile_id(U8 id, Vec_t pos);
void draw_tile_flags(U16 flags, Vec_t tile_pos, bool editor = false);
void draw_interactive(Interactive_t* interactive, Vec_t pos_vec, Coord_t coord,
                      TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                      bool editor = false);
void draw_color_quad(Quad_t quad, F32 r, F32 g, F32 b, F32 a);
void draw_floor(Vec_t pos, Tile_t* tile, U8 portal_rotations);
void draw_flats(Vec_t pos, Tile_t* tile, Interactive_t* interactive, U8 portal_rotations);
void draw_set_ice_color();
void draw_ice_tile(Vec_t pos);
void draw_solids(Vec_t pos, Interactive_t* interactive, Block_t** blocks, S16 block_count,
                 ObjectArray_t<Player_t>* players, bool* draw_players,
                 Position_t screen_camera, GLuint theme_texture, GLuint player_texture,
                 Coord_t source_coord, Coord_t destination_coord, U8 portal_rotations,
                 TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree);
void draw_block(Block_t* block, Vec_t pos_vec, U8 portal_rotations);
void draw_solid_interactive(Coord_t src_coord, Coord_t dst_coord, TileMap_t* tilemap,
                            QuadTreeNode_t<Interactive_t>* interactive_qt, Position_t camera);
void draw_world_row_solids(S16 y, S16 x_start, S16 x_end, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                           QuadTreeNode_t<Block_t>* block_qt, ObjectArray_t<Player_t>* players, Position_t camera, GLuint player_texture,
                           EntangleTints_t* entangle_tints);
void draw_world_row_arrows(S16 y, S16 x_start, S16 x_end, const ArrowArray_t* arrow_aray, Position_t camera);
void draw_portal_blocks(Block_t** blocks, S16 block_count, Coord_t source_coord, Coord_t destination_coord, S8 portal_rotations, Position_t camera);
void draw_portal_players(ObjectArray_t<Player_t>* players, Rect_t region, Coord_t source_coord, Coord_t destination_coord,
                         S8 portal_rotations, Position_t camera);
void draw_quad_wireframe(const Quad_t* quad, F32 red, F32 green, F32 blue);
void draw_quad_filled(const Quad_t* quad, F32 red, F32 green, F32 blue);

void draw_selection(Coord_t selection_start, Coord_t selection_end, Camera_t* camera, F32 red, F32 green, F32 blue);

void draw_text(const char* message, Vec_t pos, Vec_t dim = Vec_t{TEXT_CHAR_WIDTH, TEXT_CHAR_HEIGHT}, F32 spacing = TEXT_CHAR_SPACING);
void draw_editor_visible_map_indicators(Vec_t pos_vec, Tile_t* tile, Interactive_t* interactive);
void draw_editor(Editor_t* editor, World_t* world, Camera_t* camera, Vec_t mouse_screen,
                 GLuint theme_texture, GLuint floor_texture, GLuint solid_texture, GLuint text_texture);
void draw_checkbox(Checkbox_t* checkbox, Vec_t scroll);

Color3f_t rgb_to_color3f(BitmapPixel_t* p);
