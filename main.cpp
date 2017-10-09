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
#include "object_array.h"

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
     Direction_t face;
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

Block_t* block_against_another_block(Block_t* block_to_check, Direction_t direction, ObjectArray_t<Block_t>* block_array){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          for(S16 i = 0; i < block_array->count; i++){
               Block_t* block = block_array->elements + i;
               if(!blocks_at_collidable_height(block_to_check, block)){
                    continue;
               }

               if((block->pos.pixel.x + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.x &&
                  block->pos.pixel.y >= block_to_check->pos.pixel.y &&
                  block->pos.pixel.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_RIGHT:
          for(S16 i = 0; i < block_array->count; i++){
               Block_t* block = block_array->elements + i;
               if(!blocks_at_collidable_height(block_to_check, block)){
                    continue;
               }

               if(block->pos.pixel.x == (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS) &&
                  block->pos.pixel.y >= block_to_check->pos.pixel.y &&
                  block->pos.pixel.y < (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_DOWN:
          for(S16 i = 0; i < block_array->count; i++){
               Block_t* block = block_array->elements + i;
               if(!blocks_at_collidable_height(block_to_check, block)){
                    continue;
               }

               if((block->pos.pixel.y + TILE_SIZE_IN_PIXELS) == block_to_check->pos.pixel.y &&
                  block->pos.pixel.x >= block_to_check->pos.pixel.x &&
                  block->pos.pixel.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     case DIRECTION_UP:
          for(S16 i = 0; i < block_array->count; i++){
               Block_t* block = block_array->elements + i;
               if(!blocks_at_collidable_height(block_to_check, block)){
                    continue;
               }

               if(block->pos.pixel.y == (block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS) &&
                  block->pos.pixel.x >= block_to_check->pos.pixel.x &&
                  block->pos.pixel.x < (block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS)){
                    return block;
               }
          }
          break;
     }

     return nullptr;
}

Block_t* block_inside_another_block(Block_t* block_to_check, ObjectArray_t<Block_t>* block_array){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
     for(S16 i = 0; i < block_array->count; i++){
          if(block_array->elements + i == block_to_check) continue;
          Block_t* block = block_array->elements + i;

          Pixel_t top_left {block->pos.pixel.x, (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), block->pos.pixel.y};

          if(pixel_in_rect(block->pos.pixel, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               return block;
          }
     }

     return nullptr;
}

Block_t* block_held_up_by_another_block(Block_t* block_to_check, ObjectArray_t<Block_t>* block_array){
     // TODO: need more complicated function to detect this
     Rect_t rect = {block_to_check->pos.pixel.x, block_to_check->pos.pixel.y,
                    (S16)(block_to_check->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1),
                    (S16)(block_to_check->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
     S8 held_at_height = block_to_check->pos.z - HEIGHT_INTERVAL;
     for(S16 i = 0; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          if(block == block_to_check || block->pos.z != held_at_height) continue;

          Pixel_t top_left {block->pos.pixel.x, (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t top_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), (S16)(block->pos.pixel.y + TILE_SIZE_IN_PIXELS - 1)};
          Pixel_t bottom_right {(S16)(block->pos.pixel.x + TILE_SIZE_IN_PIXELS - 1), block->pos.pixel.y};

          if(pixel_in_rect(block->pos.pixel, rect) ||
             pixel_in_rect(top_left, rect) ||
             pixel_in_rect(top_right, rect) ||
             pixel_in_rect(bottom_right, rect)){
               return block;
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
     INTERACTIVE_TYPE_NONE,
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
     Direction_t face;
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

bool interactive_is_solid(Interactive_t* interactive){
     return (interactive->type == INTERACTIVE_TYPE_LEVER ||
             (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks > 1));
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

InteractiveQuadTreeNode_t* interactive_quad_tree_build(ObjectArray_t<Interactive_t>* interactive_array){
     if(interactive_array->count == 0) return nullptr;

     InteractiveQuadTreeNode_t* root = (InteractiveQuadTreeNode_t*)(calloc(1, sizeof(*root)));
     root->bounds.min.x = interactive_array->elements[0].coord.x;
     root->bounds.max.x = interactive_array->elements[0].coord.x;
     root->bounds.min.y = interactive_array->elements[0].coord.y;
     root->bounds.max.y = interactive_array->elements[0].coord.y;

     // find mins/maxs for dimensions
     for(int i = 0; i < interactive_array->count; i++){
          if(root->bounds.min.x > interactive_array->elements[i].coord.x) root->bounds.min.x = interactive_array->elements[i].coord.x;
          if(root->bounds.max.x < interactive_array->elements[i].coord.x) root->bounds.max.x = interactive_array->elements[i].coord.x;
          if(root->bounds.min.y > interactive_array->elements[i].coord.y) root->bounds.min.y = interactive_array->elements[i].coord.y;
          if(root->bounds.max.y < interactive_array->elements[i].coord.y) root->bounds.max.y = interactive_array->elements[i].coord.y;
     }

     // insert coords
     for(int i = 0; i < interactive_array->count; i++){
          if(!interactive_quad_tree_insert(root, interactive_array->elements + i)) break;
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

Interactive_t* interactive_quad_tree_solid_at(const InteractiveQuadTreeNode_t* root, Coord_t coord){
     Interactive_t* interactive = interactive_quad_tree_find_at(root, coord);
     if(interactive && interactive_is_solid(interactive)){
          return interactive;
     }

     return nullptr;
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
                                               InteractiveQuadTreeNode_t* interactive_quad_tree){
     Pixel_t pixel_a;
     Pixel_t pixel_b;

     if(!block_adjacent_pixels_to_check(block_to_check, direction, &pixel_a, &pixel_b)){
          return nullptr;
     }

     Coord_t tile_coord = pixel_to_coord(pixel_a);
     Interactive_t* interactive = interactive_quad_tree_solid_at(interactive_quad_tree, tile_coord);
     if(interactive) return interactive;

     tile_coord = pixel_to_coord(pixel_b);
     interactive = interactive_quad_tree_solid_at(interactive_quad_tree, tile_coord);
     if(interactive) return interactive;

     return nullptr;
}

void block_push(Block_t* block, Direction_t direction, TileMap_t* tilemap, InteractiveQuadTreeNode_t* interactive_quad_tree,
                ObjectArray_t<Block_t>* block_array, bool pushed_by_player){
     Block_t* against_block = block_against_another_block(block, direction, block_array);
     if(against_block){
          if(!pushed_by_player && block_on_ice(against_block, tilemap)){
               block_push(against_block, direction, tilemap, interactive_quad_tree, block_array, false);
          }

          return;
     }

     if(block_against_solid_tile(block, direction, tilemap)) return;
     if(block_against_solid_interactive(block, direction, interactive_quad_tree)) return;

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

#define MAP_VERSION 1

#pragma pack(push, 1)
struct MapTileV1_t{
     U8 id;
     U16 flags;
};

struct MapBlockV1_t{
     Pixel_t pixel;
     Direction_t face;
     Element_t element;
     S8 z;
};

struct MapPopupV1_t{
     bool up;
     bool iced;
};

struct MapDoorV1_t{
     bool up;
     Direction_t face;
};

struct MapInteractiveV1_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          WireCluster_t wire_cluster;
          Detector_t detector;
          MapPopupV1_t popup;
          Stairs_t stairs;
          MapDoorV1_t door; // up or down
          Portal_t portal;
     };
};
#pragma pack(pop)

bool save_map(const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array, ObjectArray_t<Interactive_t>* interactive_array,
              const char* filepath){
     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(tilemap->width) * (S32)(tilemap->height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc(map_tile_count, sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc(block_array->count, sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_array->count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc(interactive_array->count, sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_array->count);
          return false;
     }

     // convert to map formats
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               int index = y * tilemap->width + x;
               map_tiles[index].id = tilemap->tiles[y][x].id;
               map_tiles[index].flags = tilemap->tiles[y][x].flags;
          }
     }

     for(S16 i = 0; i < block_array->count; i++){
          Block_t* block = block_array->elements + i;
          map_blocks[i].pixel = block->pos.pixel;
          map_blocks[i].z = block->pos.z;
          map_blocks[i].face = block->face;
          map_blocks[i].element = block->element;
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          map_interactives[i].coord = interactive_array->elements[i].coord;
          map_interactives[i].type = interactive_array->elements[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               map_interactives[i].pressure_plate = interactive_array->elements[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_WIRE_CLUSTER:
               map_interactives[i].wire_cluster = interactive_array->elements[i].wire_cluster;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               map_interactives[i].detector = interactive_array->elements[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               map_interactives[i].popup.up = interactive_array->elements[i].popup.lift.up;
               map_interactives[i].popup.iced = interactive_array->elements[i].popup.iced;
               break;
          case INTERACTIVE_TYPE_DOOR:
               map_interactives[i].door.up = interactive_array->elements[i].door.lift.up;
               map_interactives[i].door.face = interactive_array->elements[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               map_interactives[i].portal.face = interactive_array->elements[i].portal.face;
               map_interactives[i].portal.on = interactive_array->elements[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               map_interactives[i].stairs.up = interactive_array->elements[i].stairs.up;
               map_interactives[i].stairs.face = interactive_array->elements[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_PROMPT:
               break;
          }
     }

     // write to file
     FILE* f = fopen(filepath, "wb");
     if(!f){
          LOG("%s: fopen() failed\n", __FUNCTION__);
          return false;
     }

     U8 map_version = MAP_VERSION;
     fwrite(&map_version, sizeof(map_version), 1, f);
     fwrite(&tilemap->width, sizeof(tilemap->width), 1, f);
     fwrite(&tilemap->height, sizeof(tilemap->height), 1, f);
     fwrite(&block_array->count, sizeof(block_array->count), 1, f);
     fwrite(&interactive_array->count, sizeof(interactive_array->count), 1, f);
     fwrite(map_tiles, sizeof(*map_tiles), map_tile_count, f);
     fwrite(map_blocks, sizeof(*map_blocks), block_array->count, f);
     fwrite(map_interactives, sizeof(*map_interactives), interactive_array->count, f);
     fclose(f);

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

bool load_map(TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array, ObjectArray_t<Interactive_t>* interactive_array,
              const char* filepath){
     // read counts from file
     S16 map_width;
     S16 map_height;
     S16 interactive_count;
     S16 block_count;

     FILE* f = fopen(filepath, "rb");
     if(!f){
          LOG("%s(): fopen() failed\n", __FUNCTION__);
          return false;
     }

     U8 map_version = MAP_VERSION;
     fread(&map_version, sizeof(map_version), 1, f);
     if(map_version != MAP_VERSION){
          LOG("%s(): mismatched version loading '%s', actual %d, expected %d\n", __FUNCTION__, filepath, map_version, MAP_VERSION);
          return false;
     }

     fread(&map_width, sizeof(map_width), 1, f);
     fread(&map_height, sizeof(map_height), 1, f);
     fread(&block_count, sizeof(block_count), 1, f);
     fread(&interactive_count, sizeof(interactive_count), 1, f);

     // alloc and convert map elements to map format
     S32 map_tile_count = (S32)(tilemap->width) * (S32)(tilemap->height);
     MapTileV1_t* map_tiles = (MapTileV1_t*)(calloc(map_tile_count, sizeof(*map_tiles)));
     if(!map_tiles){
          LOG("%s(): failed to allocate %d tiles\n", __FUNCTION__, map_tile_count);
          return false;
     }

     MapBlockV1_t* map_blocks = (MapBlockV1_t*)(calloc(block_count, sizeof(*map_blocks)));
     if(!map_blocks){
          LOG("%s(): failed to allocate %d blocks\n", __FUNCTION__, block_count);
          return false;
     }

     MapInteractiveV1_t* map_interactives = (MapInteractiveV1_t*)(calloc(interactive_array->count, sizeof(*map_interactives)));
     if(!map_interactives){
          LOG("%s(): failed to allocate %d interactives\n", __FUNCTION__, interactive_array->count);
          return false;
     }

     // read data from file
     fread(map_tiles, sizeof(*map_tiles), map_tile_count, f);
     fread(map_blocks, sizeof(*map_blocks), block_count, f);
     fread(map_interactives, sizeof(*map_interactives), interactive_array->count, f);
     fclose(f);

     destroy(tilemap);
     init(tilemap, map_width, map_height);

     destroy(block_array);
     init(block_array, block_count);

     destroy(interactive_array);
     init(interactive_array, interactive_count);

     // convert to map formats
     for(S32 y = 0; y < tilemap->height; y++){
          for(S32 x = 0; x < tilemap->width; x++){
               int index = y * tilemap->width + x;
               tilemap->tiles[y][x].id = map_tiles[index].id;
               tilemap->tiles[y][x].flags = map_tiles[index].flags;
          }
     }

     for(S16 i = 0; i < block_count; i++){
          block_array->elements[i].pos.pixel = map_blocks[i].pixel;
          block_array->elements[i].pos.z = map_blocks[i].z;
          block_array->elements[i].face = map_blocks[i].face;
          block_array->elements[i].element = map_blocks[i].element;
     }

     for(S16 i = 0; i < interactive_array->count; i++){
          interactive_array->elements[i].coord = map_interactives[i].coord;
          interactive_array->elements[i].type = map_interactives[i].type;

          switch(map_interactives[i].type){
          default:
          case INTERACTIVE_TYPE_LEVER:
          case INTERACTIVE_TYPE_BOW:
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               interactive_array->elements[i].pressure_plate = map_interactives[i].pressure_plate;
               break;
          case INTERACTIVE_TYPE_WIRE_CLUSTER:
               interactive_array->elements[i].wire_cluster = map_interactives[i].wire_cluster;
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               interactive_array->elements[i].detector = map_interactives[i].detector;
               break;
          case INTERACTIVE_TYPE_POPUP:
               interactive_array->elements[i].popup.lift.up = map_interactives[i].popup.up;
               interactive_array->elements[i].popup.iced = map_interactives[i].popup.iced;
               if(interactive_array->elements[i].popup.lift.up){
                    interactive_array->elements[i].popup.lift.ticks = HEIGHT_INTERVAL + 1;
               }else{
                    interactive_array->elements[i].popup.lift.ticks = 1;
               }
               break;
          case INTERACTIVE_TYPE_DOOR:
               interactive_array->elements[i].door.lift.up = map_interactives[i].door.up;
               interactive_array->elements[i].door.face = map_interactives[i].door.face;
               break;
          case INTERACTIVE_TYPE_PORTAL:
               interactive_array->elements[i].portal.face = map_interactives[i].portal.face;
               interactive_array->elements[i].portal.on = map_interactives[i].portal.on;
               break;
          case INTERACTIVE_TYPE_STAIRS:
               interactive_array->elements[i].stairs.up = map_interactives[i].stairs.up;
               interactive_array->elements[i].stairs.face = map_interactives[i].stairs.face;
               break;
          case INTERACTIVE_TYPE_PROMPT:
               break;
          }
     }

     free(map_tiles);
     free(map_blocks);
     free(map_interactives);

     return true;
}

enum StampType_t{
     STAMP_TYPE_NONE,
     STAMP_TYPE_TILE_ID,
     STAMP_TYPE_TILE_FLAGS,
     STAMP_TYPE_BLOCK,
     STAMP_TYPE_INTERACTIVE,
};

struct StampBlock_t{
     Element_t element;
     Direction_t face;
};

struct Stamp_t{
     StampType_t type;

     union{
          U8 tile_id;
          U16 tile_flags;
          StampBlock_t block;
          Interactive_t interactive;
     };

     Coord_t offset;
};

enum EditorMode_t : U8{
     EDITOR_MODE_OFF,
     EDITOR_MODE_CATEGORY_SELECT,
     EDITOR_MODE_STAMP_SELECT,
     EDITOR_MODE_STAMP_HIDE,
     EDITOR_MODE_CREATE_SELECTION,
     EDITOR_MODE_SELECTION_MANIPULATION,
};

Coord_t stamp_array_dimensions(ObjectArray_t<Stamp_t>* object_array){
     Coord_t min {0, 0};
     Coord_t max {0, 0};

     for(S32 i = 0; i < object_array->count; ++i){
          Stamp_t* stamp = object_array->elements + i;

          if(stamp->offset.x < min.x){min.x = stamp->offset.x;}
          if(stamp->offset.y < min.y){min.y = stamp->offset.y;}

          if(stamp->offset.x > max.x){max.x = stamp->offset.x;}
          if(stamp->offset.y > max.y){max.y = stamp->offset.y;}
     }

     return (max - min) + Coord_t{1, 1};
}

enum EditorCategory_t : U8{
     EDITOR_CATEGORY_TILE_ID,
     // EDITOR_CATEGORY_TILE_FLAGS,
     EDITOR_CATEGORY_BLOCK,
     // EDITOR_CATEGORY_INTERACTIVE_LEVER,
     EDITOR_CATEGORY_COUNT,
};

struct Editor_t{
     ObjectArray_t<ObjectArray_t<Stamp_t>> category_array;
     EditorMode_t mode = EDITOR_MODE_OFF;

     S32 category = 0;
     S32 stamp = 0;

     Coord_t selection_start;
     Coord_t selection_end;

     Coord_t clipboard_start_offset;
     Coord_t clipboard_end_offset;

     ObjectArray_t<Stamp_t> selection;
     ObjectArray_t<Stamp_t> clipboard;
};

bool init(Editor_t* editor){
     memset(editor, 0, sizeof(*editor));
     init(&editor->category_array, EDITOR_CATEGORY_COUNT);

     // tile
     auto* tile_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_ID;
     init(tile_category, 48);
     for(S16 i = 0; i < tile_category->count; i++){
          tile_category->elements[i].type = STAMP_TYPE_TILE_ID;
          tile_category->elements[i].tile_id = (U8)(i);
     }

     auto* block_category = editor->category_array.elements + EDITOR_CATEGORY_BLOCK;
     init(block_category, 4);
     for(S16 i = 0; i < block_category->count; i++){
          block_category->elements[i].type = STAMP_TYPE_BLOCK;
          block_category->elements[i].block.face = DIRECTION_LEFT;
          block_category->elements[i].block.element = (Element_t)(i);
     }

     return true;
}

void destroy(Editor_t* editor){
     for(S16 i = 0; i < editor->category_array.count; i++){
          destroy(editor->category_array.elements + i);
     }

     destroy(&editor->category_array);
     destroy(&editor->selection);
     destroy(&editor->clipboard);
}

void tile_id_draw(U8 id, Vec_t pos){
     U8 id_x = id % 16;
     U8 id_y = id / 16;

     Vec_t tex_vec = theme_frame(id_x, id_y);

     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos.x, pos.y);
     glTexCoord2f(tex_vec.x, tex_vec.y + THEME_FRAME_HEIGHT);
     glVertex2f(pos.x, pos.y + TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + THEME_FRAME_HEIGHT);
     glVertex2f(pos.x + TILE_SIZE, pos.y + TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos.x + TILE_SIZE, pos.y);

}

void block_draw(Block_t* block, Vec_t pos_vec){
     Vec_t tex_vec = theme_frame(0, 6);
     glTexCoord2f(tex_vec.x, tex_vec.y);
     glVertex2f(pos_vec.x, pos_vec.y);
     glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
     glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
     glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);

     if(block->element == ELEMENT_FIRE){
          tex_vec = theme_frame(1, 6);
          // TODO: compress
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x, pos_vec.y);
          glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
     }else if(block->element == ELEMENT_ICE){
          tex_vec = theme_frame(5, 6);
          // TODO: compress
          glTexCoord2f(tex_vec.x, tex_vec.y);
          glVertex2f(pos_vec.x, pos_vec.y);
          glTexCoord2f(tex_vec.x, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y + 2.0f * THEME_FRAME_HEIGHT);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y + 2.0f * TILE_SIZE);
          glTexCoord2f(tex_vec.x + THEME_FRAME_WIDTH, tex_vec.y);
          glVertex2f(pos_vec.x + TILE_SIZE, pos_vec.y);
     }
}

Coord_t mouse_select_coord(Vec_t mouse_screen)
{
     return {(S16)(mouse_screen.x * (F32)(ROOM_TILE_SIZE)), (S16)(mouse_screen.y * (F32)(ROOM_TILE_SIZE))};
}

S32 mouse_select_index(Vec_t mouse_screen)
{
     Coord_t coord = mouse_select_coord(mouse_screen);
     return coord.y * ROOM_TILE_SIZE + coord.x;
}

Vec_t coord_to_screen_position(Coord_t coord)
{
     Pixel_t pixel = coord_to_pixel(coord);
     Position_t relative_loc {pixel, 0, {0.0f, 0.0f}};
     return pos_to_vec(relative_loc);
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

     ObjectArray_t<Block_t> block_array;
     if(!init(&block_array, 3)){
          return 1;
     }

     Block_t** sorted_blocks;
     {
          block_array.elements[0].pos = coord_to_pos(Coord_t{6, 6});
          block_array.elements[1].pos = coord_to_pos(Coord_t{6, 2});
          block_array.elements[2].pos = coord_to_pos(Coord_t{8, 8});
          block_array.elements[0].vel = vec_zero();
          block_array.elements[1].vel = vec_zero();
          block_array.elements[2].vel = vec_zero();
          block_array.elements[0].accel = vec_zero();
          block_array.elements[1].accel = vec_zero();
          block_array.elements[2].accel = vec_zero();

          sorted_blocks = (Block_t**)(calloc(block_array.count, sizeof(*sorted_blocks)));
          for(S16 i = 0; i < block_array.count; ++i){
               sorted_blocks[i] = block_array.elements + i;
          }
     }

     ObjectArray_t<Interactive_t> interactive_array;
     {
          if(!init(&interactive_array, 4)){
               return 1;
          }

          interactive_array.elements[0].type = INTERACTIVE_TYPE_LEVER;
          interactive_array.elements[0].coord = Coord_t{3, 9};
          interactive_array.elements[1].type = INTERACTIVE_TYPE_POPUP;
          interactive_array.elements[1].coord = Coord_t{5, 11};
          interactive_array.elements[1].popup.lift.ticks = 1;
          interactive_array.elements[2].type = INTERACTIVE_TYPE_POPUP;
          interactive_array.elements[2].coord = Coord_t{9, 2};
          interactive_array.elements[2].popup.lift.ticks = HEIGHT_INTERVAL + 1;
          interactive_array.elements[2].popup.lift.up = true;
          interactive_array.elements[3].type = INTERACTIVE_TYPE_PRESSURE_PLATE;
          interactive_array.elements[3].coord = Coord_t{3, 6};
     }

     auto* interactive_quad_tree = interactive_quad_tree_build(&interactive_array);

#define SOLID_TILE 16

     TileMap_t tilemap;
     {
          init(&tilemap, 34, 17);

          for(S16 i = 0; i < 17; i++){
               tilemap.tiles[0][i].id = SOLID_TILE;
               tilemap.tiles[tilemap.height - 1][i].id = SOLID_TILE;
          }

          for(S16 i = 0; i < 10; i++){
               tilemap.tiles[5][17 + i].id = SOLID_TILE;
               tilemap.tiles[15][17 + i].id = SOLID_TILE;
          }

          for(S16 i = 0; i < tilemap.height; i++){
               tilemap.tiles[i][0].id = SOLID_TILE;
               tilemap.tiles[i][16].id = SOLID_TILE;
               tilemap.tiles[i][17].id = SOLID_TILE;
          }

          for(S16 i = 0; i < 10; i++){
               tilemap.tiles[5 + i][27].id = SOLID_TILE;
          }

          tilemap.tiles[10][17].id = 0;
          tilemap.tiles[10][16].id = 0;

          tilemap.tiles[3][4].id = SOLID_TILE;
          tilemap.tiles[4][5].id = SOLID_TILE;

          tilemap.tiles[8][4].id = SOLID_TILE;
          tilemap.tiles[8][5].id = SOLID_TILE;

          tilemap.tiles[11][10].id = SOLID_TILE;
          tilemap.tiles[12][10].id = SOLID_TILE;

          tilemap.tiles[5][10].id = SOLID_TILE;
          tilemap.tiles[7][10].id = SOLID_TILE;

          tilemap.tiles[5][12].id = SOLID_TILE;
          tilemap.tiles[7][12].id = SOLID_TILE;

          tilemap.tiles[2][14].id = SOLID_TILE;

          tilemap.tiles[8][22].id = SOLID_TILE;
          tilemap.tiles[9][23].id = SOLID_TILE;

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

     // bool left_click_down = false;
     Vec_t mouse_screen = {}; // 0.0f to 1.0f
     Position_t mouse_world = {};

     Editor_t editor;
     init(&editor);

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
          dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.
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
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x--;
                              editor.selection_end.x--;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x++;
                              editor.selection_end.x++;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_UP:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y++;
                              editor.selection_end.y++;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y--;
                              editor.selection_end.y--;
                         }else{
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_START, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_L:
                         if(load_map(&tilemap, &block_array, &interactive_array, "first.bm")){
                              // update sort blocks list
                              free(sorted_blocks);
                              sorted_blocks = (Block_t**)(calloc(block_array.count, sizeof(*sorted_blocks)));
                              for(S16 i = 0; i < block_array.count; ++i){
                                   sorted_blocks[i] = block_array.elements + i;
                              }

                              // update interactive quad tree
                              interactive_quad_tree_free(interactive_quad_tree);
                              interactive_quad_tree = interactive_quad_tree_build(&interactive_array);
                         }
                         break;
                    // TODO: #ifdef DEBUG
                    case SDL_SCANCODE_GRAVE:
                         if(editor.mode == EDITOR_MODE_OFF){
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }else{
                              editor.mode = EDITOR_MODE_OFF;
                              editor.selection_start = {};
                              editor.selection_end = {};
                         }
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
               case SDL_MOUSEBUTTONDOWN:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = true;

                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_OFF:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                         {
                              S32 select_index = mouse_select_index(mouse_screen);
                              if(select_index < EDITOR_CATEGORY_COUNT){
                                   editor.mode = EDITOR_MODE_STAMP_SELECT;
                                   editor.category = select_index;
                                   editor.stamp = 0;
                              }else{
                                   editor.mode = EDITOR_MODE_CREATE_SELECTION;
                                   editor.selection_start = pixel_to_coord(mouse_world.pixel);
                                   editor.selection_end = editor.selection_start;
                              }
                         } break;
                         case EDITOR_MODE_STAMP_SELECT:
                         {
                              S32 select_index = mouse_select_index(mouse_screen);
                              if(select_index < editor.category_array.elements[editor.category].count){
                                   editor.stamp = select_index;
                              }else{
                                   Stamp_t* stamp = editor.category_array.elements[editor.category].elements + editor.stamp;
                                   switch(stamp->type){
                                   default:
                                        break;
                                   case STAMP_TYPE_TILE_ID:
                                   {
                                        Coord_t select_coord = mouse_select_coord(mouse_screen) + (pos_to_coord(camera) - Coord_t{ROOM_TILE_SIZE / 2 - 1, ROOM_TILE_SIZE / 2 - 1});
                                        Tile_t* tile = tilemap_get_tile(&tilemap, select_coord);
                                        if(tile) tile->id = stamp->tile_id;
                                   } break;
                                   case STAMP_TYPE_BLOCK:
                                   {
                                        Coord_t select_coord = mouse_select_coord(mouse_screen) + (pos_to_coord(camera) - Coord_t{ROOM_TILE_SIZE / 2 - 1, ROOM_TILE_SIZE / 2 - 1});
                                        int index = block_array.count;
                                        resize(&block_array, block_array.count + 1);
                                        Block_t* block = block_array.elements + index;
                                        block->pos = coord_to_pos(select_coord);
                                        block->vel = vec_zero();
                                        block->accel = vec_zero();

                                        sorted_blocks = (Block_t**)(realloc(sorted_blocks, block_array.count * sizeof(*sorted_blocks)));
                                        for(S16 i = 0; i < block_array.count; ++i){
                                             sorted_blocks[i] = block_array.elements + i;
                                        }
                                   } break;
                                   }
                              }
                         } break;
                         }
                    }
                    break;
               case SDL_MOUSEBUTTONUP:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = false;
                         break;
                    }
                    break;
               case SDL_MOUSEMOTION:
                    mouse_screen = Vec_t{((F32)(sdl_event.button.x) / (F32)(window_width)),
                                         1.0f - ((F32)(sdl_event.button.y) / (F32)(window_height))};
                    mouse_world = vec_to_pos(mouse_screen);
                    break;
               }
          }

          // update interactives
          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               if(interactive->type == INTERACTIVE_TYPE_POPUP){
                    lift_update(&interactive->popup.lift, POPUP_TICK_DELAY, dt, 1, HEIGHT_INTERVAL + 1);
               }else if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                    bool should_be_down = false;
                    Coord_t player_coord = pos_to_coord(player.pos);
                    if(interactive->coord == player_coord){
                         should_be_down = true;
                    }else{
                         for(S16 b = 0; b < block_array.count; b++){
                              Coord_t block_coord = block_get_coord(block_array.elements + b);
                              if(interactive->coord == block_coord){
                                   should_be_down = true;
                                   break;
                              }
                         }
                    }

                    if(should_be_down != interactive->pressure_plate.down){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->pressure_plate.down = should_be_down;
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
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;

               Vec_t pos_delta = (block->accel * dt * dt * 0.5f) + (block->vel * dt);
               block->vel += block->accel * dt;
               block->vel *= drag;

               // TODO: blocks with velocity need to be checked against other blocks

               Position_t pre_move = block->pos;
               block->pos += pos_delta;

               bool stop_on_boundary_x = false;
               bool stop_on_boundary_y = false;
               bool held_up = false;

               Block_t* inside_block = nullptr;

               while((inside_block = block_inside_another_block(block_array.elements + i, &block_array)) && blocks_at_collidable_height(block, inside_block)){
                    auto quadrant = relative_quadrant(block->pos.pixel, inside_block->pos.pixel);

                    switch(quadrant){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         block->pos.pixel.x = inside_block->pos.pixel.x + TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.x = 0.0f;
                         block->vel.x = 0.0f;
                         block->accel.x = 0.0f;
                         break;
                    case DIRECTION_RIGHT:
                         block->pos.pixel.x = inside_block->pos.pixel.x - TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.x = 0.0f;
                         block->vel.x = 0.0f;
                         block->accel.x = 0.0f;
                         break;
                    case DIRECTION_DOWN:
                         block->pos.pixel.y = inside_block->pos.pixel.y + TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.y = 0.0f;
                         block->vel.y = 0.0f;
                         block->accel.y = 0.0f;
                         break;
                    case DIRECTION_UP:
                         block->pos.pixel.y = inside_block->pos.pixel.y - TILE_SIZE_IN_PIXELS;
                         block->pos.decimal.y = 0.0f;
                         block->vel.y = 0.0f;
                         block->accel.y = 0.0f;
                         break;
                    }

                    if(block == last_block_pushed && quadrant == last_block_pushed_direction){
                         player.push_time = 0.0f;
                    }

                    if(block_on_ice(inside_block, &tilemap) && block_on_ice(block, &tilemap)){
                         block_push(inside_block, quadrant, &tilemap, interactive_quad_tree, &block_array, false);
                    }
               }

               // get the current coord of the center of the block
               Pixel_t center = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               Coord_t coord = pixel_to_coord(center);

               // check for adjacent walls
               if(block->vel.x > 0.0f){
                    Coord_t check = coord + Coord_t{1, 0};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = interactive_quad_tree_solid_at(interactive_quad_tree, check);
                    }
               }else if(block->vel.x < 0.0f){
                    Coord_t check = coord + Coord_t{-1, 0};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = interactive_quad_tree_solid_at(interactive_quad_tree, check);
                    }
               }

               if(block->vel.y > 0.0f){
                    Coord_t check = coord + Coord_t{0, 1};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = interactive_quad_tree_solid_at(interactive_quad_tree, check);
                    }
               }else if(block->vel.y < 0.0f){
                    Coord_t check = coord + Coord_t{0, -1};
                    if(tilemap_is_solid(&tilemap, check)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = interactive_quad_tree_solid_at(interactive_quad_tree, check);
                    }
               }

               if(block != last_block_pushed && !tilemap_is_iced(&tilemap, coord)){
                    stop_on_boundary_x = true;
                    stop_on_boundary_y = true;
               }

               if(stop_on_boundary_x){
                    // stop on tile boundaries separately for each axis
                    S16 boundary_x = range_passes_tile_boundary(pre_move.pixel.x, block->pos.pixel.x, block->push_start.x);
                    if(boundary_x){
                         block->pos.pixel.x = boundary_x;
                         block->pos.decimal.x = 0.0f;
                         block->vel.x = 0.0f;
                         block->accel.x = 0.0f;
                    }
               }

               if(stop_on_boundary_y){
                    S16 boundary_y = range_passes_tile_boundary(pre_move.pixel.y, block->pos.pixel.y, block->push_start.y);
                    if(boundary_y){
                         block->pos.pixel.y = boundary_y;
                         block->pos.decimal.y = 0.0f;
                         block->vel.y = 0.0f;
                         block->accel.y = 0.0f;
                    }
               }

               held_up = block_held_up_by_another_block(block, &block_array);

               // TODO: should we care about the decimal component of the position ?
               Interactive_t* interactive = interactive_quad_tree_find_at(interactive_quad_tree, coord);
               if(interactive){
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         if(block->pos.z == interactive->popup.lift.ticks - 2){
                              block->pos.z++;
                              held_up = true;
                         }else if(block->pos.z > (interactive->popup.lift.ticks - 1)){
                              block->fall_time += dt;
                              if(block->fall_time >= FALL_TIME){
                                   block->fall_time -= FALL_TIME;
                                   block->pos.z--;
                              }
                              held_up = true;
                         }else if(block->pos.z == (interactive->popup.lift.ticks - 1)){
                              held_up = true;
                         }
                    }
               }

               if(!held_up && block->pos.z > 0){
                    block->fall_time += dt;
                    if(block->fall_time >= FALL_TIME){
                         block->fall_time -= FALL_TIME;
                         block->pos.z--;
                    }
               }
          }

          // sort blocks
          {
               // TODO: do we ever need a better sort for this?
               // bubble sort
               for(S16 i = 0; i < block_array.count; ++i){
                    for(S16 j = 0; j < block_array.count - i - 1; ++j){
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

               for(S16 i = 0; i < block_array.count; i++){
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

               for(S16 y = min.y; y <= max.y; y++){
                    for(S16 x = min.x; x <= max.x; x++){
                         Coord_t coord {x, y};
                         if(interactive_quad_tree_solid_at(interactive_quad_tree, coord)){
                              player_collide_coord(player.pos, coord, player.radius, &pos_delta, &collide_with_tile);
                         }
                    }
               }

               if(block_to_push){
                    player.push_time += dt;
                    if(player.push_time > BLOCK_PUSH_TIME){
                         block_push(block_to_push, player.face, &tilemap, interactive_quad_tree, &block_array, true);
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
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
          for(S16 y = min.y; y <= max.y; y++){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = tilemap.tiles[y] + x;

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    tile_id_draw(tile->id, tile_pos);

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
          glBegin(GL_QUADS);
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;
               Position_t block_camera_offset = block->pos - screen_camera;
               block_camera_offset.pixel.y += block->pos.z;
               block_draw(block, pos_to_vec(block_camera_offset));
          }
          glEnd();

          // interactive
          glBegin(GL_QUADS);
          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               switch(interactive->type){
               default:
                    break;
               case INTERACTIVE_TYPE_PRESSURE_PLATE:
               {
                    int frame_x = 7;
                    if(interactive->pressure_plate.down) frame_x++;
                    tex_vec = theme_frame(frame_x, 8);
               } break;
               case INTERACTIVE_TYPE_LEVER:
                    tex_vec = theme_frame(0, 12);
                    break;
               case INTERACTIVE_TYPE_POPUP:
                    tex_vec = theme_frame(interactive->popup.lift.ticks - 1, 8);
                    break;
               }

               Position_t interactive_camera_offset = coord_to_pos(interactive->coord) - screen_camera;
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

          // editor
          switch(editor.mode){
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

               for(S32 g = 0; g < editor.category_array.count; ++g){
                    auto* stamp_array = editor.category_array.elements + g;
                    auto* stamp = stamp_array->elements + 0;

                    if(g && (g % ROOM_TILE_SIZE) == 0){
                         vec.x = -1.0f;
                         vec.y += TILE_SIZE;
                    }

                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         tile_id_draw(stamp->tile_id, vec);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         // flat_draw(&stamp->flat, vec, app.controller != nullptr, false);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;

                         block_draw(&block, vec);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                         // if(stamp->solid.type == SOLID_DOOR){
                         //      solid_draw_door_bottom(&stamp->solid, vec);
                         // }else{
                         //      solid_draw(&stamp->solid, vec);
                         // }
                         // block_draw(&stamp->block, vec, block_frame);
                         break;
                    }

                    vec.x += TILE_SIZE;
               }

               glEnd();
          } break;
          case EDITOR_MODE_STAMP_SELECT:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               // draw stamp at mouse
               auto* mouse_stamp = editor.category_array.elements[editor.category].elements + editor.stamp;
               Coord_t mouse_coord = mouse_select_coord(mouse_screen);
               Vec_t stamp_pos = coord_to_screen_position(mouse_coord);

               switch(mouse_stamp->type){
               default:
                    break;
               case STAMP_TYPE_TILE_ID:
                    tile_id_draw(mouse_stamp->tile_id, stamp_pos);
                    break;
               case STAMP_TYPE_BLOCK:
               {
                    Block_t block = {};
                    block.element = mouse_stamp->block.element;
                    block.face = mouse_stamp->block.face;
                    block_draw(&block, stamp_pos);
               } break;
               }

               // draw stamps to select from at the bottom
               Vec_t pos = {0.0f, 0.0f};
               int max_stamp_height = 1;
               auto* stamp_array = editor.category_array.elements + editor.category;

               for(S32 g = 0; g < stamp_array->count; ++g){
                    auto* stamp = stamp_array->elements + g;
                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         tile_id_draw(stamp->tile_id, pos + coord_to_vec(stamp->offset));
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;
                         block_draw(&block, pos + coord_to_vec(stamp->offset));
                    } break;
                    }

                    pos.x += (F32)(max_stamp_height) * TILE_SIZE;

                    if(g > 0 && (g % ROOM_TILE_SIZE) == 0){
                         pos.x = 0.0f;
                         pos.y += max_stamp_height * TILE_SIZE;
                         // max_stamp_height = 1;
                    }
               }

               glEnd();
          } break;
          }

          SDL_GL_SwapWindow(window);
     }

     save_map(&tilemap, &block_array, &interactive_array, "first.bm");
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
