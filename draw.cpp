#include "draw.h"
#include "defines.h"
#include "portal_exit.h"
#include "conversion.h"
#include "utils.h"
#include "world.h"
#include "editor.h"

#include <float.h>
#include <ctype.h>

Vec_t theme_frame(S16 x, S16 y){
     y = (THEME_FRAMES_TALL - (S16)(1)) - y;
     return Vec_t{(F32)(x) * THEME_FRAME_WIDTH, (F32)(y) * THEME_FRAME_HEIGHT};
}

Vec_t arrow_frame(S16 x, S16 y) {
     y = (ARROW_FRAMES_TALL - (S16)(1)) - y;
     return Vec_t{(F32)(x) * ARROW_FRAME_WIDTH, (F32)(y) * ARROW_FRAME_HEIGHT};
}

Vec_t player_frame(S16 x, S16 y){
     y = (PLAYER_FRAMES_TALL - (S16)(1)) - y;
     return Vec_t{(F32)(x) * PLAYER_FRAME_WIDTH, (F32)(y) * PLAYER_FRAME_HEIGHT};
}

GLuint create_texture_from_bitmap(AlphaBitmap_t* bitmap){
     GLuint texture = 0;

     glGenTextures(1, &texture);

     glBindTexture(GL_TEXTURE_2D, texture);

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap->width, bitmap->height, 0,  GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);

     return texture;
}

GLuint transparent_texture_from_file(const char* filepath){
     Bitmap_t bitmap = bitmap_load_from_file(filepath);
     if(bitmap.raw.byte_count == 0) return 0;
     AlphaBitmap_t alpha_bitmap = bitmap_to_alpha_bitmap(&bitmap, BitmapPixel_t{255, 0, 255});
     free(bitmap.raw.bytes);
     GLuint texture_id = create_texture_from_bitmap(&alpha_bitmap);
     free(alpha_bitmap.pixels);
     return texture_id;
}

void draw_screen_texture(Vec_t pos, Vec_t tex, Vec_t dim, Vec_t tex_dim){
     glTexCoord2f(tex.x, tex.y);
     glVertex2f(pos.x, pos.y);
     glTexCoord2f(tex.x, tex.y + tex_dim.y);
     glVertex2f(pos.x, pos.y + dim.y);
     glTexCoord2f(tex.x + tex_dim.x, tex.y + tex_dim.y);
     glVertex2f(pos.x + dim.x, pos.y + dim.y);
     glTexCoord2f(tex.x + tex_dim.x, tex.y);
     glVertex2f(pos.x + dim.x, pos.y);
}

void draw_theme_frame(Vec_t pos, Vec_t tex){
     Vec_t dim {TILE_SIZE, TILE_SIZE};
     Vec_t tex_dim {THEME_FRAME_WIDTH, THEME_FRAME_HEIGHT};
     draw_screen_texture(pos, tex, dim, tex_dim);
}

void draw_double_theme_frame(Vec_t pos, Vec_t tex){
     Vec_t dim {TILE_SIZE, 2.0f * TILE_SIZE};
     Vec_t tex_dim {THEME_FRAME_WIDTH, 2.0f * THEME_FRAME_HEIGHT};
     draw_screen_texture(pos, tex, dim, tex_dim);
}

void draw_tile_id(U8 id, Vec_t pos){
     U8 id_x = id % (U8)(16);
     U8 id_y = id / (U8)(16);
     draw_theme_frame(pos, theme_frame(id_x, id_y));
}

void draw_tile_flags(U16 flags, Vec_t tile_pos){
     if(flags == 0) return;

     if(flags & TILE_FLAG_CHECKPOINT){
          draw_theme_frame(tile_pos, theme_frame(0, 21));
     }

     if(flags & TILE_FLAG_RESET_IMMUNE){
          draw_theme_frame(tile_pos, theme_frame(1, 21));
     }

     if(flags & (TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN)){
          S16 frame_y = 9;
          S16 frame_x = flags >> 4;

          if(flags & TILE_FLAG_WIRE_STATE) frame_y++;

          draw_theme_frame(tile_pos, theme_frame(frame_x, frame_y));
     }

     if(flags & (TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT)){
          S16 frame_y = (S16)(17) + tile_flags_cluster_direction(flags);
          S16 frame_x = 0;

          if(flags & TILE_FLAG_WIRE_CLUSTER_LEFT){
               if(flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON){
                    frame_x = 1;
               }else{
                    frame_x = 0;
               }

               draw_theme_frame(tile_pos, theme_frame(frame_x, frame_y));
          }

          if(flags & TILE_FLAG_WIRE_CLUSTER_MID){
               if(flags & TILE_FLAG_WIRE_CLUSTER_MID_ON){
                    frame_x = 3;
               }else{
                    frame_x = 2;
               }

               draw_theme_frame(tile_pos, theme_frame(frame_x, frame_y));
          }

          if(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT){
               if(flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON){
                    frame_x = 5;
               }else{
                    frame_x = 4;
               }

               draw_theme_frame(tile_pos, theme_frame(frame_x, frame_y));
          }
     }
}

void draw_block(Block_t* block, Vec_t pos_vec, U8 portal_rotations){
     static const F32 block_shadow_opacity = 0.3f;

     U8 rotation = (block->rotation + portal_rotations) % static_cast<U8>(DIRECTION_COUNT);
     Vec_t tex_vec = theme_frame(rotation, 29);
     draw_double_theme_frame(pos_vec, tex_vec);

     if(block->element == ELEMENT_ONLY_ICED || block->element == ELEMENT_ICE ){
          tex_vec = theme_frame(4, 12);
          glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
          draw_double_theme_frame(pos_vec, tex_vec);
          glColor3f(1.0f, 1.0f, 1.0f);
     }

     if(block->element == ELEMENT_FIRE){
          tex_vec = theme_frame(1, 6);
          draw_double_theme_frame(pos_vec, tex_vec);
     }else if(block->element == ELEMENT_ICE){
          tex_vec = theme_frame(5, 6);
          draw_double_theme_frame(pos_vec, tex_vec);
     }

     if(block->entangle_index >= 0){
          tex_vec = theme_frame(0, 22);
          draw_theme_frame(pos_vec, tex_vec);
     }

     if(block->pos.z > 0){
          Quad_t quad;
          quad.left = pos_vec.x;
          quad.right = pos_vec.x + (F32)(TILE_SIZE_IN_PIXELS) * PIXEL_SIZE;;
          quad.top = pos_vec.y;
          quad.bottom = pos_vec.y - (F32)(block->pos.z) * PIXEL_SIZE;

          glEnd();

          GLint save_texture;
          glGetIntegerv(GL_TEXTURE_BINDING_2D, &save_texture);

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_QUADS);
          glColor4f(0.0f, 0.0f, 0.0f, block_shadow_opacity);
          glVertex2f(quad.left,  quad.top);
          glVertex2f(quad.left,  quad.bottom);
          glVertex2f(quad.right, quad.bottom);
          glVertex2f(quad.right, quad.top);
          glEnd();

          glBindTexture(GL_TEXTURE_2D, save_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
     }
}

void draw_interactive(Interactive_t* interactive, Vec_t pos_vec, Coord_t coord,
                      TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Vec_t tex_vec = {};
     switch(interactive->type){
     default:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
     {
          S16 frame_x = 7;
          if(interactive->pressure_plate.down) frame_x++;
          tex_vec = theme_frame(frame_x, 8);
          draw_theme_frame(pos_vec, tex_vec);
     } break;
     case INTERACTIVE_TYPE_LEVER:
          tex_vec = theme_frame(0, 12);
          draw_double_theme_frame(pos_vec, tex_vec);
          break;
     case INTERACTIVE_TYPE_BOW:
          draw_theme_frame(pos_vec, theme_frame(0, 9));
          break;
     case INTERACTIVE_TYPE_POPUP:
          tex_vec = theme_frame(interactive->popup.lift.ticks - (S16)(1), 8);
          draw_double_theme_frame(pos_vec, tex_vec);
          break;
     case INTERACTIVE_TYPE_DOOR:
          tex_vec = theme_frame(interactive->door.lift.ticks + (S16)(8), (S16)(11) + interactive->door.face);
          draw_theme_frame(pos_vec, tex_vec);
          break;
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          draw_theme_frame(pos_vec, theme_frame(1, 11));
          if(interactive->detector.on){
               draw_theme_frame(pos_vec, theme_frame(2, 11));
          }
          break;
     case INTERACTIVE_TYPE_ICE_DETECTOR:
          draw_theme_frame(pos_vec, theme_frame(1, 12));
          if(interactive->detector.on){
               draw_theme_frame(pos_vec, theme_frame(2, 12));
          }
          break;
     case INTERACTIVE_TYPE_PORTAL:
     {
          bool draw_wall = false;
          if(!interactive->portal.on){
               draw_wall = true;
          }else{
               draw_wall = true;

               // search all portal exits for a portal they can go through
               PortalExit_t portal_exits = find_portal_exits(coord, tilemap, interactive_quad_tree);
               for(S8 d = 0; d < DIRECTION_COUNT && draw_wall; d++){
                    for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                         if(portal_exits.directions[d].coords[p] == coord) continue;

                         Coord_t portal_dest = portal_exits.directions[d].coords[p];
                         Interactive_t* portal_dest_interactive = quad_tree_find_at(interactive_quad_tree, portal_dest.x, portal_dest.y);
                         if(is_active_portal(portal_dest_interactive)){
                              draw_wall = false;
                              break;
                         }
                    }
               }
          }

          if(draw_wall){
               S16 frame_x = 0;
               S16 frame_y = 0;

               switch(interactive->portal.face){
               default:
                    break;
               case DIRECTION_LEFT:
                    frame_x = 3;
                    frame_y = 1;
                    break;
               case DIRECTION_UP:
                    frame_x = 0;
                    frame_y = 2;
                    break;
               case DIRECTION_RIGHT:
                    frame_x = 2;
                    frame_y = 2;
                    break;
               case DIRECTION_DOWN:
                    frame_x = 1;
                    frame_y = 1;
                    break;
               }

               draw_theme_frame(pos_vec, theme_frame(frame_x, frame_y));
          }

          draw_theme_frame(pos_vec, theme_frame(interactive->portal.face, (S16)(26 + interactive->portal.on)));
          break;
     }
     case INTERACTIVE_TYPE_WIRE_CROSS:
     {
          S16 y_frame = (S16)(17) + interactive->wire_cross.on;
          if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT) draw_theme_frame(pos_vec, theme_frame(6, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_UP) draw_theme_frame(pos_vec, theme_frame(7, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT) draw_theme_frame(pos_vec, theme_frame(8, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN) draw_theme_frame(pos_vec, theme_frame(9, y_frame));
          break;
     }
     case INTERACTIVE_TYPE_CLONE_KILLER:
          draw_theme_frame(pos_vec, theme_frame(4, 27));
          break;
     }
}

void draw_quad_wireframe(const Quad_t* quad, F32 red, F32 green, F32 blue){
     glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

     glBegin(GL_QUADS);
     glColor3f(red, green, blue);
     glVertex2f(quad->left,  quad->top);
     glVertex2f(quad->left,  quad->bottom);
     glVertex2f(quad->right, quad->bottom);
     glVertex2f(quad->right, quad->top);
     glEnd();

     glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void draw_quad_filled(const Quad_t* quad, F32 red, F32 green, F32 blue){
     glBegin(GL_QUADS);
     glColor3f(red, green, blue);
     glVertex2f(quad->left,  quad->top);
     glVertex2f(quad->left,  quad->bottom);
     glVertex2f(quad->right, quad->bottom);
     glVertex2f(quad->right, quad->top);
     glEnd();
}

void draw_selection(Coord_t selection_start, Coord_t selection_end, Position_t camera, F32 red, F32 green, F32 blue){
     if(selection_start.x > selection_end.x) SWAP(selection_start.x, selection_end.x);
     if(selection_start.y > selection_end.y) SWAP(selection_start.y, selection_end.y);

     Position_t start_location = coord_to_pos(selection_start) - camera;
     Position_t end_location = coord_to_pos(selection_end) - camera;
     Vec_t start_vec = pos_to_vec(start_location);
     Vec_t end_vec = pos_to_vec(end_location);

     Quad_t selection_quad {start_vec.x, start_vec.y, end_vec.x + TILE_SIZE, end_vec.y + TILE_SIZE};
     glBindTexture(GL_TEXTURE_2D, 0);
     draw_quad_wireframe(&selection_quad, red, green, blue);
}

static void draw_ice(Vec_t pos){
     glEnd();

     GLint save_texture;
     glGetIntegerv(GL_TEXTURE_BINDING_2D, &save_texture);

     // get state ready for ice
     glBindTexture(GL_TEXTURE_2D, 0);
     glColor4f(196.0f / 255.0f, 217.0f / 255.0f, 1.0f, 0.45f);
     glBegin(GL_QUADS);
     glVertex2f(pos.x, pos.y);
     glVertex2f(pos.x, pos.y + TILE_SIZE);
     glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
     glVertex2f(pos.x + TILE_SIZE, pos.y);
     glEnd();

     // reset state back to default
     glBindTexture(GL_TEXTURE_2D, save_texture);
     glBegin(GL_QUADS);
     glColor3f(1.0f, 1.0f, 1.0f);
}

Vec_t draw_player(Player_t* player, Vec_t camera, Coord_t source_coord, Coord_t destination_coord, S8 portal_rotations){
     Vec_t pos_vec = pos_to_vec(player->pos) + camera;
     if(destination_coord.x >= 0){
          Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
          Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
          Position_t center_delta = player->pos - source_pos;
          center_delta = position_rotate_quadrants_clockwise(center_delta, portal_rotations);
          pos_vec = pos_to_vec(destination_pos + center_delta);
     }

     S8 player_frame_y = direction_rotate_clockwise(player->face, portal_rotations);
     if(player->pushing_block >= 0){
         player_frame_y += 4;
     }else if(player->stopping_block_from < DIRECTION_COUNT){
          player_frame_y = direction_rotate_clockwise(player->stopping_block_from, portal_rotations);
          player_frame_y += 4;
     }

     if(player->has_bow){
          player_frame_y += 8;
          if(player->bow_draw_time > (PLAYER_BOW_DRAW_DELAY / 2.0f)){
               player_frame_y += 8;
               if(player->bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                    player_frame_y += 4;
               }
          }
     }

     Vec_t tex_vec = player_frame(player->walk_frame, player_frame_y);
     pos_vec.y += (5.0f * PIXEL_SIZE);
     pos_vec.y += ((float)(player->pos.z) * PIXEL_SIZE);

     Vec_t shadow_vec = player_frame(3, 0);


     // draw shadow
     pos_vec.y += PIXEL_SIZE; // move shadow up 1
     glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
     glTexCoord2f(shadow_vec.x, shadow_vec.y);
     glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
     glTexCoord2f(shadow_vec.x, shadow_vec.y + PLAYER_FRAME_HEIGHT);
     glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
     glTexCoord2f(shadow_vec.x + PLAYER_FRAME_WIDTH, shadow_vec.y + PLAYER_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
     glTexCoord2f(shadow_vec.x + PLAYER_FRAME_WIDTH, shadow_vec.y);
     glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
     pos_vec.y += PIXEL_SIZE; // undo shadow transform

     // draw player
     glColor3f(1.0f, 1.0f, 1.0f);
     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
     glTexCoord2f(tex_vec.x, tex_vec.y + PLAYER_FRAME_HEIGHT);
     glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
     glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y + PLAYER_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
     glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);

     return pos_vec;
}

void draw_flats(Vec_t pos, Tile_t* tile, Interactive_t* interactive, U8 portal_rotations){
     draw_tile_id(tile->id, pos);

     U16 tile_flags = tile->flags;
     for(U8 i = 0; i < portal_rotations; i++){
          U16 new_flags = tile_flags & ~(TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN);

          if(tile_flags & TILE_FLAG_WIRE_LEFT) new_flags |= TILE_FLAG_WIRE_UP;
          if(tile_flags & TILE_FLAG_WIRE_UP) new_flags |= TILE_FLAG_WIRE_RIGHT;
          if(tile_flags & TILE_FLAG_WIRE_RIGHT) new_flags |= TILE_FLAG_WIRE_DOWN;
          if(tile_flags & TILE_FLAG_WIRE_DOWN) new_flags |= TILE_FLAG_WIRE_LEFT;

          tile_flags = new_flags;
     }

     draw_tile_flags(tile_flags, pos);

     // draw flat interactives that could be covered by ice
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
               if(!tile_is_iced(tile) && interactive->pressure_plate.iced_under){
                    draw_ice(pos);
               }

               draw_interactive(interactive, pos, Coord_t{-1, -1}, nullptr, nullptr);
          }else if(interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks == 1){
               if(interactive->popup.iced){
                    pos.y += interactive->popup.lift.ticks * PIXEL_SIZE;
                    Vec_t tex_vec = theme_frame(3, 12);
                    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                    draw_theme_frame(pos, tex_vec);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }

               draw_interactive(interactive, pos, Coord_t{-1, -1}, nullptr, nullptr);
          }else if(interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR ||
                   interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR){
               draw_interactive(interactive, pos, Coord_t{-1, -1}, nullptr, nullptr);
          }
     }

     if(tile_is_iced(tile)) draw_ice(pos);
}

void draw_portal_blocks(Block_t** blocks, S16 block_count, Coord_t source_coord, Coord_t destination_coord, S8 portal_rotations, Vec_t camera) {
     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          Position_t draw_pos = block->pos;
          draw_pos.pixel += HALF_TILE_SIZE_PIXEL;

          Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
          Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
          Position_t center_delta = draw_pos - source_pos;
          center_delta = position_rotate_quadrants_clockwise(center_delta, portal_rotations);
          draw_pos = destination_pos + center_delta;

          draw_pos.pixel -= HALF_TILE_SIZE_PIXEL;
          draw_pos += camera;
          draw_pos.pixel.y += block->pos.z;
          draw_block(block, pos_to_vec(draw_pos), portal_rotations);
     }
}

void draw_portal_players(ObjectArray_t<Player_t>* players, Rect_t region, Coord_t source_coord, Coord_t destination_coord,
                         S8 portal_rotations, Vec_t camera){
     for(S16 i = 0; i < players->count; i++){
          Player_t* player = players->elements + i;
          if(!pixel_in_rect(player->pos.pixel, region)) continue;

          draw_player(player, camera, source_coord, destination_coord, portal_rotations);
     }
}

void draw_world_row_flats(S16 y, S16 x_start, S16 x_end, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                          Vec_t camera){
     auto draw_pos = Vec_t{(float)(x_start) * TILE_SIZE, (float)(y) * TILE_SIZE} + camera;
     auto save_draw_pos = draw_pos;

     // flat layer
     draw_pos = save_draw_pos;
     for(S16 x = x_start; x <= x_end; x++){
          auto tile = tilemap_get_tile(tilemap, Coord_t{x, y});
          Interactive_t* interactive = quad_tree_find_at(interactive_qt, x, y);
          if(is_active_portal(interactive)){
               Coord_t coord {x, y};
               PortalExit_t portal_exits = find_portal_exits(coord, tilemap, interactive_qt);

               for(S8 d = 0; d < DIRECTION_COUNT; d++){
                    for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                         if(portal_exits.directions[d].coords[i] == coord) continue;
                         Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                         Tile_t* portal_tile = tilemap_get_tile(tilemap, portal_coord);
                         Interactive_t* portal_interactive = quad_tree_find_at(interactive_qt, portal_coord.x, portal_coord.y);
                         U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                         draw_flats(draw_pos, portal_tile, portal_interactive, portal_rotations);
                    }
               }
          }else{
               draw_flats(draw_pos, tile, interactive, 0);
          }
          draw_pos.x += TILE_SIZE;
     }
}

void draw_world_row_solids(S16 y, S16 x_start, S16 x_end, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                           QuadTreeNode_t<Block_t>* block_qt, ObjectArray_t<Player_t>* players, Vec_t camera, GLuint player_texture){
     auto draw_pos = Vec_t{(float)(x_start) * TILE_SIZE, (float)(y) * TILE_SIZE} + camera;
     auto save_draw_pos = draw_pos;

     // solid layer
     draw_pos = save_draw_pos;
     for(S16 x = x_start; x <= x_end; x++){
          Interactive_t* interactive = quad_tree_find_at(interactive_qt, x, y);
          if(interactive){
               if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE ||
                  interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR ||
                  interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR ||
                  (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks == 1)){
                    // pass
               }else{
                    draw_interactive(interactive, draw_pos, Coord_t{x, y}, tilemap, interactive_qt);

                    if(interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.iced){
                         auto ice_draw_pos = draw_pos;
                         ice_draw_pos.y += interactive->popup.lift.ticks * PIXEL_SIZE;
                         Vec_t tex_vec = theme_frame(3, 12);
                         glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                         draw_theme_frame(ice_draw_pos, tex_vec);
                         glColor3f(1.0f, 1.0f, 1.0f);
                    }
               }
          }

          draw_pos.x += TILE_SIZE;
     }

     // block layer
     auto search_rect = Rect_t{(S16)(x_start * TILE_SIZE_IN_PIXELS), (S16)(y * TILE_SIZE_IN_PIXELS),
                               (S16)((x_end * TILE_SIZE_IN_PIXELS) + TILE_SIZE_IN_PIXELS),
                               (S16)((y * TILE_SIZE_IN_PIXELS) + TILE_SIZE_IN_PIXELS)};

     S16 block_count = 0;
     Block_t* blocks[256]; // oh god i hope we don't need more than that?
     quad_tree_find_in(block_qt, search_rect, blocks, &block_count, 256);

     sort_blocks_by_height(blocks, block_count);

     for(S16 i = 0; i < block_count; i++){
          auto block = blocks[i];
          auto draw_block_pos = block->pos;
          draw_block_pos.pixel.y += block->pos.z;
          draw_block(block, pos_to_vec(draw_block_pos) + camera, 0);
     }

     glEnd();

     GLint save_texture = 0;
     glGetIntegerv(GL_TEXTURE_BINDING_2D, &save_texture);

     glBindTexture(GL_TEXTURE_2D, player_texture);
     glBegin(GL_QUADS);

     // player layer slayer flayer bayer
     for(S16 i = 0; i < players->count; i++){
          auto player = players->elements + i;
          auto coord = pos_to_coord(player->pos);

          if(coord.y == y && coord.x >= x_start && coord.x <= x_end){
               draw_pos = draw_player(player, camera, coord, Coord_t{-1, -1}, 0);

               if(i >= 1){
                   glEnd();
                   glBindTexture(GL_TEXTURE_2D, save_texture);
                   glBegin(GL_QUADS);

                   draw_pos -= Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};
                   auto tex_vec = theme_frame(0, 22);
                   draw_theme_frame(draw_pos, tex_vec);

                   glEnd();
                   glBindTexture(GL_TEXTURE_2D, player_texture);
                   glBegin(GL_QUADS);
               }
          }
     }
}

void draw_world_row_arrows(S16 y, S16 x_start, S16 x_end, const ArrowArray_t* arrow_array, Vec_t camera){
     // draw arrows
     static Vec_t arrow_tip_offset[DIRECTION_COUNT] = {
          {0.0f,               9.0f * PIXEL_SIZE},
          {8.0f * PIXEL_SIZE,  16.0f * PIXEL_SIZE},
          {16.0f * PIXEL_SIZE, 9.0f * PIXEL_SIZE},
          {8.0f * PIXEL_SIZE,  0.0f * PIXEL_SIZE},
     };

     for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
          const Arrow_t* arrow = arrow_array->arrows + a;
          if(!arrow->alive) continue;
          auto coord = pos_to_coord(arrow->pos);
          if(coord.y != y) continue;
          if(coord.x < x_start || coord.x > x_end) continue;

          Vec_t arrow_vec = pos_to_vec(arrow->pos - camera);
          arrow_vec.x -= arrow_tip_offset[arrow->face].x;
          arrow_vec.y -= arrow_tip_offset[arrow->face].y;

          // shadow
          //arrow_vec.y -= (arrow->pos.z * PIXEL_SIZE);
          Vec_t tex_vec = arrow_frame(arrow->face, 1);
          Vec_t dim {TILE_SIZE, TILE_SIZE};
          Vec_t tex_dim {ARROW_FRAME_WIDTH, ARROW_FRAME_HEIGHT};
          draw_screen_texture(arrow_vec, tex_vec, dim, tex_dim);

          arrow_vec.y += (arrow->pos.z * PIXEL_SIZE);

          S8 y_frame = 0;
          if(arrow->element) y_frame = (S8)(2 + ((arrow->element - 1) * 4));

          tex_vec = arrow_frame(arrow->face, y_frame);
          draw_screen_texture(arrow_vec, tex_vec, dim, tex_dim);
     }
}

void draw_text(const char* message, Vec_t pos)
{
     char c;
     Vec_t dimensions {TEXT_CHAR_WIDTH, TEXT_CHAR_HEIGHT};
     Vec_t tex {};
     Vec_t tex_dimensions {TEXT_CHAR_TEX_WIDTH, TEXT_CHAR_TEX_HEIGHT};

     while((c = *message)){
          if(isalpha(c)){
               tex.x = (F32)((c - 'A')) * TEXT_CHAR_TEX_WIDTH;
          }else if(isdigit(c)){
               tex.x = (F32)((c - '0') + ('Z' - 'A') + 1) * TEXT_CHAR_TEX_WIDTH;
          }else if(c == ':'){
               tex.x = 36.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '.'){
               tex.x = 37.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == ','){
               tex.x = 38.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '+'){
               tex.x = 39.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '?'){
               tex.x = 40.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '!'){
               tex.x = 41.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '\''){
               tex.x = 42.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '*'){
               tex.x = 43.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '/'){
               tex.x = 44.0f * TEXT_CHAR_TEX_WIDTH;
          }else if(c == '-'){
               tex.x = 45.0f * TEXT_CHAR_TEX_WIDTH;
          }else{
               tex.x = 1.0f - TEXT_CHAR_TEX_WIDTH;
          }

          draw_screen_texture(pos, tex, dimensions, tex_dimensions);

          pos.x += TEXT_CHAR_WIDTH + TEXT_CHAR_SPACING;
          message++;
     }
}

static Block_t block_from_stamp(Stamp_t* stamp){
     Block_t block = {};
     block.element = stamp->block.element;
     block.rotation = stamp->block.rotation;
     block.pos.z = stamp->block.z;
     block.entangle_index = -1;
     block.clone_start = Coord_t{};
     block.clone_id = 0;
     return block;
}

void draw_editor(Editor_t* editor, World_t* world, Position_t screen_camera, Vec_t mouse_screen,
                 GLuint theme_texture, GLuint text_texture){
     switch(editor->mode){
     default:
          break;
     case EDITOR_MODE_OFF:
          // pass
          break;
     case EDITOR_MODE_CATEGORY_SELECT:
     {
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          Vec_t vec = {0.0f, 0.0f};

          for(S32 g = 0; g < editor->category_array.count; ++g){
               auto* category = editor->category_array.elements + g;
               auto* stamp_array = category->elements + 0;

               for(S16 s = 0; s < stamp_array->count; s++){
                    auto* stamp = stamp_array->elements + s;
                    if(g && (g % ROOM_TILE_SIZE) == 0){
                         vec.x = 0.0f;
                         vec.y += TILE_SIZE;
                    }

                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         draw_tile_id(stamp->tile_id, vec);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         draw_tile_flags(stamp->tile_flags, vec);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = block_from_stamp(stamp);
                         vec.y += PIXEL_SIZE * block.pos.z;
                         draw_block(&block, vec, 0);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         draw_interactive(&stamp->interactive, vec, Coord_t{-1, -1}, &world->tilemap, world->interactive_qt);
                    } break;
                    }
               }

               vec.x += TILE_SIZE;
          }

          glEnd();
     } break;
     case EDITOR_MODE_STAMP_SELECT:
     case EDITOR_MODE_STAMP_HIDE:
     {
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          // draw stamp at mouse
          auto* stamp_array = editor->category_array.elements[editor->category].elements + editor->stamp;
          Coord_t mouse_coord = mouse_select_coord(mouse_screen);

          for(S16 s = 0; s < stamp_array->count; s++){
               auto* stamp = stamp_array->elements + s;
               Vec_t stamp_pos = coord_to_screen_position(mouse_coord + stamp->offset);
               switch(stamp->type){
               default:
                    break;
               case STAMP_TYPE_TILE_ID:
                    draw_tile_id(stamp->tile_id, stamp_pos);
                    break;
               case STAMP_TYPE_TILE_FLAGS:
                    draw_tile_flags(stamp->tile_flags, stamp_pos);
                    break;
               case STAMP_TYPE_BLOCK:
               {
                    Block_t block = block_from_stamp(stamp);
                    stamp_pos.y += PIXEL_SIZE * block.pos.z;
                    draw_block(&block, stamp_pos, 0);
               } break;
               case STAMP_TYPE_INTERACTIVE:
               {
                    draw_interactive(&stamp->interactive, stamp_pos, Coord_t{-1, -1}, &world->tilemap, world->interactive_qt);
               } break;
               }
          }

          if(editor->mode == EDITOR_MODE_STAMP_SELECT){
               // draw stamps to select from at the bottom
               Vec_t pos = {0.0f, 0.0f};
               S16 row_height = 1;
               auto* category = editor->category_array.elements + editor->category;

               for(S32 g = 0; g < category->count; ++g){
                    stamp_array = category->elements + g;
                    Coord_t dimensions = stamp_array_dimensions(stamp_array);
                    if(dimensions.y > row_height) row_height = dimensions.y;

                    for(S32 s = 0; s < stamp_array->count; s++){
                         auto* stamp = stamp_array->elements + s;
                         Vec_t stamp_vec = pos + coord_to_vec(stamp->offset);

                         switch(stamp->type){
                         default:
                              break;
                         case STAMP_TYPE_TILE_ID:
                              draw_tile_id(stamp->tile_id, stamp_vec);
                              break;
                         case STAMP_TYPE_TILE_FLAGS:
                              draw_tile_flags(stamp->tile_flags, stamp_vec);
                              break;
                         case STAMP_TYPE_BLOCK:
                         {
                              Block_t block = block_from_stamp(stamp);
                              stamp_vec.y += PIXEL_SIZE * block.pos.z;
                              draw_block(&block, stamp_vec, 0);
                         } break;
                         case STAMP_TYPE_INTERACTIVE:
                         {
                              draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &world->tilemap, world->interactive_qt);
                         } break;
                         }
                    }

                    pos.x += (dimensions.x * TILE_SIZE);
                    if(pos.x >= 1.0f){
                         pos.x = 0.0f;
                         pos.y += row_height * TILE_SIZE;
                         row_height = 1;
                    }
               }
          }

          glEnd();
     } break;
     case EDITOR_MODE_CREATE_SELECTION:
          draw_selection(editor->selection_start, editor->selection_end, screen_camera, 1.0f, 0.0f, 0.0f);
          break;
     case EDITOR_MODE_SELECTION_MANIPULATION:
     {
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          for(S32 g = 0; g < editor->selection.count; ++g){
               auto* stamp = editor->selection.elements + g;
               Position_t stamp_pos = coord_to_pos(editor->selection_start + stamp->offset);
               Vec_t stamp_vec = pos_to_vec(stamp_pos);

               switch(stamp->type){
               default:
                    break;
               case STAMP_TYPE_TILE_ID:
                    draw_tile_id(stamp->tile_id, stamp_vec);
                    break;
               case STAMP_TYPE_TILE_FLAGS:
                    draw_tile_flags(stamp->tile_flags, stamp_vec);
                    if(stamp->tile_flags & TILE_FLAG_ICED){
                         draw_ice(stamp_vec);
                    }
                    break;
               case STAMP_TYPE_BLOCK:
               {
                    Block_t block = block_from_stamp(stamp);
                    stamp_vec.y += PIXEL_SIZE * block.pos.z;
                    draw_block(&block, stamp_vec, 0);
               } break;
               case STAMP_TYPE_INTERACTIVE:
               {
                    draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &world->tilemap, world->interactive_qt);
               } break;
               }
          }
          glEnd();

          Rect_t selection_bounds = editor_selection_bounds(editor);
          Coord_t min_coord {selection_bounds.left, selection_bounds.bottom};
          Coord_t max_coord {selection_bounds.right, selection_bounds.top};
          draw_selection(min_coord, max_coord, screen_camera, 1.0f, 0.0f, 0.0f);
     } break;
     }

     if(editor->mode){
          glBindTexture(GL_TEXTURE_2D, text_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          auto mouse_world = vec_to_pos(mouse_screen);
          auto mouse_coord = pos_to_coord(mouse_world);
          char buffer[64];
          snprintf(buffer, 64, "M: %d,%d", mouse_coord.x, mouse_coord.y);

          Vec_t text_pos {0.005f, 0.965f};

          glColor3f(0.0f, 0.0f, 0.0f);
          draw_text(buffer, text_pos + Vec_t{0.002f, -0.002f});

          glColor3f(1.0f, 1.0f, 1.0f);
          draw_text(buffer, text_pos);

          Player_t* player = world->players.elements;
          snprintf(buffer, 64, "P: %d,%d,%d R: %d", player->pos.pixel.x, player->pos.pixel.y, player->pos.z, player->rotation);
          text_pos.y -= 0.045f;

          glColor3f(0.0f, 0.0f, 0.0f);
          draw_text(buffer, text_pos + Vec_t{0.002f, -0.002f});

          glColor3f(1.0f, 1.0f, 1.0f);
          draw_text(buffer, text_pos);

          glEnd();
     }
}
