#include "draw.h"
#include "defines.h"
#include "portal_exit.h"
#include "conversion.h"
#include "utils.h"

#include <float.h>

Vec_t theme_frame(S16 x, S16 y){
     y = (THEME_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * THEME_FRAME_WIDTH, (F32)(y) * THEME_FRAME_HEIGHT};
}

Vec_t arrow_frame(S8 x, S8 y) {
     y = (ARROW_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * ARROW_FRAME_WIDTH, (F32)(y) * ARROW_FRAME_HEIGHT};
}

Vec_t player_frame(S8 x, S8 y){
     y = (PLAYER_FRAMES_TALL - 1) - y;
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
     U8 id_x = id % 16;
     U8 id_y = id / 16;

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
          S16 frame_y = 17 + tile_flags_cluster_direction(flags);
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

void draw_block(Block_t* block, Vec_t pos_vec){
     Vec_t tex_vec = theme_frame(0, 6);
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
}

void draw_interactive(Interactive_t* interactive, Vec_t pos_vec, Coord_t coord,
                      TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     Vec_t tex_vec = {};
     switch(interactive->type){
     default:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
     {
          int frame_x = 7;
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
          tex_vec = theme_frame(interactive->popup.lift.ticks - 1, 8);
          draw_double_theme_frame(pos_vec, tex_vec);
          break;
     case INTERACTIVE_TYPE_DOOR:
          tex_vec = theme_frame(interactive->door.lift.ticks + 8, 11 + interactive->door.face);
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
                         if(portal_dest_interactive && portal_dest_interactive->type == INTERACTIVE_TYPE_PORTAL &&
                            portal_dest_interactive->portal.on){
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

          draw_theme_frame(pos_vec, theme_frame(interactive->portal.face, 26 + interactive->portal.on));
          break;
     }
     case INTERACTIVE_TYPE_WIRE_CROSS:
     {
          int y_frame = 17 + interactive->wire_cross.on;
          if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT) draw_theme_frame(pos_vec, theme_frame(6, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_UP) draw_theme_frame(pos_vec, theme_frame(7, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT) draw_theme_frame(pos_vec, theme_frame(8, y_frame));
          if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN) draw_theme_frame(pos_vec, theme_frame(9, y_frame));
          break;
     }
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

static void draw_ice(Vec_t pos, GLuint theme_texture){
     glEnd();

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
     glBindTexture(GL_TEXTURE_2D, theme_texture);
     glBegin(GL_QUADS);
     glColor3f(1.0f, 1.0f, 1.0f);
}

void draw_flats(Vec_t pos, Tile_t* tile, Interactive_t* interactive, GLuint theme_texture,
                U8 portal_rotations){
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
                    draw_ice(pos, theme_texture);
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

     if(tile_is_iced(tile)) draw_ice(pos, theme_texture);
}

void draw_solids(Vec_t pos, Interactive_t* interactive, Block_t** blocks, S16 block_count, Player_t* player,
                 Position_t screen_camera, GLuint theme_texture, GLuint player_texture,
                 Coord_t source_coord, Coord_t destination_coord, U8 portal_rotations,
                 TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree){
     if(interactive){
          if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE ||
             interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR ||
             interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR ||
             (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks == 1)){
               // pass, these are flat
          }else{
               draw_interactive(interactive, pos, source_coord, tilemap, interactive_quad_tree);

               if(interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.iced){
                    pos.y += interactive->popup.lift.ticks * PIXEL_SIZE;
                    Vec_t tex_vec = theme_frame(3, 12);
                    glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
                    draw_theme_frame(pos, tex_vec);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }
          }
     }

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          Position_t block_camera_offset = block->pos;
          block_camera_offset.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
          if(destination_coord.x >= 0){
               Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
               Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
               Position_t center_delta = block_camera_offset - source_pos;
               center_delta = position_rotate_quadrants_clockwise(center_delta, portal_rotations);
               block_camera_offset = destination_pos + center_delta;
          }

          block_camera_offset.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
          block_camera_offset -= screen_camera;
          block_camera_offset.pixel.y += block->pos.z;
          draw_block(block, pos_to_vec(block_camera_offset));
     }

     if(player){
          Vec_t pos_vec = pos_to_vec(player->pos);
          if(destination_coord.x >= 0){
               Position_t destination_pos = coord_to_pos_at_tile_center(destination_coord);
               Position_t source_pos = coord_to_pos_at_tile_center(source_coord);
               Position_t center_delta = player->pos - source_pos;
               center_delta = position_rotate_quadrants_clockwise(center_delta, portal_rotations);
               pos_vec = pos_to_vec(destination_pos + center_delta);
          }

          S8 player_frame_y = direction_rotate_clockwise(player->face, portal_rotations);
          if(player->push_time > FLT_EPSILON) player_frame_y += 4;
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

          glEnd();
          glBindTexture(GL_TEXTURE_2D, player_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x, tex_vec.y + PLAYER_FRAME_HEIGHT);
          glVertex2f(pos_vec.x - HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y + PLAYER_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y + HALF_TILE_SIZE);
          glTexCoord2f(tex_vec.x + PLAYER_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + HALF_TILE_SIZE, pos_vec.y - HALF_TILE_SIZE);
          glEnd();

          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
     }
}

