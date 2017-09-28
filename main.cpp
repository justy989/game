#include <iostream>
#include <chrono>
#include <cfloat>
#include <cassert>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "log.h"
#include "defines.h"
#include "direction.h"
#include "bitmap.h"
#include "conversion.h"
#include "rect.h"
#include "tile.h"
#include "element.h"

#define PLAYER_SPEED 5.5f
#define PLAYER_WALK_DELAY 0.15f
#define PLAYER_IDLE_SPEED 0.0025f

struct Player_t{
     Position_t pos;
     Vec_t accel;
     Vec_t vel;
     Direction_t face;
     F32 radius;
     F32 push_time;
     S8 walk_frame;
     S8 walk_frame_delta;
     F32 walk_frame_time;
};

struct Block_t{
     Position_t pos;
     Vec_t accel;
     Vec_t vel;
     F32 fall_time;
     DirectionMask_t force;
     Element_t element;
     Pixel_t push_start;
};

Coord_t block_get_coord(Block_t* block){
     Pixel_t center = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
     return pixel_to_coord(center);
}

bool block_on_ice(Block_t* block, TileMap_t* tilemap){
     return tilemap_is_iced(tilemap, block_get_coord(block));
}

bool blocks_at_collidable_height(Block_t* a, Block_t* b){
     S8 a_top = a->pos.z + HEIGHT_INTERVAL - 1;
     S8 b_top = b->pos.z + HEIGHT_INTERVAL - 1;

     if(a_top >= b->pos.z && a_top <= b_top){
          return true;
     }

     if(a->pos.z >= b->pos.z && a->pos.z <= b_top){
          return true;
     }

     return false;
}

Block_t* block_against_another_block(Block_t* block_to_check, Direction_t direction, Block_t* blocks, S16 block_count){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          for(S16 i = 0; i < block_count; i++){
               if(!blocks_at_collidable_height(block_to_check, blocks + i)){
                    continue;
               }

               if((blocks[i].pos.pixel.x + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.x &&
                  blocks[i].pos.pixel.y >= block_to_check->pos.pixel.y &&
                  blocks[i].pos.pixel.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return blocks + i;
               }
          }
          break;
     case DIRECTION_RIGHT:
          for(S16 i = 0; i < block_count; i++){
               if(!blocks_at_collidable_height(block_to_check, blocks + i)){
                    continue;
               }

               if(blocks[i].pos.pixel.x == (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS) &&
                  blocks[i].pos.pixel.y >= block_to_check->pos.pixel.y &&
                  blocks[i].pos.pixel.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return blocks + i;
               }
          }
          break;
     case DIRECTION_DOWN:
          for(S16 i = 0; i < block_count; i++){
               if(!blocks_at_collidable_height(block_to_check, blocks + i)){
                    continue;
               }

               if((blocks[i].pos.pixel.y + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.y &&
                  blocks[i].pos.pixel.x >= block_to_check->pos.pixel.x &&
                  blocks[i].pos.pixel.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return blocks + i;
               }
          }
          break;
     case DIRECTION_UP:
          for(S16 i = 0; i < block_count; i++){
               if(!blocks_at_collidable_height(block_to_check, blocks + i)){
                    continue;
               }

               if(blocks[i].pos.pixel.y == (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS) &&
                  blocks[i].pos.pixel.x >= block_to_check->pos.pixel.x &&
                  blocks[i].pos.pixel.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return blocks + i;
               }
          }
          break;
     }

     return nullptr;
}

Block_t* block_inside_another_block(Block_t* block_to_check, Block_t* blocks, S16 block_count){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
     for(S16 i = 0; i < block_count; i++){
          if(blocks + i == block_to_check) continue;

          Pixel_t top_left {blocks[i].pos.pixel.x, (S16)(blocks[i].pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(blocks[i].pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), (S16)(blocks[i].pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(blocks[i].pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), blocks[i].pos.pixel.y};

          if(pixel_in_rect(blocks[i].pos.pixel, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               return blocks + i;
          }
     }

     return nullptr;
}

Block_t* block_held_up_by_another_block(Block_t* block_to_check, Block_t* blocks, S16 block_count){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
     S8 held_at_height = block_to_check->pos.z - HEIGHT_INTERVAL;
     for(S16 i = 0; i < block_count; i++){
          if(blocks + i == block_to_check || blocks[i].pos.z != held_at_height) continue;

          Pixel_t top_left {blocks[i].pos.pixel.x, (S16)(blocks[i].pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(blocks[i].pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), (S16)(blocks[i].pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(blocks[i].pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), blocks[i].pos.pixel.y};

          if(pixel_in_rect(blocks[i].pos.pixel, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               return blocks + i;
          }
     }

     return nullptr;
}

bool block_adjacent_pixels_to_check(Block_t* block_to_check, Direction_t direction, Pixel_t* a, Pixel_t* b){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.x--;
          *a = pixel;

          // top corner
          pixel.y += (TILE_SIZE_IN_PIXELS - 1);
          *b = pixel;
          return true;
     };
     case DIRECTION_RIGHT:
     {
          // check bottom corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.x += TILE_SIZE_IN_PIXELS;
          *a = pixel;

          // check top corner
          pixel.y += (TILE_SIZE_IN_PIXELS - 1);
          *b = pixel;
          return true;
     };
     case DIRECTION_DOWN:
     {
          // check left corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.y--;
          *a = pixel;

          // check right corner
          pixel.x += (TILE_SIZE_IN_PIXELS - 1);
          *b = pixel;
          return true;
     };
     case DIRECTION_UP:
     {
          // check left corner
          Pixel_t pixel = block_to_check->pos.pixel;
          pixel.y += TILE_SIZE_IN_PIXELS;
          *a = pixel;

          // check right corner
          pixel.x += (TILE_SIZE_IN_PIXELS - 1);
          *b = pixel;
          return true;
     };
     }

     return false;
}

Tile_t* block_against_solid_tile(Block_t* block_to_check, Direction_t direction, TileMap_t* tilemap){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Tile_t* tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

     tile_coord = pixel_to_coord(pixel_b);
     tile = tilemap_get_tile(tilemap, tile_coord);
     if(tile && tile->id) return tile;

     return nullptr;
}

Vec_t collide_circle_with_line(Vec_t circle_center, F32 circle_radius, Vec_t a, Vec_t b, bool* collided){
     // move data we care about to the origin
     Vec_t c = circle_center - a;
     Vec_t l = b - a;
     Vec_t closest_point_on_l_to_c = vec_project_onto(c, l);
     Vec_t collision_to_c = closest_point_on_l_to_c - c;

     if(vec_magnitude(collision_to_c) < circle_radius){
          // find edge of circle in that direction
          Vec_t edge_of_circle = vec_normalize(collision_to_c) * circle_radius;
          *collided = true;
          return collision_to_c - edge_of_circle;
     }

     return vec_zero();
}

S16 range_passes_tile_boundary(S16 a, S16 b, S16 ignore){
     if(a == b) return 0;
     if(a > b){
          if((b % TILE_SIZE_IN_PIXELS) == 0) return 0;
          SWAP(a, b);
     }

     for(S16 i = a; i <= b; i++){
          if((i % TILE_SIZE_IN_PIXELS) == 0 && i != ignore){
               return i;
          }
     }

     return 0;
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

#define THEME_FRAMES_WIDE 16
#define THEME_FRAMES_TALL 32
#define THEME_FRAME_WIDTH 0.0625f
#define THEME_FRAME_HEIGHT 0.03125f

Vec_t theme_frame(S16 x, S16 y){
     y = (THEME_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * THEME_FRAME_WIDTH, (F32)(y) * THEME_FRAME_HEIGHT};
}

#define PLAYER_FRAME_WIDTH 0.25f
#define PLAYER_FRAME_HEIGHT 0.03125f
#define PLAYER_FRAMES_WIDE 4
#define PLAYER_FRAMES_TALL 32

// new quakelive bot settings
// bot_thinktime
// challenge mode

Vec_t player_frame(S8 x, S8 y){
     y = (PLAYER_FRAMES_TALL - 1) - y;
     return Vec_t{(F32)(x) * PLAYER_FRAME_WIDTH, (F32)(y) * PLAYER_FRAME_HEIGHT};
}

struct Lift_t{
     U8 ticks; // start at 1
     bool up;
     F32 timer;
};

void lift_update(Lift_t* lift, float tick_delay, float dt, S8 min_tick, S8 max_tick)
{
     lift->timer += dt;

     if(lift->timer > tick_delay){
          lift->timer -= tick_delay;

          if(lift->up){
               if(lift->ticks < max_tick) lift->ticks++;
          }else{
               if(lift->ticks > min_tick) lift->ticks--;
          }
     }
}

#define POPUP_TICK_DELAY 0.1f

enum InteractiveType_t{
     INTERACTIVE_TYPE_PRESSURE_PLATE,
     INTERACTIVE_TYPE_WIRE_CLUSTER,
     INTERACTIVE_TYPE_LIGHT_DETECTOR,
     INTERACTIVE_TYPE_ICE_DETECTOR,
     INTERACTIVE_TYPE_POPUP,
     INTERACTIVE_TYPE_LEVER,
     INTERACTIVE_TYPE_DOOR,
     INTERACTIVE_TYPE_PORTAL,
     INTERACTIVE_TYPE_BOW,
     INTERACTIVE_TYPE_STAIRS,
     INTERACTIVE_TYPE_PROMPT,
};

enum WireClusterConnection_t : U8{
     WIRE_CLUSTER_CONNECTION_NONE,
     WIRE_CLUSTER_CONNECTION_OFF,
     WIRE_CLUSTER_CONNECTION_ON,
};

struct PressurePlate_t{
     bool down;
     bool iced_under;
};

struct WireCluster_t{
     Direction_t facing;
     WireClusterConnection_t left;
     WireClusterConnection_t mid;
     WireClusterConnection_t right;
};

struct Detector_t{
     bool on;
};

struct Popup_t{
     Lift_t lift;
     bool iced;
};

struct Stairs_t{
     bool up;
     Direction_t facing;
};

struct Lever_t{
     Direction_t activated_from;
     S8 ticks;
     F32 timer;
};

struct Door_t{
     Lift_t lift;
     Direction_t face;
};

struct Portal_t{
     Direction_t face;
     bool on;
};

struct Interactive_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          WireCluster_t wire_cluster;
          Detector_t detector;
          Popup_t popup;
          Stairs_t stairs;
          Lever_t lever;
          Door_t door;
          Portal_t portal;
     };
};

Interactive_t* interactive_get_at(Interactive_t* interactives, U16 interactive_count, Coord_t coord){
     for(U16 i = 0; i < interactive_count; i++){
          if(interactives[i].coord == coord){
               return interactives + i;
          }
     }

     return NULL;
}

bool interactive_is_solid(Interactive_t* interactive){
     return (interactive->type == INTERACTIVE_TYPE_LEVER ||
             (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks > 1));
}

bool interactive_solid_at(Interactive_t* interactives, U16 interactive_count, Coord_t coord){
     Interactive_t* interactive = interactive_get_at(interactives, interactive_count, coord);
     return (interactive && interactive_is_solid(interactive));
}

struct InteractiveQuadTreeBounds_t{
     Coord_t min;
     Coord_t max;
};

bool interactive_quad_tree_bounds_contains(const InteractiveQuadTreeBounds_t* bounds, Coord_t coord){
     return (coord.x >= bounds->min.x && coord.x <= bounds->max.x &&
             coord.y >= bounds->min.y && coord.y <= bounds->max.y);
}

#define INTERACTIVE_QUAD_TREE_NODE_ENTRY_COUNT 4

struct InteractiveQuadTreeNode_t{
     Interactive_t* entries[INTERACTIVE_QUAD_TREE_NODE_ENTRY_COUNT];
     S8 entry_count;

     InteractiveQuadTreeBounds_t bounds;

     InteractiveQuadTreeNode_t* bottom_left;
     InteractiveQuadTreeNode_t* bottom_right;
     InteractiveQuadTreeNode_t* top_left;
     InteractiveQuadTreeNode_t* top_right;
};

bool interactive_quad_tree_insert(InteractiveQuadTreeNode_t* node, Interactive_t* interactive);

bool interactive_quad_tree_subdivide(InteractiveQuadTreeNode_t* node){
     node->bottom_left = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*node)));
     if(!node->bottom_left) return false;

     node->bottom_right = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*node)));
     if(!node->bottom_right) return false;

     node->top_left = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*node)));
     if(!node->top_left) return false;

     node->top_right = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*node)));
     if(!node->top_right) return false;

     int half_width = (node->bounds.max.x - node->bounds.min.x) / 2;
     int half_height = (node->bounds.max.y - node->bounds.min.y) / 2;

     node->bottom_left->bounds.min.x = node->bounds.min.x;
     node->bottom_left->bounds.max.x = node->bounds.min.x + half_width;
     node->bottom_left->bounds.min.y = node->bounds.min.y;
     node->bottom_left->bounds.max.y = node->bounds.min.y + half_height;

     node->bottom_right->bounds.min.x = node->bottom_left->bounds.max.x + 1;
     node->bottom_right->bounds.max.x = node->bounds.max.x;
     node->bottom_right->bounds.min.y = node->bounds.min.y;
     node->bottom_right->bounds.max.y = node->bounds.min.y + half_height;

     node->top_left->bounds.min.x = node->bounds.min.x;
     node->top_left->bounds.max.x = node->bounds.min.x + half_width;
     node->top_left->bounds.min.y = node->bottom_left->bounds.max.y + 1;
     node->top_left->bounds.max.y = node->bounds.max.y;

     node->top_right->bounds.min.x = node->bottom_right->bounds.min.x;
     node->top_right->bounds.max.x = node->bottom_right->bounds.max.x;
     node->top_right->bounds.min.y = node->bottom_left->bounds.max.y + 1;
     node->top_right->bounds.max.y = node->bounds.max.y;

     for(S8 i = 0; i < node->entry_count; i++){
          if(interactive_quad_tree_insert(node->bottom_left, node->entries[i])) continue;
          if(interactive_quad_tree_insert(node->bottom_right, node->entries[i])) continue;
          if(interactive_quad_tree_insert(node->top_left, node->entries[i])) continue;
          if(interactive_quad_tree_insert(node->top_right, node->entries[i])) continue;
     }

     node->entry_count = 0;
     return true;
}

bool interactive_quad_tree_insert(InteractiveQuadTreeNode_t* node, Interactive_t* interactive){
     if(!interactive_quad_tree_bounds_contains(&node->bounds, interactive->coord)) return false;

     if(node->entry_count == 0 && node->bottom_left){
          // pass if the node is empty and has been subdivided
     }else{
          if(node->entry_count < INTERACTIVE_QUAD_TREE_NODE_ENTRY_COUNT){
               node->entries[node->entry_count] = interactive;
               node->entry_count++;
               return true;
          }
     }

     if(!node->bottom_left){
          if(!interactive_quad_tree_subdivide(node)) return false; // nomem
     }

     if(interactive_quad_tree_insert(node->bottom_left, interactive)) return true;
     if(interactive_quad_tree_insert(node->bottom_right, interactive)) return true;
     if(interactive_quad_tree_insert(node->top_left, interactive)) return true;
     if(interactive_quad_tree_insert(node->top_right, interactive)) return true;

     return false;
}

InteractiveQuadTreeNode_t* interactive_quad_tree_build(Interactive_t* interactives, int interactive_count){
     if(interactive_count == 0) return nullptr;

     InteractiveQuadTreeNode_t* root = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*root)));
     root->bounds.min.x = interactives[0].coord.x;
     root->bounds.max.x = interactives[0].coord.x;
     root->bounds.min.y = interactives[0].coord.y;
     root->bounds.max.y = interactives[0].coord.y;

     // find mins/maxs for dimensions
     for(int i = 0; i < interactive_count; i++){
          if(root->bounds.min.x > interactives[i].coord.x) root->bounds.min.x = interactives[i].coord.x;
          if(root->bounds.max.x < interactives[i].coord.x) root->bounds.max.x = interactives[i].coord.x;
          if(root->bounds.min.y > interactives[i].coord.y) root->bounds.min.y = interactives[i].coord.y;
          if(root->bounds.max.y < interactives[i].coord.y) root->bounds.max.y = interactives[i].coord.y;
     }

     // insert coords
     for(int i = 0; i < interactive_count; i++){
          if(!interactive_quad_tree_insert(root, interactives + i)) break;
     }

     return root;
}

Interactive_t* interactive_quad_tree_find_at(const InteractiveQuadTreeNode_t* node, Coord_t coord){
     if(!interactive_quad_tree_bounds_contains(&node->bounds, coord)) return nullptr;

     for(S8 i = 0; i < node->entry_count; i++){
          if(node->entries[i]->coord == coord) return node->entries[i];
     }

     if(node->bottom_left){
          Interactive_t* result = interactive_quad_tree_find_at(node->bottom_left, coord);
          if(result) return result;
          result = interactive_quad_tree_find_at(node->bottom_right, coord);
          if(result) return result;
          result = interactive_quad_tree_find_at(node->top_left, coord);
          if(result) return result;
          result = interactive_quad_tree_find_at(node->top_right, coord);
          if(result) return result;
     }

     return nullptr;
}

void interactive_quad_tree_free(InteractiveQuadTreeNode_t* root){
     if(root->top_left) interactive_quad_tree_free(root->top_left);
     if(root->top_right) interactive_quad_tree_free(root->top_right);
     if(root->bottom_left) interactive_quad_tree_free(root->bottom_left);
     if(root->bottom_right) interactive_quad_tree_free(root->bottom_right);
     free(root);
}

void toggle_electricity(TileMap_t* tilemap, InteractiveQuadTreeNode_t* interactive_quad_tree,  Coord_t coord, Direction_t direction){
     Coord_t adjacent_coord = coord + direction;
     Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
     if(!tile) return;

     Interactive_t* interactive = interactive_quad_tree_find_at(interactive_quad_tree, adjacent_coord);
     if(interactive){
          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive->popup.lift.up = !interactive->popup.lift.up;
               break;
          }
     }

     switch(direction){
     default:
          return;
     case DIRECTION_LEFT:
          if(!(tile->flags & (TILE_FLAG_WIRE_RIGHT_OFF | TILE_FLAG_WIRE_RIGHT_ON))){
               return;
          }
          break;
     case DIRECTION_RIGHT:
          if(!(tile->flags & (TILE_FLAG_WIRE_LEFT_OFF | TILE_FLAG_WIRE_LEFT_ON))){
               return;
          }
          break;
     case DIRECTION_UP:
          if(!(tile->flags & (TILE_FLAG_WIRE_DOWN_OFF | TILE_FLAG_WIRE_DOWN_ON))){
               return;
          }
          break;
     case DIRECTION_DOWN:
          if(!(tile->flags & (TILE_FLAG_WIRE_UP_OFF | TILE_FLAG_WIRE_UP_ON))){
               return;
          }
          break;
     }

     S16 flags = 0;

     if(tile->flags >= TILE_FLAG_WIRE_LEFT_ON){
          flags = tile->flags >> 7;
          S16 new_flags = tile->flags & ~(TILE_FLAG_WIRE_LEFT_ON | TILE_FLAG_WIRE_UP_ON | TILE_FLAG_WIRE_RIGHT_ON | TILE_FLAG_WIRE_DOWN_ON);
          new_flags |= flags << 3;
          tile->flags = new_flags;
     }else if(tile->flags >= TILE_FLAG_WIRE_LEFT_OFF){
          flags = tile->flags >> 3;
          S16 new_flags = tile->flags & ~(TILE_FLAG_WIRE_LEFT_OFF | TILE_FLAG_WIRE_UP_OFF | TILE_FLAG_WIRE_RIGHT_OFF | TILE_FLAG_WIRE_DOWN_OFF);
          new_flags |= flags << 7;
          tile->flags = new_flags;
     }

     if(flags & DIRECTION_MASK_LEFT && direction != DIRECTION_RIGHT){
          toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_LEFT);
     }

     if(flags & DIRECTION_MASK_RIGHT && direction != DIRECTION_LEFT){
          toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_RIGHT);
     }

     if(flags & DIRECTION_MASK_DOWN && direction != DIRECTION_UP){
          toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_DOWN);
     }

     if(flags & DIRECTION_MASK_UP && direction != DIRECTION_DOWN){
          toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_UP);
     }
}

void activate(TileMap_t* tilemap, InteractiveQuadTreeNode_t* interactive_quad_tree, Coord_t coord){
     Interactive_t* interactive = interactive_quad_tree_find_at(interactive_quad_tree, coord);
     if(!interactive) return;

     if(interactive->type != INTERACTIVE_TYPE_LEVER &&
        interactive->type != INTERACTIVE_TYPE_PRESSURE_PLATE) return;

     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_LEFT);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_RIGHT);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_UP);
     toggle_electricity(tilemap, interactive_quad_tree, coord, DIRECTION_DOWN);
}

Interactive_t* block_against_solid_interactive(Block_t* block_to_check, Direction_t direction,
                                               Interactive_t* interactives, S16 interactive_count){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Interactive_t* interactive = interactive_get_at(interactives, interactive_count, tile_coord);
     if(interactive && interactive_is_solid(interactive)) return interactive;

     tile_coord = pixel_to_coord(pixel_b);
     interactive = interactive_get_at(interactives, interactive_count, tile_coord);
     if(interactive && interactive_is_solid(interactive)) return interactive;

     return nullptr;
}


void block_push(Block_t* block, Direction_t direction, TileMap_t* tilemap, Interactive_t* interactives, S16 interactive_count,
                Block_t* blocks, S16 block_count, bool pushed_by_player){
     Block_t* against_block = block_against_another_block(block, direction, blocks, block_count);
     if(against_block){
          if(!pushed_by_player && block_on_ice(against_block, tilemap)){
               block_push(against_block, direction, tilemap, interactives, interactive_count, blocks, block_count, false);
          }

          return;
     }

     if(block_against_solid_tile(block, direction, tilemap)) return;
     if(block_against_solid_interactive(block, direction, interactives, interactive_count)) return;

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          block->accel.x = -PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_RIGHT:
          block->accel.x = PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_DOWN:
          block->accel.y = -PLAYER_SPEED * 0.99f;
          break;
     case DIRECTION_UP:
          block->accel.y = PLAYER_SPEED * 0.99f;
          break;
     }

     block->push_start = block->pos.pixel;
}

void player_collide_coord(Position_t player_pos, Coord_t coord, F32 player_radius, Vec_t* pos_delta, bool* collide_with_coord){
     Coord_t player_coord = pos_to_coord(player_pos);
     Position_t relative = coord_to_pos(coord) - player_pos;
     Vec_t bottom_left = pos_to_vec(relative);
     Vec_t top_left {bottom_left.x, bottom_left.y + TILE_SIZE};
     Vec_t top_right {bottom_left.x + TILE_SIZE, bottom_left.y + TILE_SIZE};
     Vec_t bottom_right {bottom_left.x + TILE_SIZE, bottom_left.y};

     DirectionMask_t mask = directions_between(coord, player_coord);

     // TODO: figure out slide when on tile boundaries
     switch((U8)(mask)){
     default:
          break;
     case DIRECTION_MASK_LEFT:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     case DIRECTION_MASK_LEFT | DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_LEFT | DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, top_left, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, top_left, top_right, collide_with_coord);
          break;
     case DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN:
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_right, top_right, collide_with_coord);
          *pos_delta += collide_circle_with_line(*pos_delta, player_radius, bottom_left, bottom_right, collide_with_coord);
          break;
     }

}
enum PlayerActionType_t{
     PLAYER_ACTION_TYPE_MOVE_LEFT_START,
     PLAYER_ACTION_TYPE_MOVE_LEFT_STOP,
     PLAYER_ACTION_TYPE_MOVE_UP_START,
     PLAYER_ACTION_TYPE_MOVE_UP_STOP,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_START,
     PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP,
     PLAYER_ACTION_TYPE_MOVE_DOWN_START,
     PLAYER_ACTION_TYPE_MOVE_DOWN_STOP,
     PLAYER_ACTION_TYPE_ACTIVATE_START,
     PLAYER_ACTION_TYPE_ACTIVATE_STOP,
     PLAYER_ACTION_TYPE_SHOOT_START,
     PLAYER_ACTION_TYPE_SHOOT_STOP,
};

struct PlayerAction_t{
     bool move_left;
     bool move_right;
     bool move_up;
     bool move_down;
     bool activate;
     bool last_activate;
     bool reface;
};

enum DemoMode_t{
     DEMO_MODE_NONE,
     DEMO_MODE_PLAY,
     DEMO_MODE_RECORD,
};

struct DemoEntry_t{
     S64 frame;
     PlayerActionType_t player_action_type;
};

void demo_entry_get(DemoEntry_t* demo_entry, FILE* file){
     size_t read_count = fread(demo_entry, sizeof(*demo_entry), 1, file);
     if(read_count != 1) demo_entry->frame = (S64)(-1);
}

void player_action_perform(PlayerAction_t* player_action, Player_t* player, PlayerActionType_t player_action_type,
                           DemoMode_t demo_mode, FILE* demo_file, S64 frame_count){
     switch(player_action_type){
     default:
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_START:
          player_action->move_left = true;
          player->face = DIRECTION_LEFT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_LEFT_STOP:
          player_action->move_left = false;
          if(player->face == DIRECTION_LEFT) player_action->reface = DIRECTION_LEFT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_START:
          player_action->move_up = true;
          player->face = DIRECTION_UP;
          break;
     case PLAYER_ACTION_TYPE_MOVE_UP_STOP:
          player_action->move_up = false;
          if(player->face == DIRECTION_UP) player_action->reface = DIRECTION_UP;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_START:
          player_action->move_right = true;
          player->face = DIRECTION_RIGHT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP:
          player_action->move_right = false;
          if(player->face == DIRECTION_RIGHT) player_action->reface = DIRECTION_RIGHT;
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_START:
          player_action->move_down = true;
          player->face = DIRECTION_DOWN;
          break;
     case PLAYER_ACTION_TYPE_MOVE_DOWN_STOP:
          player_action->move_down = false;
          if(player->face == DIRECTION_DOWN) player_action->reface = DIRECTION_DOWN;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_START:
          player_action->activate = true;
          break;
     case PLAYER_ACTION_TYPE_ACTIVATE_STOP:
          player_action->activate = false;
          break;
     }

     if(demo_mode == DEMO_MODE_RECORD){
          DemoEntry_t demo_entry {frame_count, player_action_type};
          fwrite(&demo_entry, sizeof(demo_entry), 1, demo_file);
     }
}

using namespace std::chrono;

int main(int argc, char** argv){
     DemoMode_t demo_mode = DEMO_MODE_NONE;
     const char* demo_filepath = NULL;

     for(int i = 1; i < argc; i++){
          if(strcmp(argv[i], "-play") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_PLAY;
          }else if(strcmp(argv[i], "-record") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_RECORD;
          }
     }

     const char* log_path = "bryte.log";
     if(!Log_t::create(log_path)){
          fprintf(stderr, "failed to create log file: '%s'\n", log_path);
          return -1;
     }

     if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0){
          return 1;
     }

     SDL_DisplayMode display_mode;

     if(SDL_GetCurrentDisplayMode(0, &display_mode) != 0){
          return 1;
     }

     int window_width = 1280;
     int window_height = 1024;

     SDL_Window* window = SDL_CreateWindow("bryte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL);
     if(!window) return 1;

     SDL_GLContext opengl_context = SDL_GL_CreateContext(window);

     SDL_GL_SetSwapInterval(SDL_TRUE);
     glViewport(0, 0, window_width, window_height);
     glClearColor(0.0, 0.0, 0.0, 1.0);
     glEnable(GL_TEXTURE_2D);
     glViewport(0, 0, window_width, window_height);
     glMatrixMode(GL_PROJECTION);
     glLoadIdentity();
     glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
     glMatrixMode(GL_MODELVIEW);
     glLoadIdentity();
     glEnable(GL_BLEND);
     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
     glBlendEquation(GL_FUNC_ADD);

     GLuint theme_texture = transparent_texture_from_file("theme.bmp");
     if(theme_texture == 0) return 1;
     GLuint player_texture = transparent_texture_from_file("player.bmp");
     if(player_texture == 0) return 1;

     Rect_t rooms[2];
     {
          rooms[0].left = 0;
          rooms[0].bottom = 0;
          rooms[0].top = 16;
          rooms[0].right = 16;

          rooms[1].left = 17;
          rooms[1].bottom = 5;
          rooms[1].top = 15;
          rooms[1].right = 27;
     }

     const U16 block_count = 3;
     Block_t blocks[block_count];
     Block_t* sorted_blocks[block_count];
     memset(blocks, 0, sizeof(blocks));
     {
          blocks[0].pos = coord_to_pos(Coord_t{6, 6});
          blocks[1].pos = coord_to_pos(Coord_t{6, 2});
          blocks[2].pos = coord_to_pos(Coord_t{8, 8});
          blocks[0].vel = vec_zero();
          blocks[1].vel = vec_zero();
          blocks[2].vel = vec_zero();
          blocks[0].accel = vec_zero();
          blocks[1].accel = vec_zero();
          blocks[2].accel = vec_zero();

          for(U16 i = 0; i < block_count; ++i){
               sorted_blocks[i] = blocks + i;
          }
     }

     const U16 interactive_count = 4;
     Interactive_t interactives[interactive_count];
     memset(interactives, 0, sizeof(interactives));
     {
          interactives[0].type = INTERACTIVE_TYPE_LEVER;
          interactives[0].coord = Coord_t{3, 9};
          interactives[1].type = INTERACTIVE_TYPE_POPUP;
          interactives[1].coord = Coord_t{5, 11};
          interactives[1].popup.lift.ticks = 1;
          interactives[2].type = INTERACTIVE_TYPE_POPUP;
          interactives[2].coord = Coord_t{9, 2};
          interactives[2].popup.lift.ticks = HEIGHT_INTERVAL + 1;
          interactives[2].popup.lift.up = true;
          interactives[3].type = INTERACTIVE_TYPE_PRESSURE_PLATE;
          interactives[3].coord = Coord_t{3, 6};
     }

     auto* interactive_quad_tree = interactive_quad_tree_build(interactives, interactive_count);

     TileMap_t tilemap;
     {
          init(&tilemap, 34, 17);

          for(U16 i = 0; i < 17; i++){
               tilemap.tiles[0][i].id = 1;
               tilemap.tiles[tilemap.height - 1][i].id = 1;
          }

          for(U16 i = 0; i < 10; i++){
               tilemap.tiles[5][17 + i].id = 1;
               tilemap.tiles[15][17 + i].id = 1;
          }

          for(U16 i = 0; i < tilemap.height; i++){
               tilemap.tiles[i][0].id = 1;
               tilemap.tiles[i][16].id = 1;
               tilemap.tiles[i][17].id = 1;
          }

          for(U16 i = 0; i < 10; i++){
               tilemap.tiles[5 + i][27].id = 1;
          }

          tilemap.tiles[10][17].id = 0;
          tilemap.tiles[10][16].id = 0;

          tilemap.tiles[3][4].id = 1;
          tilemap.tiles[4][5].id = 1;

          tilemap.tiles[8][4].id = 1;
          tilemap.tiles[8][5].id = 1;

          tilemap.tiles[11][10].id = 1;
          tilemap.tiles[12][10].id = 1;

          tilemap.tiles[5][10].id = 1;
          tilemap.tiles[7][10].id = 1;

          tilemap.tiles[5][12].id = 1;
          tilemap.tiles[7][12].id = 1;

          tilemap.tiles[2][14].id = 1;

          tilemap.tiles[8][22].id = 1;
          tilemap.tiles[9][23].id = 1;

          tilemap.tiles[8][6].flags |= TILE_FLAG_ICED;
          tilemap.tiles[9][6].flags |= TILE_FLAG_ICED;
          tilemap.tiles[10][6].flags |= TILE_FLAG_ICED;
          tilemap.tiles[8][7].flags |= TILE_FLAG_ICED;
          tilemap.tiles[9][7].flags |= TILE_FLAG_ICED;
          tilemap.tiles[10][7].flags |= TILE_FLAG_ICED;
          tilemap.tiles[8][8].flags |= TILE_FLAG_ICED;
          tilemap.tiles[9][8].flags |= TILE_FLAG_ICED;
          tilemap.tiles[10][8].flags |= TILE_FLAG_ICED;

          tilemap.tiles[10][3].flags |= TILE_FLAG_WIRE_UP_OFF;
          tilemap.tiles[10][3].flags |= TILE_FLAG_WIRE_DOWN_OFF;
          tilemap.tiles[11][3].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
          tilemap.tiles[11][3].flags |= TILE_FLAG_WIRE_DOWN_OFF;
          tilemap.tiles[11][4].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[11][4].flags |= TILE_FLAG_WIRE_RIGHT_OFF;

          tilemap.tiles[5][3].flags |= TILE_FLAG_WIRE_UP_OFF;
          tilemap.tiles[5][3].flags |= TILE_FLAG_WIRE_DOWN_OFF;
          tilemap.tiles[4][3].flags |= TILE_FLAG_WIRE_UP_OFF;
          tilemap.tiles[4][3].flags |= TILE_FLAG_WIRE_DOWN_OFF;
          tilemap.tiles[3][3].flags |= TILE_FLAG_WIRE_UP_OFF;
          tilemap.tiles[3][3].flags |= TILE_FLAG_WIRE_DOWN_OFF;

          tilemap.tiles[2][3].flags |= TILE_FLAG_WIRE_UP_OFF;
          tilemap.tiles[2][3].flags |= TILE_FLAG_WIRE_RIGHT_OFF;

          tilemap.tiles[2][4].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[2][4].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
          tilemap.tiles[2][5].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[2][5].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
          tilemap.tiles[2][6].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[2][6].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
          tilemap.tiles[2][7].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[2][7].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
          tilemap.tiles[2][8].flags |= TILE_FLAG_WIRE_LEFT_OFF;
          tilemap.tiles[2][8].flags |= TILE_FLAG_WIRE_RIGHT_OFF;
     }

     bool quit = false;

     Vec_t user_movement = {};

     Player_t player {};
     player.pos = coord_to_pos(Coord_t{3, 3});
     player.walk_frame_delta = 1;
     player.radius = 3.5f / 272.0f;

     PlayerAction_t player_action {};

     auto last_time = system_clock::now();
     auto current_time = last_time;

     Position_t camera {};

     Block_t* last_block_pushed = nullptr;
     Direction_t last_block_pushed_direction = DIRECTION_LEFT;
     Block_t* block_to_push = nullptr;

     FILE* demo_file = NULL;
     DemoEntry_t demo_entry {};
     switch(demo_mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          demo_file = fopen(demo_filepath, "w");
          // TODO: write header
          break;
     case DEMO_MODE_PLAY:
          demo_file = fopen(demo_filepath, "r");
          // TODO: read header
          demo_entry_get(&demo_entry, demo_file);
          break;
     }

     S64 frame_count = 0;

#ifdef COUNT_FRAMES
     float time_buildup = 0.0f;
     S64 last_frame_count = 0;
#endif

     while(!quit){
          current_time = system_clock::now();
          duration<double> elapsed_seconds = current_time - last_time;
          F64 dt = (F64)(elapsed_seconds.count());

          if(dt < 0.0166666f) continue; // limit 60 fps
          frame_count++;

#ifdef COUNT_FRAMES
          time_buildup += dt;
          if(time_buildup >= 1.0f){
               LOG("FPS: %ld\n", frame_count, frame_count - last_frame_count);
               last_frame_count = frame_count;
               time_buildup -= 1.0f;
          }
#endif

          last_time = current_time;

          last_block_pushed = block_to_push;
          block_to_push = nullptr;

          player_action.last_activate = player_action.activate;
          player_action.reface = false;

          if(demo_mode == DEMO_MODE_PLAY){
               while(frame_count == demo_entry.frame && !feof(demo_file)){
                    player_action_perform(&player_action, &player, demo_entry.player_action_type, demo_mode,
                                          demo_file, frame_count);
                    demo_entry_get(&demo_entry, demo_file);
               }
          }

          SDL_Event sdl_event;
          while(SDL_PollEvent(&sdl_event)){
               switch(sdl_event.type){
               default:
                    break;
               case SDL_KEYDOWN:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    }
                    break;
               case SDL_KEYUP:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    }
                    break;
               }
          }

          // update interactives
          for(S16 i = 0; i < interactive_count; i++){
               if(interactives[i].type == INTERACTIVE_TYPE_POPUP){
                    lift_update(&interactives[i].popup.lift, POPUP_TICK_DELAY, dt, 1, HEIGHT_INTERVAL + 1);
               }else if(interactives[i].type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                    bool should_be_down = false;
                    Coord_t player_coord = pos_to_coord(player.pos);
                    if(interactives[i].coord == player_coord){
                         should_be_down = true;
                    }else{
                         for(U16 b = 0; b < block_count; b++){
                              Coord_t block_coord = block_get_coord(blocks + b);
                              if(interactives[i].coord == block_coord){
                                   should_be_down = true;
                                   break;
                              }
                         }
                    }

                    if(should_be_down != interactives[i].pressure_plate.down){
                         activate(&tilemap, interactive_quad_tree, interactives[i].coord);
                         interactives[i].pressure_plate.down = should_be_down;
                    }
               }
          }

          // update player
          user_movement = vec_zero();

          if(player_action.move_left){
               user_movement += Vec_t{-1, 0};
               if(player_action.reface) player.face = DIRECTION_LEFT;
          }

          if(player_action.move_right){
               user_movement += Vec_t{1, 0};
               if(player_action.reface) player.face = DIRECTION_RIGHT;
          }

          if(player_action.move_up){
               user_movement += Vec_t{0, 1};
               if(player_action.reface) player.face = DIRECTION_UP;
          }

          if(player_action.move_down){
               user_movement += Vec_t{0, -1};
               if(player_action.reface) player.face = DIRECTION_DOWN;
          }

          if(player_action.activate && !player_action.last_activate){
               activate(&tilemap, interactive_quad_tree, pos_to_coord(player.pos) + player.face);
          }

          if(!player_action.move_left && !player_action.move_right && !player_action.move_up && !player_action.move_down){
               player.walk_frame = 1;
          }else{
               player.walk_frame_time += dt;

               if(player.walk_frame_time > PLAYER_WALK_DELAY){
                    if(vec_magnitude(player.vel) > PLAYER_IDLE_SPEED){
                         player.walk_frame_time = 0.0f;

                         player.walk_frame += player.walk_frame_delta;
                         if(player.walk_frame > 2 || player.walk_frame < 0){
                              player.walk_frame = 1;
                              player.walk_frame_delta = -player.walk_frame_delta;
                         }
                    }else{
                         player.walk_frame = 1;
                         player.walk_frame_time = 0.0f;
                    }
               }
          }

          // figure out what room we should focus on
          Vec_t pos_vec = pos_to_vec(player.pos);
          Position_t room_center {};
          for(U32 i = 0; i < ELEM_COUNT(rooms); i++){
               if(coord_in_rect(vec_to_coord(pos_vec), rooms[i])){
                    S16 half_room_width = (rooms[i].right - rooms[i].left) / 2;
                    S16 half_room_height = (rooms[i].top - rooms[i].bottom) / 2;
                    Coord_t room_center_coord {(S16)(rooms[i].left + half_room_width),
                                               (S16)(rooms[i].bottom + half_room_height)};
                    room_center = coord_to_pos(room_center_coord);
                    break;
               }
          }

          Position_t camera_movement = room_center - camera;
          camera += camera_movement * 0.05f;

          float drag = 0.625f;

          // block movement
          for(U16 i = 0; i < block_count; i++){
               Vec_t pos_delta = (blocks[i].accel * dt * dt * 0.5f) + (blocks[i].vel * dt);
               blocks[i].vel += blocks[i].accel * dt;
               blocks[i].vel *= drag;

               // TODO: blocks with velocity need to be checked against other blocks

               Position_t pre_move = blocks[i].pos;
               blocks[i].pos += pos_delta;

               bool stop_on_boundary_x = false;
               bool stop_on_boundary_y = false;
               bool held_up = false;

               Block_t* inside_block = nullptr;

               while((inside_block = block_inside_another_block(blocks + i, blocks, block_count)) && blocks_at_collidable_height(blocks + i, inside_block)){
                    auto quadrant = relative_quadrant(blocks[i].pos.pixel, inside_block->pos.pixel);

                    switch(quadrant){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         blocks[i].pos.pixel.x = inside_block->pos.pixel.x + TILE_SIZE_IN_PIXELS;
                         blocks[i].pos.decimal.x = 0.0f;
                         blocks[i].vel.x = 0.0f;
                         blocks[i].accel.x = 0.0f;
                         break;
                    case DIRECTION_RIGHT:
                         blocks[i].pos.pixel.x = inside_block->pos.pixel.x - TILE_SIZE_IN_PIXELS;
                         blocks[i].pos.decimal.x = 0.0f;
                         blocks[i].vel.x = 0.0f;
                         blocks[i].accel.x = 0.0f;
                         break;
                    case DIRECTION_DOWN:
                         blocks[i].pos.pixel.y = inside_block->pos.pixel.y + TILE_SIZE_IN_PIXELS;
                         blocks[i].pos.decimal.y = 0.0f;
                         blocks[i].vel.y = 0.0f;
                         blocks[i].accel.y = 0.0f;
                         break;
                    case DIRECTION_UP:
                         blocks[i].pos.pixel.y = inside_block->pos.pixel.y - TILE_SIZE_IN_PIXELS;
                         blocks[i].pos.decimal.y = 0.0f;
                         blocks[i].vel.y = 0.0f;
                         blocks[i].accel.y = 0.0f;
                         break;
                    }

                    if(blocks + i == last_block_pushed && quadrant == last_block_pushed_direction){
                         player.push_time = 0.0f;
                    }

                    if(block_on_ice(inside_block, &tilemap) && block_on_ice(blocks + i, &tilemap)){
                         block_push(inside_block, quadrant, &tilemap, interactives, interactive_count,
                                    blocks, block_count, false);
                    }
               }

               // get the current coord of the center of the block
               Pixel_t center = blocks[i].pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               Coord_t coord = pixel_to_coord(center);

               // check for adjacent walls
               if(blocks[i].vel.x > 0.0f){
                    Coord_t check = coord + Coord_t{1, 0};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = interactive_solid_at(interactives, interactive_count, check);
                    }
               }else if(blocks[i].vel.x < 0.0f){
                    Coord_t check = coord + Coord_t{-1, 0};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = interactive_solid_at(interactives, interactive_count, check);
                    }
               }

               if(blocks[i].vel.y > 0.0f){
                    Coord_t check = coord + Coord_t{0, 1};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = interactive_solid_at(interactives, interactive_count, check);
                    }
               }else if(blocks[i].vel.y < 0.0f){
                    Coord_t check = coord + Coord_t{0, -1};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = interactive_solid_at(interactives, interactive_count, check);
                    }
               }

               if(blocks + i != last_block_pushed && !tilemap_is_iced(&tilemap, coord)){
                    stop_on_boundary_x = true;
                    stop_on_boundary_y = true;
               }

               if(stop_on_boundary_x){
                    // stop on tile boundaries separately for each axis
                    S16 boundary_x = range_passes_tile_boundary(pre_move.pixel.x, blocks[i].pos.pixel.x, blocks[i].push_start.x);
                    if(boundary_x){
                         blocks[i].pos.pixel.x = boundary_x;
                         blocks[i].pos.decimal.x = 0.0f;
                         blocks[i].vel.x = 0.0f;
                         blocks[i].accel.x = 0.0f;
                    }
               }

               if(stop_on_boundary_y){
                    S16 boundary_y = range_passes_tile_boundary(pre_move.pixel.y, blocks[i].pos.pixel.y, blocks[i].push_start.y);
                    if(boundary_y){
                         blocks[i].pos.pixel.y = boundary_y;
                         blocks[i].pos.decimal.y = 0.0f;
                         blocks[i].vel.y = 0.0f;
                         blocks[i].accel.y = 0.0f;
                    }
               }

               held_up = block_held_up_by_another_block(blocks + i, blocks, block_count);

               // TODO: should we care about the decimal component of the position ?
               Interactive_t* interactive = interactive_get_at(interactives, interactive_count, coord);
               if(interactive){
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         if(blocks[i].pos.z == interactive->popup.lift.ticks - 2){
                              blocks[i].pos.z++;
                              held_up = true;
                         }else if(blocks[i].pos.z > (interactive->popup.lift.ticks - 1)){
                              blocks[i].fall_time += dt;
                              if(blocks[i].fall_time >= FALL_TIME){
                                   blocks[i].fall_time -= FALL_TIME;
                                   blocks[i].pos.z--;
                              }
                              held_up = true;
                         }else if(blocks[i].pos.z == (interactive->popup.lift.ticks - 1)){
                              held_up = true;
                         }
                    }
               }

               if(!held_up && blocks[i].pos.z > 0){
                    blocks[i].fall_time += dt;
                    if(blocks[i].fall_time >= FALL_TIME){
                         blocks[i].fall_time -= FALL_TIME;
                         blocks[i].pos.z--;
                    }
               }
          }

          // sort blocks
          {
               // TODO: do we ever need a better sort for this?
               // bubble sort
               for(U16 i = 0; i < block_count; ++i){
                    for(U16 j = 0; j < block_count - i - 1; ++j){
                         if(sorted_blocks[j]->pos.pixel.y < sorted_blocks[j + 1]->pos.pixel.y ||
                            (sorted_blocks[j]->pos.pixel.y == sorted_blocks[j + 1]->pos.pixel.y &&
                             sorted_blocks[j]->pos.z < sorted_blocks[j + 1]->pos.z)){
                              // swap
                              Block_t* tmp = sorted_blocks[j];
                              sorted_blocks[j] = sorted_blocks[j + 1];
                              sorted_blocks[j + 1] = tmp;
                         }
                    }
               }
          }

          // player movement
          {
               user_movement = vec_normalize(user_movement);
               player.accel = user_movement * PLAYER_SPEED;

               Vec_t pos_delta = (player.accel * dt * dt * 0.5f) + (player.vel * dt);
               player.vel += player.accel * dt;
               player.vel *= drag;

               if(fabs(vec_magnitude(player.vel)) > PLAYER_SPEED){
                    player.vel = vec_normalize(player.vel) * PLAYER_SPEED;
               }

               // figure out tiles that are close by
               bool collide_with_tile = false;
               Coord_t player_coord = pos_to_coord(player.pos);
               Coord_t min = player_coord - Coord_t{1, 1};
               Coord_t max = player_coord + Coord_t{1, 1};
               S8 player_top = player.pos.z + 2 * HEIGHT_INTERVAL;
               min = coord_clamp_zero_to_dim(min, tilemap.width - 1, tilemap.height - 1);
               max = coord_clamp_zero_to_dim(max, tilemap.width - 1, tilemap.height - 1);

               for(S16 y = min.y; y <= max.y; y++){
                    for(S16 x = min.x; x <= max.x; x++){
                         if(tilemap.tiles[y][x].id){
                              Coord_t coord {x, y};
                              player_collide_coord(player.pos, coord, player.radius, &pos_delta, &collide_with_tile);
                         }
                    }
               }

               for(U16 i = 0; i < block_count; i++){
                    if(sorted_blocks[i]->pos.z >= player_top) continue;

                    bool collide_with_block = false;

                    Position_t relative = sorted_blocks[i]->pos - player.pos;
                    Vec_t bottom_left = pos_to_vec(relative);
                    if(vec_magnitude(bottom_left) > (2 * TILE_SIZE)) continue;

                    Vec_t top_left {bottom_left.x, bottom_left.y + TILE_SIZE};
                    Vec_t top_right {bottom_left.x + TILE_SIZE, bottom_left.y + TILE_SIZE};
                    Vec_t bottom_right {bottom_left.x + TILE_SIZE, bottom_left.y};

                    pos_delta += collide_circle_with_line(pos_delta, player.radius, bottom_left, top_left, &collide_with_block);
                    pos_delta += collide_circle_with_line(pos_delta, player.radius, top_left, top_right, &collide_with_block);
                    pos_delta += collide_circle_with_line(pos_delta, player.radius, bottom_left, bottom_right, &collide_with_block);
                    pos_delta += collide_circle_with_line(pos_delta, player.radius, bottom_right, top_right, &collide_with_block);

                    if(collide_with_block){
                         auto player_quadrant = relative_quadrant(player.pos.pixel, sorted_blocks[i]->pos.pixel +
                                                                  Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
                         if(player_quadrant == player.face &&
                            (user_movement.x != 0.0f || user_movement.y != 0.0f)){ // also check that the player is actually pushing against the block
                              if(block_to_push == nullptr){
                                   block_to_push = sorted_blocks[i];
                                   last_block_pushed_direction = player.face;
                              }
                         }
                    }
               }

               for(U16 i = 0; i < interactive_count; i++){
                    if(interactive_is_solid(interactives + i)){
                         player_collide_coord(player.pos, interactives[i].coord, player.radius, &pos_delta, &collide_with_tile);
                    }
               }

               if(block_to_push){
                    player.push_time += dt;
                    if(player.push_time > BLOCK_PUSH_TIME){
                         block_push(block_to_push, player.face, &tilemap, interactives, interactive_count,
                                    blocks, block_count, true);
                         if(block_to_push->pos.z > 0) player.push_time = -0.5f;
                    }
               }else{
                    player.push_time = 0;
               }

               player.pos += pos_delta;
          }

          glClear(GL_COLOR_BUFFER_BIT);

          Position_t screen_camera = camera - Vec_t{0.5f, 0.5f} + Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};

          // tilemap
          Coord_t min = pos_to_coord(screen_camera);
          Coord_t max = min + Coord_t{ROOM_TILE_SIZE, ROOM_TILE_SIZE};
          min = coord_clamp_zero_to_dim(min, tilemap.width - 1, tilemap.height - 1);
          max = coord_clamp_zero_to_dim(max, tilemap.width - 1, tilemap.height - 1);
          Position_t tile_bottom_left = coord_to_pos(min);
          Vec_t camera_offset = pos_to_vec(tile_bottom_left - screen_camera);
          Vec_t tex_vec;
          Vec_t floor_tex_vec = theme_frame(0, 25);
          Vec_t solid_tex_vec = theme_frame(0, 1);
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
          for(U16 y = min.y; y <= max.y; y++){
               for(U16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = tilemap.tiles[y] + x;

                    if(tile->id){
                         tex_vec = solid_tex_vec;
                    }else{
                         tex_vec = floor_tex_vec;
                    }

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(tile_pos.x, tile_pos.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);

                    if(tile->flags >= TILE_FLAG_WIRE_LEFT_OFF){
                         S16 frame_y = 9;
                         S16 frame_x = 0;

                         if(tile->flags >= TILE_FLAG_WIRE_LEFT_ON){
                              frame_y++;
                              frame_x = tile->flags >> 7;
                         }else{
                              frame_x = tile->flags >> 3;
                         }

                         tex_vec = theme_frame(frame_x, frame_y);

                         glTexCoord2f(tex_vec.x, tex_vec.y);
                         glVertex2f(tile_pos.x, tile_pos.y);
                         glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
                         glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                         glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
                         glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                         glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
                         glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);
                    }
               }
          }
          glEnd();

          // block
          tex_vec = theme_frame(0, 6);
          glBegin(GL_QUADS);
          for(U16 i = 0; i < block_count; i++){
               Position_t block_camera_offset = blocks[i].pos - screen_camera;
               block_camera_offset.pixel.y += blocks[i].pos.z;
               pos_vec = pos_to_vec(block_camera_offset);
               glTexCoord2f(tex_vec.x, tex_vec.y);
               glVertex2f(pos_vec.x, pos_vec.y);
               glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
               glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
               glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
               glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
               glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
               glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
          }
          glEnd();

          // interactive
          glBegin(GL_QUADS);
          for(U16 i = 0; i < interactive_count; i++){
               switch(interactives[i].type){
               default:
                    break;
               case INTERACTIVE_TYPE_PRESSURE_PLATE:
               {
                    int frame_x = 7;
                    if(interactives[i].pressure_plate.down) frame_x++;
                    tex_vec = theme_frame(frame_x, 8);
               } break;
               case INTERACTIVE_TYPE_LEVER:
                    tex_vec = theme_frame(0, 12);
                    break;
               case INTERACTIVE_TYPE_POPUP:
                    tex_vec = theme_frame(interactives[i].popup.lift.ticks - 1, 8);
                    break;
               }

               Position_t interactive_camera_offset = coord_to_pos(interactives[i].coord) - screen_camera;
               pos_vec = pos_to_vec(interactive_camera_offset);
               glTexCoord2f(tex_vec.x, tex_vec.y);
               glVertex2f(pos_vec.x, pos_vec.y);
               glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
               glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
               glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
               glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
               glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
               glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
          }
          glEnd();

          // player circle
          Position_t player_camera_offset = player.pos - screen_camera;
          pos_vec = pos_to_vec(player_camera_offset);

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_LINES);
          if(block_to_push){
               glColor3f(1.0f, 0.0f, 0.0f);
          }else{
               glColor3f(1.0f, 1.0f, 1.0f);
          }
          Vec_t prev_vec {pos_vec.x + player.radius, pos_vec.y};
          S32 segments = 32;
          F32 delta = 3.14159f * 2.0f / (F32)(segments);
          F32 angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * player.radius;
               F32 dy = sin(angle) * player.radius;

               glVertex2f(prev_vec.x, prev_vec.y);
               glVertex2f(pos_vec.x + dx, pos_vec.y + dy);
               prev_vec.x = pos_vec.x + dx;
               prev_vec.y = pos_vec.y + dy;
               angle += delta;
          }
          glEnd();

          S8 player_frame_y = player.face;
          if(player.push_time > FLT_EPSILON) player_frame_y += 4;

          tex_vec = player_frame(player.walk_frame, player_frame_y);

          pos_vec.y += (5.0f * PIXEL_SIZE);
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

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_LINES);
          glColor3f(0.0f, 1.0f, 1.0f);
          for(S16 y = min.y; y <= max.y; y++){
               for(S16 x = min.x; x <= max.x; x++){
                    if(!tilemap_is_iced(&tilemap, Coord_t{x, y})) continue;

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    glVertex2f(tile_pos.x, tile_pos.y);
                    glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);

                    glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);

                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);

                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);
                    glVertex2f(tile_pos.x, tile_pos.y);

               }
          }
          glEnd();

          SDL_GL_SwapWindow(window);
     }

     interactive_quad_tree_free(interactive_quad_tree);

     destroy(&tilemap);

     glDeleteTextures(1, &theme_texture);
     glDeleteTextures(1, &player_texture);

     SDL_GL_DeleteContext(opengl_context);
     SDL_DestroyWindow(window);
     SDL_Quit();

     if(demo_file) fclose(demo_file);

     Log_t::destroy();

     return 0;
}
