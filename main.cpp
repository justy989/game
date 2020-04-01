/*
SHOTCUT Open Source Video Editting Program

http://www.simonstalenhag.se/
-Grant Sanderson 3blue1brown
-Shane Hendrixson

USE deal_with_push_result() FOR RESULTS OF push_entangled_block() CALLS INSIDE OF block_collision_push()

TODO:
Entanglement Puzzles:
- entangle puzzle where there is a line of pressure plates against the wall with a line of popups on the other side that would
  trap an entangled block if it got to close, stopping the player from using walls to get blocks closer
- entangle puzzle with an extra non-entangled block where you don't want one of the entangled blocks to move, and you use
  the non-entangled block to accomplish that
- rotated entangled puzzles where the centroid is on a portal destination coord

Current bugs:
- two opposite entangled blocks moving towards each other do not do what I expect (stop halfway grid aligned, touching)
- a block travelling diagonally at a wall will stop on both axis' against the wall because of the collision
- player is able to get into a block sometimes when standing on a popup that is going down and pushing a block that has fallen off of it
- infinite recursion for entangled against pushes (may or may not be important that the pushes go through portals)
- for collision, can shorten things pos_deltas before we cause pushes to happen on ice in the same frame due to collisions ?
- A block on the tile outside a portal pushed into the portal to clone, the clone has weird behavior and ends up on the portal block
- When pushing a block through a portal that turns off, the block keeps going
- Getting a block and it's rotated entangler to push into the centroid causes any other entangled blocks to alternate pushing
- Pushing a block and shooting an arrow causes the player to go invisible
- The -test command line option used on it's own with -play cannot load the map from the demo

Big Features:
- Split blocks
  - detect split block centroids correctly
- Bring back da pits
- 3D
     - if we put a popup on the other side of a portal and a block 1 interval high goes through the portal, will it work the way we expect?
     - how does a stack of entangled blocks move? really f***ing weird right now tbh
- 2 adjacent cut blocks can be pushed, if the mass is <= 1 full block
- Visually show forces? Maybe just show blocks or walls on solid ground absorbing impacts
- When pushing blocks against blocks that are off-grid (due to pushing against a player), sometimes the blocks can't move towards other off-grid blocks
- Players impact carry velocity until the block teleports
- update get mass and block push to handle infinite mass cases
- A visual way to tell which blocks are entangled
- arrow kills player
- arrow entanglement
- Multiple players pushing blocks at once

Cleanup:
- Probably make a get_x_component(), get_y_component() for position as well ? That could help reduce code
- Template the result structures that add results up to a certain count

Potential Board Game Idea around newton's cradle
- N by N grid (maybe like chess?) that pieces can be dropped onto (creating height) or slide in from the side, attempting to impact
- Use newtonian physics to decide the impact the outcome
- maybe when both players are out of pieces, whoever has the most mass on the table wins?

Velocity Table: 1 block hits 2 blocks, hits 3 blocks etc etc.
1 block : 0.168067
2 blocks: 0.118841
3 blocks: 0.097033
4 blocks: 0.0840335

Work = Force * Distance
Kinetic Energy = 1/2 * mass * (velocity squared)
Static Friction Force to Overcome = Static Friction Coefficient * Mass * Gravity

We can just make gravity = 1 in this world probably ?
Ice Fs = Us * M

Thanks GDB:
(gdb) watch world.blocks.elements[1].accel.x == -nan
Argument to negate operation not a number.

In order to communicate that the world doesn't work like this (Real objects don't stop grid aligned).
Put in a special block that doesn't have the functionality of stopping grid aligned on purpose. Make it look like a regular rock. Make it not used for anything else.
It also bounces off of walls when colliding with them while on ice.
You can use the normal blocks to guide the these realistic blocks for some sweet metapuzzle action.
It can be a more jaggy shape to represent how the real world is much messier

Should we ice the under side of a block when it is in the air near an ice lit block?

build the entangled pushes before the loop and then when invalidating, we need to invalidate the entangled pushes that we added

*/

#include <iostream>
#include <chrono>
#include <cfloat>
#include <cassert>
#include <thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "log.h"
#include "defines.h"
#include "direction.h"
#include "bitmap.h"
#include "conversion.h"
#include "object_array.h"
#include "portal_exit.h"
#include "block.h"
#include "utils.h"
#include "map_format.h"
#include "draw.h"
#include "block_utils.h"
#include "demo.h"
#include "collision.h"
#include "world.h"
#include "editor.h"
#include "utils.h"

#define DEBUG_FILE 0

struct VecMaskCollisionEntry_t{
     S8 mask;

     // how a and b have to move in order to collide

     // first scenario
     Direction_t move_a_1;
     Direction_t move_b_1;

     // second scenario
     Direction_t move_a_2;
     Direction_t move_b_2;
};

F32 get_collision_dt(CheckBlockCollisionResult_t* collision){
    F32 vel_mag = vec_magnitude(collision->original_vel);
    F32 pos_delta_mag = vec_magnitude(collision->pos_delta);

    F32 dt = pos_delta_mag / vel_mag;
    if(dt < 0) return 0;
    if(dt > FRAME_TIME) return FRAME_TIME;

    return dt;
}

int sort_collision_by_time_comparer(const void* a, const void* b){
    CheckBlockCollisionResult_t* collision_a = (CheckBlockCollisionResult_t*)a;
    CheckBlockCollisionResult_t* collision_b = (CheckBlockCollisionResult_t*)b;

    return get_collision_dt(collision_a) < get_collision_dt(collision_b);
}

struct CheckBlockCollisions_t{
    CheckBlockCollisionResult_t* collisions = NULL;
    S32 count = 0;
    S32 allocated = 0;

    bool init(S32 block_count){
        collisions = (CheckBlockCollisionResult_t*)malloc(block_count * block_count * sizeof(*collisions));
        if(collisions == NULL) return false;
        allocated = block_count;
        return true;
    }

    bool add_collision(CheckBlockCollisionResult_t* collision){
        if(count >= allocated) return false;
        collisions[count] = *collision;
        count++;
        return true;
    }

    void reset(){
        count = 0;
    }

    void clear(){
        free(collisions);
        collisions = NULL;
        count = 0;
        allocated = 0;
    }

    void sort_by_time(){
        qsort(collisions, count, sizeof(*collisions), sort_collision_by_time_comparer);
    }
};

FILE* load_demo_number(S32 map_number, const char** demo_filepath){
     char filepath[64] = {};
     snprintf(filepath, 64, "content/%03d.bd", map_number);
     *demo_filepath = strdup(filepath);
     return fopen(*demo_filepath, "rb");
}

void cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                         ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(&world->tilemap, demo_starting_tilemap);
     deep_copy(&world->blocks, demo_starting_blocks);
     deep_copy(&world->interactives, demo_starting_interactives);
}

void fetch_cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                               ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(demo_starting_tilemap, &world->tilemap);
     deep_copy(demo_starting_blocks, &world->blocks);
     deep_copy(demo_starting_interactives, &world->interactives);
}

bool load_map_number_demo(Demo_t* demo, S16 map_number, S64* frame_count){
     if(demo->file) fclose(demo->file);
     demo->file = load_demo_number(map_number, &demo->filepath);
     if(!demo->file){
          LOG("missing map %d corresponding demo.\n", map_number);
          return false;
     }

     free(demo->entries.entries);
     demo->entry_index = 0;
     fread(&demo->version, sizeof(demo->version), 1, demo->file);
     demo->entries = demo_entries_get(demo->file);
     *frame_count = 0;
     demo->last_frame = demo->entries.entries[demo->entries.count - 1].frame;
     LOG("testing demo %s: version %d with %ld actions across %ld frames\n", demo->filepath,
         demo->version, demo->entries.count, demo->last_frame);
     return true;
}

bool load_map_number_map(S16 map_number, World_t* world, Undo_t* undo,
                         Coord_t* player_start, PlayerAction_t* player_action){
     if(load_map_number(map_number, player_start, world)){
          reset_map(*player_start, world, undo);
          *player_action = {};
          return true;
     }

     return false;
}

void update_light_and_ice_detectors(Interactive_t* interactive, World_t* world){
     switch(interactive->type){
     default:
          break;
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
     {
          Tile_t* tile = tilemap_get_tile(&world->tilemap, interactive->coord);
          Rect_t coord_rect = rect_surrounding_adjacent_coords(interactive->coord);

          S16 block_count = 0;
          Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
          quad_tree_find_in(world->block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

          Block_t* block = nullptr;
          for(S16 b = 0; b < block_count; b++){
               // blocks on the coordinate and on the ground block light
               if(block_get_coord(blocks[b]) == interactive->coord && blocks[b]->pos.z == 0){
                    block = blocks[b];
                    break;
               }
          }

          if(interactive->detector.on && (tile->light < LIGHT_DETECTOR_THRESHOLD || block)){
               activate(world, interactive->coord);
               interactive->detector.on = false;
          }else if(!interactive->detector.on && tile->light >= LIGHT_DETECTOR_THRESHOLD && !block){
               activate(world, interactive->coord);
               interactive->detector.on = true;
          }
          break;
     }
     case INTERACTIVE_TYPE_ICE_DETECTOR:
     {
          Tile_t* tile = tilemap_get_tile(&world->tilemap, interactive->coord);
          if(tile){
               if(interactive->detector.on && !tile_is_iced(tile)){
                    activate(world, interactive->coord);
                    interactive->detector.on = false;
               }else if(!interactive->detector.on && tile_is_iced(tile)){
                    activate(world, interactive->coord);
                    interactive->detector.on = true;
               }
          }
          break;
     }
     }
}

void stop_block_colliding_with_entangled(Block_t* block, Direction_t move_dir_to_stop){
     switch(move_dir_to_stop){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
     {
          block->stop_on_pixel_x = closest_pixel(block->pos.pixel.x, block->pos.decimal.x);

          Position_t final_stop_pos = pixel_pos(Pixel_t{block->stop_on_pixel_x, 0});
          Vec_t pos_delta = pos_to_vec(final_stop_pos - block->pos);

          block->pos_delta.x = pos_delta.x;
          block->vel.x = 0;
          block->accel.x = 0;

          reset_move(&block->horizontal_move);
          break;
     }
     case DIRECTION_UP:
     case DIRECTION_DOWN:
     {
          block->stop_on_pixel_y = closest_pixel(block->pos.pixel.y, block->pos.decimal.y);

          Position_t final_stop_pos = pixel_pos(Pixel_t{0, block->stop_on_pixel_y});
          Vec_t pos_delta = pos_to_vec(final_stop_pos - block->pos);

          block->pos_delta.y = pos_delta.y;
          block->vel.y = 0;
          block->accel.y = 0;

          block->stop_on_pixel_y = 0;

          reset_move(&block->vertical_move);
          break;
     }
     }
}

bool check_direction_from_block_for_adjacent_walls(Block_t* block, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt,
                                                   Direction_t direction){
     Pixel_t pixel_a {};
     Pixel_t pixel_b {};
     auto block_pos = block->teleport ? block->teleport_pos : block->pos;
     auto block_pos_delta = block->teleport ? block->teleport_pos_delta : block->pos_delta;
     block_adjacent_pixels_to_check(block_pos, block_pos_delta, block->cut, direction, &pixel_a, &pixel_b);
     Coord_t coord_a = pixel_to_coord(pixel_a);
     Coord_t coord_b = pixel_to_coord(pixel_b);

     if(tilemap_is_solid(tilemap, coord_a)){
          if(!tilemap_is_solid(tilemap, coord_a - direction)){
              return true;
          }
     }else if(tilemap_is_solid(tilemap, coord_b)){
          if(!tilemap_is_solid(tilemap, coord_b - direction)){
              return true;
          }
     }

     Interactive_t* a = quad_tree_interactive_solid_at(interactive_qt, tilemap, coord_a, block->pos.z);
     Interactive_t* b = quad_tree_interactive_solid_at(interactive_qt, tilemap, coord_b, block->pos.z);

     if(a){
         if(is_active_portal(a) && a->portal.has_block_inside && a->portal.wants_to_turn_off){
             // pass
         }else{
             return true;
         }
     }
     if(b){
         if(is_active_portal(a) && a->portal.has_block_inside && a->portal.wants_to_turn_off){
             // pass
         }else{
             return true;
         }
     }

     return false;
}

void copy_block_collision_results(Block_t* block, CheckBlockCollisionResult_t* result){
     block->pos_delta = result->pos_delta;
     block->vel = result->vel;
     block->accel = result->accel;

     block->stop_on_pixel_x = result->stop_on_pixel_x;
     block->stop_on_pixel_y = result->stop_on_pixel_y;

     block->started_on_pixel_x = 0;
     block->started_on_pixel_y = 0;

     block->horizontal_move = result->horizontal_move;
     block->vertical_move = result->vertical_move;

     block->stopped_by_player_horizontal = result->stopped_by_player_horizontal;
     block->stopped_by_player_vertical = result->stopped_by_player_vertical;
}

void build_move_actions_from_player(PlayerAction_t* player_action, Player_t* player, bool* move_actions, S8 move_action_count){
     assert(move_action_count == DIRECTION_COUNT);
     memset(move_actions, 0, sizeof(*move_actions) * move_action_count);

     for(int d = 0; d < move_action_count; d++){
          if(player_action->move[d]){
               Direction_t rot_dir = direction_rotate_clockwise((Direction_t)(d), player->move_rotation[d]);
               rot_dir = direction_rotate_clockwise(rot_dir, player->rotation);
               move_actions[rot_dir] = true;
               if(player->reface) player->face = static_cast<Direction_t>(rot_dir);
          }
     }
}

#define MAX_PLAYER_IN_BLOCK_RECT_RESULTS 16

struct PlayerInBlockRectResult_t{
     struct Entry_t{
          Block_t* block = nullptr;
          Position_t block_pos;
          S8 portal_rotations = 0;
     };

     Entry_t entries[MAX_PLAYER_IN_BLOCK_RECT_RESULTS];
     S8 entry_count = 0;

     void add_entry(Entry_t entry){
          if(entry_count < MAX_PLAYER_IN_BLOCK_RECT_RESULTS){
               entries[entry_count] = entry;
               entry_count++;
          }
     }
};

PlayerInBlockRectResult_t player_in_block_rect(Player_t* player, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     PlayerInBlockRectResult_t result;

     auto player_pos = player->teleport ? player->teleport_pos + player->teleport_pos_delta : player->pos + player->pos_delta;

     auto player_coord = pos_to_coord(player_pos);
     Rect_t search_rect = rect_surrounding_adjacent_coords(player_coord);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

     quad_tree_find_in(block_qt, search_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     for(S16 b = 0; b < block_count; b++){
         auto block_pos = blocks[b]->teleport ? blocks[b]->teleport_pos + blocks[b]->teleport_pos_delta : blocks[b]->pos + blocks[b]->pos_delta;
         auto block_rect = block_get_inclusive_rect(block_pos.pixel, blocks[b]->cut);
         if(pixel_in_rect(player->pos.pixel, block_rect)){
              PlayerInBlockRectResult_t::Entry_t entry;
              entry.block = blocks[b];
              entry.block_pos = block_pos;
              entry.portal_rotations = 0;
              result.add_entry(entry);
         }
     }

     auto found_blocks = find_blocks_through_portals(player_coord, tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.blocks + i;

         auto block_rect = block_get_inclusive_rect(found_block->position.pixel, found_block->rotated_cut);
         if(pixel_in_rect(player->pos.pixel, block_rect)){
              PlayerInBlockRectResult_t::Entry_t entry;
              entry.block = found_block->block;
              entry.block_pos = found_block->position;
              entry.portal_rotations = found_block->portal_rotations;
              result.add_entry(entry);
         }
     }

     return result;
}

void raise_entangled_blocks(World_t* world, Block_t* block);

void raise_above_blocks(World_t* world, Block_t* block){
     auto result = block_held_down_by_another_block(block, world->block_qt, world->interactive_qt, &world->tilemap);
     for(S16 i = 0; i < result.count; i++){
          Block_t* above_block = result.blocks_held[i].block;
          raise_above_blocks(world, above_block);
          above_block->pos.z++;
          above_block->held_up = BLOCK_HELD_BY_SOLID;
          raise_entangled_blocks(world, above_block);
     }
}

void raise_entangled_blocks(World_t* world, Block_t* block){
     if(block->entangle_index >= 0){
          S16 block_index = get_block_index(world, block);
          S16 entangle_index = block->entangle_index;
          while(entangle_index != block_index && entangle_index >= 0){
               auto entangled_block = world->blocks.elements + entangle_index;
               raise_above_blocks(world, entangled_block);
               entangled_block->pos.z++;
               entangled_block->held_up = BLOCK_HELD_BY_ENTANGLE;
               entangle_index = entangled_block->entangle_index;
          }
     }
}

void raise_players(ObjectArray_t<Player_t>* players){
     for(S16 i = 0; i < players->count; i++){
          players->elements[i].pos.z++;
          players->elements[i].held_up = true;

          // gettin raised makes it hard to push stuff
          players->elements[i].push_time = 0.0f;
          players->elements[i].entangle_push_time = 0.0f;
     }
}

void hold_players(ObjectArray_t<Player_t>* players){
     for(S16 i = 0; i < players->count; i++){
          players->elements[i].held_up = true;
     }
}

CheckBlockCollisionResult_t check_block_collision(World_t* world, Block_t* block){
     if(block->teleport){
          if(block->teleport_pos_delta.x != 0.0f || block->teleport_pos_delta.y != 0.0f){
               return check_block_collision_with_other_blocks(block->teleport_pos,
                                                              block->teleport_pos_delta,
                                                              block->teleport_vel,
                                                              block->teleport_accel,
                                                              block->teleport_cut,
                                                              block->stop_on_pixel_x,
                                                              block->stop_on_pixel_y,
                                                              block->teleport_horizontal_move,
                                                              block->teleport_vertical_move,
                                                              block->stopped_by_player_horizontal,
                                                              block->stopped_by_player_vertical,
                                                              get_block_index(world, block),
                                                              block->clone_start.x > 0,
                                                              world);
          }
     }else{
          if(block->pos_delta.x != 0.0f || block->pos_delta.y != 0.0f){
               return check_block_collision_with_other_blocks(block->pos,
                                                              block->pos_delta,
                                                              block->vel,
                                                              block->accel,
                                                              block->cut,
                                                              block->stop_on_pixel_x,
                                                              block->stop_on_pixel_y,
                                                              block->horizontal_move,
                                                              block->vertical_move,
                                                              block->stopped_by_player_horizontal,
                                                              block->stopped_by_player_vertical,
                                                              get_block_index(world, block),
                                                              block->clone_start.x > 0,
                                                              world);
          }
     }

     return CheckBlockCollisionResult_t{};
}

enum StopOnBoundary_t{
    DO_NOT_STOP_ON_BOUNDARY,
    STOP_ON_BOUNDARY_TRACKING_START,
    STOP_ON_BOUNDARY_IGNORING_START,
};

void update_stop_on_boundry_while_player_coasting(Direction_t move_dir, DirectionMask_t vel_mask, StopOnBoundary_t* stop_on_boundary_x, StopOnBoundary_t* stop_on_boundary_y){
     switch(move_dir){
     default:
          break;
     case DIRECTION_LEFT:
          if(vel_mask & DIRECTION_MASK_RIGHT){
               *stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }else if(vel_mask & DIRECTION_MASK_UP || vel_mask & DIRECTION_MASK_DOWN){
               *stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }
          break;
     case DIRECTION_RIGHT:
          if(vel_mask & DIRECTION_MASK_LEFT){
               *stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }else if(vel_mask & DIRECTION_MASK_UP || vel_mask & DIRECTION_MASK_DOWN){
               *stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }
          break;
     case DIRECTION_UP:
          if(vel_mask & DIRECTION_MASK_DOWN){
               *stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }else if(vel_mask & DIRECTION_MASK_LEFT || vel_mask & DIRECTION_MASK_RIGHT){
               *stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }
          break;
     case DIRECTION_DOWN:
          if(vel_mask & DIRECTION_MASK_UP){
               *stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }else if(vel_mask & DIRECTION_MASK_LEFT || vel_mask & DIRECTION_MASK_RIGHT){
               *stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }
          break;
     }
}


void apply_block_collision(World_t* world, Block_t* block, F32 dt, CheckBlockCollisionResult_t* collision_result){
     if(block->teleport){
           if(collision_result->collided){
                block->teleport_pos_delta = collision_result->pos_delta;
                block->teleport_vel = collision_result->vel;
                block->teleport_accel = collision_result->accel;

                block->teleport_stop_on_pixel_x = collision_result->stop_on_pixel_x;
                block->teleport_stop_on_pixel_y = collision_result->stop_on_pixel_y;

                block->teleport_horizontal_move = collision_result->horizontal_move;
                block->teleport_vertical_move = collision_result->vertical_move;
           }
     }else{
          if(collision_result->collided){
               S16 block_index = get_block_index(world, block);

               if(collision_result->collided_block_index >= 0 && blocks_are_entangled(collision_result->collided_block_index, block_index, &world->blocks)){
                    // TODO: I don't love indexing the blocks without checking the index is valid first
                    auto* entangled_block = world->blocks.elements + collision_result->collided_block_index;

                    // the entangled block pos might be faked due to portals, so use the resulting collision pos instead of the actual position
                    auto entangled_block_pos = collision_result->collided_pos;

                    // the result collided position is the center of the block, so handle this
                    entangled_block_pos.pixel -= block_center_pixel_offset(entangled_block->cut);

                    auto final_block_pos = block->pos + block->pos_delta;

                    Quad_t final_block_quad = {0, 0,
                                               (F32)block_get_width_in_pixels(block) * PIXEL_SIZE,
                                               (F32)block_get_height_in_pixels(block) * PIXEL_SIZE};

                    Vec_t final_block_center = {final_block_quad.right * 0.5f, final_block_quad.top * 0.5f};

                    auto entangled_block_offset_vec = pos_to_vec(entangled_block_pos - final_block_pos);

                    Quad_t entangled_block_quad = {entangled_block_offset_vec.x,
                                                   entangled_block_offset_vec.y,
                                                   entangled_block_offset_vec.x + (F32)block_get_width_in_pixels(entangled_block) * PIXEL_SIZE,
                                                   entangled_block_offset_vec.y + (F32)block_get_height_in_pixels(entangled_block) * PIXEL_SIZE};

                    Vec_t entangled_block_center = {entangled_block_quad.left + (entangled_block_quad.right - entangled_block_quad.left) * 0.5f,
                                                    entangled_block_quad.bottom + (entangled_block_quad.top - entangled_block_quad.bottom) * 0.5f};

                    auto closest_final_vec = closest_vec_in_quad(final_block_center, entangled_block_quad);
                    auto closest_entangled_vec = closest_vec_in_quad(entangled_block_center, final_block_quad);

                    auto pos_diff = closest_entangled_vec - closest_final_vec;

                    // TODO: this 0.0001f is a hack, it used to be an equality check, but the
                    //       numbers were slightly off in the case of rotated portals but not rotated entangled blocks
                    auto pos_dimension_delta = fabs(fabs(pos_diff.x) - fabs(pos_diff.y));

                    S8 final_entangle_rotation = entangled_block->rotation - collision_result->collided_portal_rotations;
                    S8 total_rotations_between = direction_rotations_between((Direction_t)(block->rotation), (Direction_t)(final_entangle_rotation));

                    bool closest_entangled_is_a_corner = (closest_final_vec == Vec_t{entangled_block_quad.left, entangled_block_quad.bottom} ||
                                                          closest_final_vec == Vec_t{entangled_block_quad.left, entangled_block_quad.top} ||
                                                          closest_final_vec == Vec_t{entangled_block_quad.right, entangled_block_quad.bottom} ||
                                                          closest_final_vec == Vec_t{entangled_block_quad.right, entangled_block_quad.top});

                    bool closest_final_is_a_corner = (closest_entangled_vec == Vec_t{final_block_quad.left, final_block_quad.bottom} ||
                                                      closest_entangled_vec == Vec_t{final_block_quad.left, final_block_quad.top} ||
                                                      closest_entangled_vec == Vec_t{final_block_quad.right,final_block_quad.bottom} ||
                                                      closest_entangled_vec == Vec_t{final_block_quad.right, final_block_quad.top});

                    // if positions are diagonal to each other and the rotation between them is odd, check if we are moving into each other
                    if(closest_final_is_a_corner && closest_entangled_is_a_corner && pos_dimension_delta <= FLT_EPSILON && (total_rotations_between) % 2 == 1){
                         auto entangle_inside_result = block_inside_others(entangled_block->pos, entangled_block->pos_delta, entangled_block->cut, get_block_index(world, entangled_block),
                                                                           entangled_block->clone_id > 0, world->block_qt, world->interactive_qt, &world->tilemap, &world->blocks);
                         if(entangle_inside_result.count > 0 && entangle_inside_result.entries[0].block == block){
                              // stop the blocks moving toward each other
                              static const VecMaskCollisionEntry_t table[] = {
                                   {static_cast<S8>(DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_LEFT  | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_LEFT  | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
                                   // TODO: single direction mask things
                              };

                              // TODO: figure out if we are colliding through a portal or not

                              auto delta_vec = pos_to_vec(block->pos - entangled_block_pos);
                              auto delta_mask = vec_direction_mask(delta_vec);
                              auto move_mask = vec_direction_mask(block->pos_delta);
                              auto entangle_move_mask = vec_direction_mask(vec_rotate_quadrants_counter_clockwise(entangled_block->pos_delta, collision_result->collided_portal_rotations));

                              Direction_t move_dir_to_stop = DIRECTION_COUNT;
                              Direction_t entangled_move_dir_to_stop = DIRECTION_COUNT;

                              for(S8 t = 0; t < (S8)(sizeof(table) / sizeof(table[0])); t++){
                                   if(table[t].mask == delta_mask){
                                        if(direction_in_mask(move_mask, table[t].move_a_1) &&
                                           direction_in_mask(entangle_move_mask, table[t].move_b_1)){
                                             move_dir_to_stop = table[t].move_a_1;
                                             entangled_move_dir_to_stop = table[t].move_b_1;
                                             break;
                                        }else if(direction_in_mask(move_mask, table[t].move_b_1) &&
                                                 direction_in_mask(entangle_move_mask, table[t].move_a_1)){
                                             move_dir_to_stop = table[t].move_b_1;
                                             entangled_move_dir_to_stop = table[t].move_a_1;
                                             break;
                                        }else if(direction_in_mask(move_mask, table[t].move_a_2) &&
                                                 direction_in_mask(entangle_move_mask, table[t].move_b_2)){
                                             move_dir_to_stop = table[t].move_a_2;
                                             entangled_move_dir_to_stop = table[t].move_b_2;
                                             break;
                                        }else if(direction_in_mask(move_mask, table[t].move_b_2) &&
                                                 direction_in_mask(entangle_move_mask, table[t].move_a_2)){
                                             move_dir_to_stop = table[t].move_b_2;
                                             entangled_move_dir_to_stop = table[t].move_a_2;
                                             break;
                                        }
                                   }
                              }

                              if(move_dir_to_stop == DIRECTION_COUNT){
                                   copy_block_collision_results(block, collision_result);
                              }else{
                                   bool block_on_ice_or_air = block_on_ice(block->pos, block->pos_delta, block->cut,
                                                                           &world->tilemap, world->interactive_qt, world->block_qt) ||
                                                              block_on_air(block->pos, block->pos_delta, block->cut,
                                                                           &world->tilemap, world->interactive_qt, world->block_qt);

                                   bool entangled_block_on_ice_or_air = block_on_ice(entangled_block->pos, entangled_block->pos_delta, entangled_block->cut,
                                                                                     &world->tilemap, world->interactive_qt, world->block_qt) ||
                                                                        block_on_air(entangled_block->pos, entangled_block->pos_delta, entangled_block->cut,
                                                                                     &world->tilemap, world->interactive_qt, world->block_qt);

                                   if(block_on_ice_or_air && entangled_block_on_ice_or_air){
                                        // TODO: handle this case for blocks not entangled on ice
                                        // TODO: handle pushing blocks diagonally

                                        TransferMomentum_t block_momentum = get_block_momentum(world, block, move_dir_to_stop);
                                        TransferMomentum_t entangled_block_momentum = get_block_momentum(world, entangled_block, entangled_move_dir_to_stop);

                                        if(block_push(block, entangled_move_dir_to_stop, world, true, 1.0f, &entangled_block_momentum).pushed){
                                             auto elastic_result = elastic_transfer_momentum_to_block(&entangled_block_momentum, world, block, entangled_move_dir_to_stop);

                                             switch(entangled_move_dir_to_stop){
                                             default:
                                                  break;
                                             case DIRECTION_LEFT:
                                             case DIRECTION_RIGHT:
                                                  block->pos_delta.x = elastic_result.second_final_velocity * dt;
                                                  break;
                                             case DIRECTION_UP:
                                             case DIRECTION_DOWN:
                                                  block->pos_delta.y = elastic_result.second_final_velocity * dt;
                                                  break;
                                             }
                                        }

                                        if(block_push(entangled_block, move_dir_to_stop, world, true, 1.0f, &block_momentum).pushed){
                                             auto elastic_result = elastic_transfer_momentum_to_block(&block_momentum, world, entangled_block, move_dir_to_stop);

                                             switch(move_dir_to_stop){
                                             default:
                                                  break;
                                             case DIRECTION_LEFT:
                                             case DIRECTION_RIGHT:
                                                  entangled_block->pos_delta.x = elastic_result.second_final_velocity * dt;
                                                  break;
                                             case DIRECTION_UP:
                                             case DIRECTION_DOWN:
                                                  entangled_block->pos_delta.y = elastic_result.second_final_velocity * dt;
                                                  break;
                                             }
                                        }
                                   }else{
                                        auto stop_entangled_dir = direction_rotate_clockwise(entangled_move_dir_to_stop, collision_result->collided_portal_rotations);

                                        stop_block_colliding_with_entangled(block, move_dir_to_stop);
                                        stop_block_colliding_with_entangled(entangled_block, stop_entangled_dir);

                                        // TODO: compress this code, it's definitely used elsewhere
                                        for(S16 p = 0; p < world->players.count; p++){
                                             auto* player = world->players.elements + p;
                                             if(player->prev_pushing_block == block_index || player->prev_pushing_block == block->entangle_index){
                                                  player->push_time = 0.0f;
                                             }
                                        }
                                   }
                              }
                         }
                    }else{
                         copy_block_collision_results(block, collision_result);
                    }
               }else{
                    copy_block_collision_results(block, collision_result);
               }
          }
     }
}

struct DoBlockCollisionResults_t{
     bool repeat_collision_pass = false;
     S16 update_blocks_count;
};

DoBlockCollisionResults_t do_block_collision(World_t* world, Block_t* block, S16 update_blocks_count){
     DoBlockCollisionResults_t result;
     result.update_blocks_count = update_blocks_count;

     S16 block_index = get_block_index(world, block);

     StopOnBoundary_t stop_on_boundary_x = DO_NOT_STOP_ON_BOUNDARY;
     StopOnBoundary_t stop_on_boundary_y = DO_NOT_STOP_ON_BOUNDARY;

     // if we are being stopped by the player and have moved more than the player radius (which is
     // a check to ensure we don't stop a block instantaneously) then stop on the coordinate boundaries
     if(block->stopped_by_player_horizontal && block->vel.x != 0){
          stop_on_boundary_x = STOP_ON_BOUNDARY_TRACKING_START;
     }

     if(block->stopped_by_player_vertical && block->vel.y != 0){
          stop_on_boundary_y = STOP_ON_BOUNDARY_TRACKING_START;
     }

     // check for adjacent walls
     if(block->pos_delta.x > 0.0f){
          if(check_direction_from_block_for_adjacent_walls(block, &world->tilemap, world->interactive_qt, DIRECTION_RIGHT)){
               stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }
     }else if(block->pos_delta.x < 0.0f){
          if(check_direction_from_block_for_adjacent_walls(block, &world->tilemap, world->interactive_qt, DIRECTION_LEFT)){
               stop_on_boundary_x = STOP_ON_BOUNDARY_IGNORING_START;
          }
     }

     if(block->pos_delta.y > 0.0f){
          if(check_direction_from_block_for_adjacent_walls(block, &world->tilemap, world->interactive_qt, DIRECTION_UP)){
               stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }
     }else if(block->pos_delta.y < 0.0f){
          if(check_direction_from_block_for_adjacent_walls(block, &world->tilemap, world->interactive_qt, DIRECTION_DOWN)){
               stop_on_boundary_y = STOP_ON_BOUNDARY_IGNORING_START;
          }
     }

     // TODO: we need to maintain a list of these blocks pushed i guess in the future
     Block_t* block_pushed = nullptr;
     for(S16 p = 0; p < world->players.count; p++){
          Player_t* player = world->players.elements + p;
          if(player->prev_pushing_block >= 0){
               block_pushed = world->blocks.elements + player->prev_pushing_block;
          }
     }

     // this instance of last_block_pushed is to keep the pushing smooth and not have it stop at the tile boundaries
     if(block != block_pushed &&
        !block_on_ice(block->pos, block->pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt) &&
        !block_on_air(block, &world->tilemap, world->interactive_qt, world->block_qt)){
          if(block_pushed && blocks_are_entangled(block_pushed, block, &world->blocks)){
               Block_t* entangled_block = block_pushed;

               S8 rotations_between = blocks_rotations_between(block, entangled_block);

               auto coast_horizontal = entangled_block->coast_horizontal;
               auto coast_vertical = entangled_block->coast_vertical;

               // swap horizontal and vertical for odd rotations
               if((rotations_between % 2) == 1){
                    coast_horizontal = entangled_block->coast_vertical;
                    coast_vertical = entangled_block->coast_horizontal;
               }

               if(coast_horizontal == BLOCK_COAST_PLAYER){
                    auto vel_mask = vec_direction_mask(block->vel);
                    Vec_t entangled_vel {entangled_block->vel.x, 0};

                    // swap x and y if rotations between is odd
                    if((rotations_between % 2) == 1) entangled_vel = Vec_t{0, entangled_block->vel.y};

                    Direction_t entangled_move_dir = vec_direction(entangled_vel);
                    Direction_t move_dir = direction_rotate_clockwise(entangled_move_dir, rotations_between);

                    update_stop_on_boundry_while_player_coasting(move_dir, vel_mask, &stop_on_boundary_x, &stop_on_boundary_y);
               }

               if(coast_vertical == BLOCK_COAST_PLAYER){
                    auto vel_mask = vec_direction_mask(block->vel);
                    Vec_t entangled_vel {0, entangled_block->vel.y};

                    // swap x and y if rotations between is odd
                    if((rotations_between % 2) == 1) entangled_vel = Vec_t{entangled_block->vel.x, 0};

                    Direction_t entangled_move_dir = vec_direction(entangled_vel);
                    Direction_t move_dir = direction_rotate_clockwise(entangled_move_dir, rotations_between);

                    update_stop_on_boundry_while_player_coasting(move_dir, vel_mask, &stop_on_boundary_x, &stop_on_boundary_y);
               }
          }
     }

     auto final_pos = block->pos + block->pos_delta;

     if(stop_on_boundary_x){
          // stop on tile boundaries separately for each axis
          S16 boundary_x = range_passes_boundary(block->pos.pixel.x, final_pos.pixel.x, block_get_lowest_dimension(block),
                                                 stop_on_boundary_x == STOP_ON_BOUNDARY_TRACKING_START ? block->started_on_pixel_x : 0);
          if(boundary_x){
               result.repeat_collision_pass = true;

               block->stop_on_pixel_x = boundary_x;
               block->accel.x = 0;
               block->vel.x = 0;
               block->horizontal_move.state = MOVE_STATE_IDLING;
               block->coast_horizontal = BLOCK_COAST_NONE;

               // figure out new pos_delta which will be used for collision in the next iteration
               auto delta_pos = pixel_to_pos(Pixel_t{boundary_x, 0}) - block->pos;
               block->pos_delta.x = pos_to_vec(delta_pos).x;
          }
     }

     if(stop_on_boundary_y){
          S16 boundary_y = range_passes_boundary(block->pos.pixel.y, final_pos.pixel.y, block_get_lowest_dimension(block),
                                                 stop_on_boundary_y == STOP_ON_BOUNDARY_TRACKING_START ? block->started_on_pixel_y : 0);
          if(boundary_y){
               result.repeat_collision_pass = true;

               block->stop_on_pixel_y = boundary_y;
               block->accel.y = 0;
               block->vel.y = 0;
               block->vertical_move.state = MOVE_STATE_IDLING;
               block->coast_vertical = BLOCK_COAST_NONE;

               // figure out new pos_delta which will be used for collision in the next iteration
               auto delta_pos = pixel_to_pos(Pixel_t{0, boundary_y}) - block->pos;
               block->pos_delta.y = pos_to_vec(delta_pos).y;
          }
     }

     auto* portal = block_is_teleporting(block, world->interactive_qt);

     // is the block teleporting and it hasn't been cloning ?
     if(portal && block->clone_start.x == 0){
          // at the first instant of the block teleporting, check if we should create an entangled_block

          PortalExit_t portal_exits = find_portal_exits(portal->coord, &world->tilemap, world->interactive_qt);
          S8 clone_id = 0;
          for (auto &direction : portal_exits.directions) {
               for(int p = 0; p < direction.count; p++){
                    if(direction.coords[p] == portal->coord) continue;

                    if(clone_id == 0){
                         block->clone_id = clone_id;
                    }else{
                         S16 new_block_index = world->blocks.count;
                         S16 old_block_index = (S16)(block - world->blocks.elements);

                         if(resize(&world->blocks, world->blocks.count + (S16)(1))){
                              // a resize will kill our block ptr, so we gotta update it
                              block = world->blocks.elements + old_block_index;
                              block->clone_start = portal->coord;

                              Block_t* entangled_block = world->blocks.elements + new_block_index;

                              *entangled_block = *block;
                              entangled_block->clone_id = clone_id;

                              // the magic
                              if(block->entangle_index == -1){
                                   entangled_block->entangle_index = old_block_index;
                              }else{
                                   entangled_block->entangle_index = block->entangle_index;
                              }

                              block->entangle_index = new_block_index;

                              // update quad tree now that we have resized the world
                              quad_tree_free(world->block_qt);
                              world->block_qt = quad_tree_build(&world->blocks);
                         }
                    }

                    clone_id++;
               }
          }

     // if we didn't find a portal but we have been cloning, we are done cloning! We either need to kill the clone or complete it
     }else if(!portal && block->clone_start.x > 0 &&
              block->entangle_index < world->blocks.count){
          // TODO: do we need to update the block quad tree here?
          auto block_move_dir = vec_direction(block->pos_delta);
          auto block_from_coord = block_get_coord(block);
          if(block_move_dir != DIRECTION_COUNT) block_from_coord -= block_move_dir;

          if(block_from_coord == block->clone_start){
               // in this instance, despawn the clone
               // NOTE: I think this relies on new entangle blocks being
               remove(&world->blocks, block->entangle_index);

               // TODO: This could lead to a subtle bug where our block index is no longer correct
               // TODO: because this was the last index in the list which got swapped to our index
               // update ptr since we could have resized
               block = world->blocks.elements + block_index;

               result.update_blocks_count--;

               // if our entangled block was not the last element, then it was replaced by another
               if(block->entangle_index < world->blocks.count){
                    Block_t* replaced_block = world->blocks.elements + block->entangle_index;
                    if(replaced_block->entangle_index >= 0){
                         // update the block's entangler that moved into our entangled blocks spot
                         Block_t* replaced_block_entangler = world->blocks.elements + replaced_block->entangle_index;
                         replaced_block_entangler->entangle_index = (S16)(replaced_block - world->blocks.elements);
                    }
               }

               block->entangle_index = -1;
          }else{
               assert(block->entangle_index >= 0);

               // great news everyone, the clone was a success

               // find the block(s) we were cloning
               S16 entangle_index = block->entangle_index;
               while(entangle_index != block_index && entangle_index != -1){
                    Block_t* entangled_block = world->blocks.elements + entangle_index;
                    if(entangled_block->clone_start.x != 0){
                         entangled_block->clone_id = 0;
                         entangled_block->clone_start = Coord_t{};
                    }

                    entangle_index = entangled_block->entangle_index;
               }

               // we reset clone_start down a little further
               block->clone_id = 0;

               // turn off the circuit
               activate(world, block->clone_start);
               auto* src_portal = quad_tree_find_at(world->interactive_qt, block->clone_start.x, block->clone_start.y);
               if(is_active_portal(src_portal)){
                    src_portal->portal.on = false;
               }
          }

          block->clone_start = Coord_t{};
     }

     return result;
}

void draw_input_on_hud(char c, Vec_t pos, bool down){
     char text[2];
     text[1] = 0;
     text[0] = c;

     glColor3f(0.0f, 0.0f, 0.0f);
     draw_text(text, pos + Vec_t{0.002f, -0.002f});

     if(down){
          glColor3f(1.0f, 1.0f, 1.0f);
          draw_text(text, pos);
     }
}

Pixel_t get_corner_pixel_from_pos(Position_t pos, DirectionMask_t from_center){
     Pixel_t result = pos.pixel;
     if(from_center & DIRECTION_MASK_UP){
          if(pos.decimal.y > FLT_EPSILON) result.y++;
     }
     if(from_center & DIRECTION_MASK_RIGHT){
          if(pos.decimal.x > FLT_EPSILON) result.x++;
     }
     return result;
}

void consolidate_block_pushes(BlockPushes_t<128>* block_pushes, BlockPushes_t<128>* consolidated_block_pushes){
     for(S16 i = 0; i < block_pushes->count; i++){
          auto* push = block_pushes->pushes + i;

          assert(push->pusher_count == 1);

          // consolidate pushes if we can
          bool consolidated_current_push = false;
          for(S16 j = 0; j < consolidated_block_pushes->count; j++){
               auto* consolidated_push = consolidated_block_pushes->pushes + j;

               if(push->pushee_index == consolidated_push->pushee_index &&
                  push->direction_mask == consolidated_push->direction_mask &&
                  push->portal_rotations == consolidated_push->portal_rotations){
                    consolidated_push->add_pusher(push->pushers[0].index, push->pushers[0].collided_with_block_count, push->is_entangled(),
                                                  push->entangle_rotations, push->portal_rotations);
                    consolidated_current_push = true;
               }
          }

          // otherwise just add it
          if(!consolidated_current_push) consolidated_block_pushes->add(push);
     }
}

void log_block_pusher(BlockPusher_t* pusher){
    LOG("   pusher %d, collided_with %d, hit_entangler: %d\n", pusher->index, pusher->collided_with_block_count, pusher->hit_entangler);
}

void log_block_push(BlockPush_t* block_push)
{
    const S16 direction_mask_string_size = 64;
    char direction_mask_string[direction_mask_string_size];
    char rot_direction_mask_string[direction_mask_string_size];

    S8 rotations = (block_push->entangle_rotations + block_push->portal_rotations) % DIRECTION_COUNT;
    auto rot_direction_mask = direction_mask_rotate_clockwise(block_push->direction_mask, rotations);

    direction_mask_to_string(block_push->direction_mask, direction_mask_string, direction_mask_string_size);
    direction_mask_to_string(rot_direction_mask, rot_direction_mask_string, direction_mask_string_size);

    LOG(" block push: invalidated %d, direction_mask %s portal_rot %d, entangle_rot %d, entangled: %d, rot direction_mask: %s\n",
        block_push->invalidated, direction_mask_string, block_push->portal_rotations, block_push->entangle_rotations,
        block_push->entangled_with_push_index, rot_direction_mask_string);

    LOG("  pushee %d, pushers (%d)\n", block_push->pushee_index, block_push->pusher_count);
    for(S8 p = 0; p < block_push->pusher_count; p++){
        log_block_pusher(block_push->pushers + p);
    }
}

void log_block_pushes(BlockPushes_t<128>& block_pushes)
{
    for(S16 i = 0; i < block_pushes.count; i++){
        log_block_push(block_pushes.pushes + i);
    }
}

bool block_pushes_are_the_same_collision(BlockPushes_t<128>& block_pushes, S16 start_index, S16 end_index, S16 block_index){
     if(start_index < 0 || start_index > block_pushes.count) return false;
     if(end_index < 0 || end_index > block_pushes.count) return false;
     if(start_index > end_index) return false;

     auto& start_push = block_pushes.pushes[start_index];
     auto& end_push = block_pushes.pushes[end_index];

     S16 start_push_count = 0;
     S16 end_push_count = 0;

     bool found_index = false;
     for(S16 i = 0; i < start_push.pusher_count; i++){
         auto& pusher = start_push.pushers[i];
         if(pusher.index != block_index) continue;
         found_index = true;
         start_push_count = pusher.collided_with_block_count;
     }
     if(!found_index) return false;

     found_index = false;
     for(S16 i = 0; i < end_push.pusher_count; i++){
         auto& pusher = end_push.pushers[i];
         if(pusher.index != block_index) continue;
         found_index = true;
         end_push_count = pusher.collided_with_block_count;
     }
     if(!found_index) return false;

     return (start_push_count > 1 && end_push_count > 1 && start_push_count == end_push_count &&
             start_push.direction_mask == end_push.direction_mask);
}

void restart_demo(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                  ObjectArray_t<Interactive_t>* demo_starting_interactives, Demo_t* demo, S64* frame_count,
                  Coord_t* player_start, PlayerAction_t* player_action, Undo_t* undo){
     fetch_cache_for_demo_seek(world, demo_starting_tilemap, demo_starting_blocks, demo_starting_interactives);

     reset_map(*player_start, world, undo);

     // reset some vars
     *player_action = {};

     demo->entry_index = 0;
     *frame_count = 0;
}

void set_against_blocks_coasting_from_player(Block_t* block, Direction_t direction, World_t* world){
     auto block_pos = block->teleport ? block->teleport_pos + block->teleport_pos_delta : block->pos + block->pos_delta;

     auto against_result = block_against_other_blocks(block_pos,
                                                      block->teleport ? block->teleport_cut : block->cut,
                                                      direction, world->block_qt, world->interactive_qt, &world->tilemap);

     for(S16 a = 0; a < against_result.count; a++){
         auto* against_other = against_result.againsts + a;
         Direction_t against_direction = direction_rotate_clockwise(direction, against_other->rotations_through_portal);

         if(direction_is_horizontal(against_direction)){
             against_other->block->coast_horizontal = BLOCK_COAST_PLAYER;
         }else{
             against_other->block->coast_vertical = BLOCK_COAST_PLAYER;
         }

         set_against_blocks_coasting_from_player(against_other->block, against_direction, world);
     }
}

int main(int argc, char** argv){
     const char* load_map_filepath = nullptr;
     bool test = false;
     bool suite = false;
     bool show_suite = false;
     bool fail_slow = false;
     S16 map_number = 0;
     S16 first_map_number = 0;
     S16 first_frame = 0;
     S16 fail_count = 0;
     int window_width = 800;
     int window_height = 800;
     int window_x = SDL_WINDOWPOS_CENTERED;
     int window_y = SDL_WINDOWPOS_CENTERED;

     Demo_t play_demo {};
     Demo_t record_demo {};

     for(int i = 1; i < argc; i++){
          if(strcmp(argv[i], "-play") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               play_demo.filepath = argv[next];
               play_demo.mode = DEMO_MODE_PLAY;
          }else if(strcmp(argv[i], "-record") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               record_demo.filepath = argv[next];
               record_demo.mode = DEMO_MODE_RECORD;
          }else if(strcmp(argv[i], "-load") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               load_map_filepath = argv[next];
          }else if(strcmp(argv[i], "-test") == 0){
               test = true;
          }else if(strcmp(argv[i], "-suite") == 0){
               test = true;
               suite = true;
          }else if(strcmp(argv[i], "-show") == 0){
               show_suite = true;
          }else if(strcmp(argv[i], "-map") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               map_number = (S16)(atoi(argv[next]));
               first_map_number = map_number;
          }else if(strcmp(argv[i], "-frame") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               first_frame = (S16)(atoi(argv[next]));
          }else if(strcmp(argv[i], "-speed") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               play_demo.dt_scalar = (F32)(atof(argv[next]));
               record_demo.dt_scalar = (F32)(atof(argv[next]));
          }else if(strcmp(argv[i], "-failslow") == 0){
               fail_slow = true;
          }else if(strcmp(argv[i], "-winw") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               window_width = atoi(argv[next]);
          }else if(strcmp(argv[i], "-winh") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               window_height = atoi(argv[next]);
          }else if(strcmp(argv[i], "-winx") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               window_x = atoi(argv[next]);
          }else if(strcmp(argv[i], "-winy") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               window_y = atoi(argv[next]);
          }else if(strcmp(argv[i], "-h") == 0){
               printf("%s [options]\n", argv[0]);
               printf("  -play   <demo filepath> replay a recorded demo file\n");
               printf("  -record <demo filepath> record a demo file\n");
               printf("  -load   <map filepath>  load a map\n");
               printf("  -test                   validate the map state is correct after playing a demo\n");
               printf("  -suite                  run map/demo combos in succession validating map state after each headless\n");
               printf("  -show                   use in combination with -suite to run with a head\n");
               printf("  -map    <integer>       load a map by number\n");
               printf("  -speed  <decimal>       when replaying a demo, specify how fast/slow to replay where 1.0 is realtime\n");
               printf("  -frame  <integer>       which frame to play to automatically before drawing\n");
               printf("  -failslow               opposite of failfast, where we continue running tests in the suite after failure\n");
               printf("  -winx                   set the x position of the window. default: SDL_WINDOWPOS_CENTERED\n");
               printf("  -winy                   set the y position of the window. default: SDL_WINDOWPOS_CENTERED\n");
               printf("  -winw                   set the width of the window. default: 800\n");
               printf("  -winh                   set the height of the window. default: 800\n");
               printf("  -h this help.\n");
               return 0;
          }
     }

     const char* log_path = "bryte.log";
     if(!Log_t::create(log_path)){
          fprintf(stderr, "failed to create log file: '%s'\n", log_path);
          return -1;
     }

     if(test && !load_map_filepath && !suite){
          LOG("cannot test without specifying a map to load\n");
          return 1;
     }

     SDL_Window* window = nullptr;
     SDL_GLContext opengl_context = nullptr;
     GLuint theme_texture = 0;
     GLuint player_texture = 0;
     GLuint arrow_texture = 0;
     GLuint text_texture = 0;

     if(!suite || show_suite){
          if(SDL_Init(SDL_INIT_EVERYTHING) != 0){
               return 1;
          }

          SDL_DisplayMode display_mode;
          if(SDL_GetCurrentDisplayMode(0, &display_mode) != 0){
               return 1;
          }

          LOG("Create window: %d, %d\n", window_width, window_height);
          window = SDL_CreateWindow("bryte", window_x, window_y, window_width, window_height, SDL_WINDOW_OPENGL);
          if(!window) return 1;

          opengl_context = SDL_GL_CreateContext(window);

          // SDL_GL_SetSwapInterval(SDL_TRUE);
          SDL_GL_SetSwapInterval(SDL_FALSE);
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

          theme_texture = transparent_texture_from_file("content/theme.bmp");
          if(theme_texture == 0) return 1;
          player_texture = transparent_texture_from_file("content/player.bmp");
          if(player_texture == 0) return 1;
          arrow_texture = transparent_texture_from_file("content/arrow.bmp");
          if(arrow_texture == 0) return 1;
          text_texture = transparent_texture_from_file("content/text.bmp");
          if(text_texture == 0) return 1;
     }

     if(play_demo.mode != DEMO_MODE_NONE){
          if(!demo_begin(&play_demo)){
               return 1;
          }
     }

     if(record_demo.mode != DEMO_MODE_NONE){
          if(!demo_begin(&record_demo)){
               return 1;
          }
     }

     World_t world {};

     Editor_t editor {};
     Undo_t undo {};

     Coord_t player_start {2, 8};

     S64 frame_count = 0;

     bool quit = false;
     bool seeked_with_mouse = false;
     bool resetting = false;
     F32 reset_timer = 1.0f;

     PlayerAction_t player_action {};
     Position_t camera = coord_to_pos(Coord_t{8, 8});

     Vec_t mouse_screen = {}; // 0.0f to 1.0f
     Position_t mouse_world = {};
     bool ctrl_down = false;

     S8 collision_attempts = 0;

     // cached to seek in demo faster
     TileMap_t demo_starting_tilemap {};
     ObjectArray_t<Block_t> demo_starting_blocks {};
     ObjectArray_t<Interactive_t> demo_starting_interactives {};

     Quad_t pct_bar_outline_quad = {0, 2.0f * PIXEL_SIZE, 1.0f, 0.02f};

     if(load_map_filepath){
          if(!load_map(load_map_filepath, &player_start, &world.tilemap, &world.blocks, &world.interactives)){
               return 1;
          }

          if(play_demo.mode == DEMO_MODE_PLAY){
               cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
          }
     }else if(suite){
          if(!load_map_number(map_number, &player_start, &world)){
               return 1;
          }

          cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

          play_demo.mode = DEMO_MODE_PLAY;
          if(!load_map_number_demo(&play_demo, map_number, &frame_count)){
               return 1;
          }
     }else if(map_number){
          if(!load_map_number(map_number, &player_start, &world)){
               return 1;
          }

          if(play_demo.mode == DEMO_MODE_PLAY){
               cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
          }

          if(first_frame > 0 && first_frame < play_demo.last_frame){
               play_demo.seek_frame = first_frame;
               play_demo.paused = true;
          }
     }else{
          setup_default_room(&world);
     }

     reset_map(player_start, &world, &undo);
     init(&editor);

#if DEBUG_FILE
     FILE* debug_file = fopen("debug.txt", "w");
#endif

     F32 dt = 0.0f;

     auto last_time = std::chrono::system_clock::now();
     auto current_time = last_time;

     while(!quit){
          if((!suite || show_suite) && play_demo.seek_frame < 0){
               current_time = std::chrono::system_clock::now();
               std::chrono::duration<double> elapsed_seconds = current_time - last_time;
               auto elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time);
               dt = (F32)(elapsed_seconds.count());

               if(dt < FRAME_TIME / play_demo.dt_scalar){
                    if(elapsed_milliseconds.count() < 16){
                         std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    continue; // limit 60 fps
               }
          }

          last_time = current_time;

          // TODO: consider 30fps as minimum for random noobs computers
          dt = FRAME_TIME; // the game always runs as if a 60th of a frame has occurred.

          quad_tree_free(world.block_qt);
          world.block_qt = quad_tree_build(&world.blocks);

          if(!play_demo.paused || play_demo.seek_frame >= 0){
               frame_count++;
               if(play_demo.seek_frame == frame_count) play_demo.seek_frame = -1;
          }

          player_action.last_activate = player_action.activate;
          for(S16 i = 0; i < world.players.count; i++) {
               world.players.elements[i].reface = false;
          }

          if(play_demo.mode == DEMO_MODE_PLAY){
               if(demo_play_frame(&play_demo, &player_action, &world.players, frame_count, &record_demo)){
                    if(test){
                         bool passed = test_map_end_state(&world, &play_demo);
                         if(!passed){
                              LOG("test failed\n");
                              fail_count++;
                         }
                         if(!passed && !fail_slow){
                              play_demo.mode = DEMO_MODE_NONE;
                              if(suite && !show_suite) return 1;
                         }else if(suite){
                              map_number++;
                              S16 maps_tested = map_number - first_map_number;

                              if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&play_demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }else{
                                   if(fail_slow){
                                        LOG("Done Testing %d maps where %d failed.\n", maps_tested, fail_count);
                                   }else{
                                        LOG("Done Testing %d maps.\n", maps_tested);
                                   }
                                   return 0;
                              }
                         }
                    }else{
                         play_demo.paused = true;
                    }
               }
          }

          SDL_Event sdl_event;
          while(SDL_PollEvent(&sdl_event)){
               switch(sdl_event.type){
               default:
                    break;
               case SDL_QUIT:
                    quit = true;
                    break;
               case SDL_KEYDOWN:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_A:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_LEFT);
                         }else if(play_demo.mode == DEMO_MODE_PLAY){
                              if(frame_count > 0 && play_demo.seek_frame < 0){
                                   play_demo.seek_frame = frame_count - 1;

                                   restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                                &play_demo, &frame_count, &player_start, &player_action, &undo);
                              }
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_LEFT_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_D:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_RIGHT);
                         }else if(play_demo.mode == DEMO_MODE_PLAY){
                              if(play_demo.seek_frame < 0){
                                   play_demo.seek_frame = frame_count + 1;
                              }
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_RIGHT_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_W:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_UP);
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_UP_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_S:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_DOWN);
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_DOWN_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_E:
                         if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_ACTIVATE_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_SPACE:
                         if(play_demo.mode == DEMO_MODE_PLAY){
                              play_demo.paused = !play_demo.paused;
                         }else{
                              if(!resetting){
                                   player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_SHOOT_START,
                                                         record_demo.mode, record_demo.file, frame_count);
                              }
                         }
                         break;
                    case SDL_SCANCODE_L:
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(record_demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
                              }
                         }
                         break;
                    case SDL_SCANCODE_LEFTBRACKET:
                         map_number--;
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(record_demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&play_demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }
                         }else{
                              map_number++;
                         }
                         break;
                    case SDL_SCANCODE_RIGHTBRACKET:
                         map_number++;
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(play_demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&play_demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }
                         }else{
                              map_number--;
                         }
                         break;
                    case SDL_SCANCODE_MINUS:
                         if(play_demo.dt_scalar > 0.1f){
                              play_demo.dt_scalar -= 0.1f;
                              LOG("game dt scalar: %.1f\n", play_demo.dt_scalar);
                         }
                         break;
                    case SDL_SCANCODE_EQUALS:
                         play_demo.dt_scalar += 0.1f;
                         LOG("game dt scalar: %.1f\n", play_demo.dt_scalar);
                         break;
                    case SDL_SCANCODE_V:
                         if(editor.mode != EDITOR_MODE_OFF){
                              char filepath[64];
                              snprintf(filepath, 64, "content/%03d.bm", map_number);
                              save_map(filepath, player_start, &world.tilemap, &world.blocks, &world.interactives);
                         }
                         break;
                    case SDL_SCANCODE_U:
                         if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_UNDO,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_N:
                    {
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, mouse_select_world(mouse_screen, camera));
                         if(tile) tile_toggle_wire_activated(tile);
                    } break;
                    case SDL_SCANCODE_8:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world(mouse_screen, camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                              for(S16 i = 0; i < block_count; i++){
                                   S16 block_index = get_block_index(&world, blocks[i]);
                                   if(editor.entangle_indices.count > 1 && block_index == editor.entangle_indices.elements[0]){
                                        LOG("applying entanglement\n");
                                        for(S16 e = 0; e < editor.entangle_indices.count; e++){
                                             S16 next_index = (e + 1) % editor.entangle_indices.count;
                                             S16 entangle_index = editor.entangle_indices.elements[next_index];
                                             Block_t* block = world.blocks.elements + editor.entangle_indices.elements[e];
                                             LOG("  %d -> %d\n", editor.entangle_indices.elements[e], entangle_index);
                                             block->entangle_index = entangle_index;
                                        }
                                        destroy(&editor.entangle_indices);
                                        break;
                                   }else{
                                       S16 last_index = editor.entangle_indices.count;
                                       resize(&editor.entangle_indices, editor.entangle_indices.count + 1);
                                       editor.entangle_indices.elements[last_index] = block_index;
                                       LOG("editor track entangle index %d\n", block_index);
                                   }
                              }
                         }
                         break;
                    case SDL_SCANCODE_2:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto pixel = mouse_select_world_pixel(mouse_screen, camera);

                              // TODO: make this a function where you can pass in entangle_index
                              S16 new_index = world.players.count;
                              if(resize(&world.players, world.players.count + (S16)(1))){
                                   Player_t* new_player = world.players.elements + new_index;
                                   *new_player = world.players.elements[0];
                                   new_player->pos = pixel_to_pos(pixel);
                              }
                         }
                         break;
                    case SDL_SCANCODE_0:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world(mouse_screen, camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                              for(S16 i = 0; i < block_count; i++){
                                   Block_t* block = blocks[i];
                                   if(block->entangle_index >= 0){
                                        S16 block_index = get_block_index(&world, block);
                                        S16 current_index = block_index;
                                        do
                                        {
                                             Block_t* current_block = world.blocks.elements + current_index;
                                             current_index = current_block->entangle_index;
                                             current_block->entangle_index = -1;
                                        }while(current_index != block_index && current_index >= 0);
                                   }
                              }

                              destroy(&editor.entangle_indices);
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
                              destroy(&editor.entangle_indices);
                         }
                         break;
                    case SDL_SCANCODE_TAB:
                         if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                              editor.mode = EDITOR_MODE_STAMP_HIDE;
                         }else{
                              editor.mode = EDITOR_MODE_STAMP_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_RETURN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                              // clear coords below stamp
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                                   }
                              }

                              for(int i = 0; i < editor.selection.count; i++){
                                   Coord_t coord = editor.selection_start + editor.selection.elements[i].offset;
                                   apply_stamp(editor.selection.elements + i, coord,
                                               &world.tilemap, &world.blocks, &world.interactives, &world.interactive_qt, ctrl_down);
                              }

                              quad_tree_free(world.block_qt);
                              world.block_qt = quad_tree_build(&world.blocks);

                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_T:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              sort_selection(&editor);

                              S16 height_offset = (S16)((editor.selection_end.y - editor.selection_start.y) - 1);

                              // perform rotation on each offset
                              for(S16 i = 0; i < editor.selection.count; i++){
                                   auto* stamp = editor.selection.elements + i;
                                   Coord_t rot {stamp->offset.y, (S16)(-stamp->offset.x + height_offset)};
                                   stamp->offset = rot;
                              }
                         }
                         break;
                    case SDL_SCANCODE_X:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              destroy(&editor.clipboard);
                              shallow_copy(&editor.selection, &editor.clipboard);
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_P:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT && editor.clipboard.count){
                              destroy(&editor.selection);
                              shallow_copy(&editor.clipboard, &editor.selection);
                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         }
                         break;
                    case SDL_SCANCODE_M:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              player_start = mouse_select_world(mouse_screen, camera);
                         }else if(editor.mode == EDITOR_MODE_OFF){
                              resetting = true;
                         }
                         break;
                    case SDL_SCANCODE_R:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world(mouse_screen, camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                              for(S16 i = 0; i < block_count; i++){
                                   blocks[i]->rotation += 1;
                                   blocks[i]->rotation %= DIRECTION_COUNT;
                              }
                         }else if(editor.mode == EDITOR_MODE_OFF){
                              resetting = true;
                         }
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = true;
                         break;
                    case SDL_SCANCODE_5:
                    {
                         reset_players(&world.players);
                         Player_t* player = world.players.elements;
                         player->pos.pixel = mouse_select_world_pixel(mouse_screen, camera) + HALF_TILE_SIZE_PIXEL;
                         player->pos.decimal.x = 0;
                         player->pos.decimal.y = 0;
                         break;
                    }
                    case SDL_SCANCODE_H:
                    {
                         Pixel_t pixel = mouse_select_world_pixel(mouse_screen, camera) + HALF_TILE_SIZE_PIXEL;
                         Coord_t coord = mouse_select_world(mouse_screen, camera);
                         LOG("mouse pixel: %d, %d, Coord: %d, %d\n", pixel.x, pixel.y, coord.x, coord.y);
                         describe_coord(coord, &world);
                    } break;
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
                    case SDL_SCANCODE_A:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_LEFT_STOP,
                                               record_demo.mode, record_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_D:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP,
                                               record_demo.mode, record_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_W:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_UP_STOP,
                                               record_demo.mode, record_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_S:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_DOWN_STOP,
                                               play_demo.mode, play_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_ACTIVATE_STOP,
                                               record_demo.mode, record_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_SPACE:
                         if(play_demo.mode == DEMO_MODE_PLAY) break;
                         if(resetting) break;

                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_SHOOT_STOP,
                                               record_demo.mode, record_demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = false;
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONDOWN:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_OFF:
                              if(play_demo.mode == DEMO_MODE_PLAY){
                                   if(vec_in_quad(&pct_bar_outline_quad, mouse_screen)){
                                        seeked_with_mouse = true;

                                        play_demo.seek_frame = (S64)((F32)(play_demo.last_frame) * mouse_screen.x);

                                        if(play_demo.seek_frame < frame_count){
                                            restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                                         &play_demo, &frame_count, &player_start, &player_action, &undo);
                                        }else if(play_demo.seek_frame == frame_count){
                                             play_demo.seek_frame = -1;
                                        }
                                   }
                              }
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              Coord_t mouse_coord = vec_to_coord(mouse_screen);
                              S32 select_index = (mouse_coord.y * ROOM_TILE_SIZE) + mouse_coord.x;
                              if(select_index < EDITOR_CATEGORY_COUNT){
                                   editor.mode = EDITOR_MODE_STAMP_SELECT;
                                   editor.category = select_index;
                                   editor.stamp = 0;
                              }else{
                                   editor.mode = EDITOR_MODE_CREATE_SELECTION;
                                   editor.selection_start = mouse_select_world(mouse_screen, camera);
                                   editor.selection_end = editor.selection_start;
                              }
                         } break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              S32 select_index = mouse_select_stamp_index(vec_to_coord(mouse_screen),
                                                                          editor.category_array.elements + editor.category);
                              if(editor.mode != EDITOR_MODE_STAMP_HIDE && select_index < editor.category_array.elements[editor.category].count && select_index >= 0){
                                   editor.stamp = select_index;
                              }else{
                                   undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                                   Coord_t select_coord = mouse_select_world(mouse_screen, camera);
                                   auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
                                   for(S16 s = 0; s < stamp_array->count; s++){
                                        auto* stamp = stamp_array->elements + s;
                                        apply_stamp(stamp, select_coord + stamp->offset,
                                                    &world.tilemap, &world.blocks, &world.interactives, &world.interactive_qt, ctrl_down);
                                   }

                                   quad_tree_free(world.block_qt);
                                   world.block_qt = quad_tree_build(&world.blocks);
                              }
                         } break;
                         }
                         break;
                    case SDL_BUTTON_RIGHT:
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              coord_clear(mouse_select_world(mouse_screen, camera), &world.tilemap, &world.interactives,
                                          world.interactive_qt, &world.blocks);
                              break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Coord_t start = mouse_select_world(mouse_screen, camera);
                              Coord_t end = start + stamp_array_dimensions(editor.category_array.elements[editor.category].elements + editor.stamp);
                              for(S16 j = start.y; j < end.y; j++){
                                   for(S16 i = start.x; i < end.x; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                                   }
                              }
                         } break;
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                                   }
                              }
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONUP:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         seeked_with_mouse = false;

                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CREATE_SELECTION:
                         {
                              editor.selection_end = mouse_select_world(mouse_screen, camera);

                              sort_selection(&editor);

                              destroy(&editor.selection);

                              S16 stamp_count = (S16)((((editor.selection_end.x - editor.selection_start.x) + 1) *
                                                       ((editor.selection_end.y - editor.selection_start.y) + 1)) * 2);
                              init(&editor.selection, stamp_count);
                              S16 stamp_index = 0;
                              for(S16 j = editor.selection_start.y; j <= editor.selection_end.y; j++){
                                   for(S16 i = editor.selection_start.x; i <= editor.selection_end.x; i++){
                                        Coord_t coord = {i, j};
                                        Coord_t offset = coord - editor.selection_start;

                                        // tile id
                                        Tile_t* tile = tilemap_get_tile(&world.tilemap, coord);
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_ID;
                                        editor.selection.elements[stamp_index].tile_id = tile->id;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // tile flags
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_FLAGS;
                                        editor.selection.elements[stamp_index].tile_flags = tile->flags;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // interactive
                                        auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, coord);
                                        if(interactive){
                                             resize(&editor.selection, editor.selection.count + (S16)(1));
                                             auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                             stamp->type = STAMP_TYPE_INTERACTIVE;
                                             stamp->interactive = *interactive;
                                             stamp->offset = offset;
                                        }

                                        for(S16 b = 0; b < world.blocks.count; b++){
                                             auto* block = world.blocks.elements + b;
                                             if(pos_to_coord(block->pos) == coord){
                                                  resize(&editor.selection, editor.selection.count + (S16)(1));
                                                  auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                                  stamp->type = STAMP_TYPE_BLOCK;
                                                  stamp->block.rotation = block->rotation;
                                                  stamp->block.element = block->element;
                                                  stamp->offset = offset;
                                                  stamp->block.z = block->pos.z;
                                             }
                                        }
                                   }
                              }

                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEMOTION:
                    mouse_screen = Vec_t{((F32)(sdl_event.button.x) / (F32)(window_width)),
                                         1.0f - ((F32)(sdl_event.button.y) / (F32)(window_height))};
                    mouse_world = vec_to_pos(mouse_screen);
                    switch(editor.mode){
                    default:
                         break;
                    case EDITOR_MODE_CREATE_SELECTION:
                         if(editor.selection_start.x >= 0 && editor.selection_start.y >= 0){
                              editor.selection_end = pos_to_coord(mouse_world);
                         }
                         break;
                    }

                    if(seeked_with_mouse && play_demo.mode == DEMO_MODE_PLAY){
                         play_demo.seek_frame = (S64)((F32)(play_demo.last_frame) * mouse_screen.x);

                         if(play_demo.seek_frame < frame_count){
                              restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                           &play_demo, &frame_count, &player_start, &player_action, &undo);
                         }else if(play_demo.seek_frame == frame_count){
                              play_demo.seek_frame = -1;
                         }
                    }
                    break;
               }
          }

          if(!play_demo.paused || play_demo.seek_frame >= 0){
               collision_attempts = 1;

               reset_tilemap_light(&world);

               // update time related interactives
               for(S16 i = 0; i < world.interactives.count; i++){
                    Interactive_t* interactive = world.interactives.elements + i;
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         lift_update(&interactive->popup.lift, POPUP_TICK_DELAY, dt, 1, POPUP_MAX_LIFT_TICKS);
                    }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                         lift_update(&interactive->door.lift, POPUP_TICK_DELAY, dt, 0, DOOR_MAX_HEIGHT);
                    }
               }

               // update arrows
               for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
                    Arrow_t* arrow = world.arrows.arrows + i;
                    if(!arrow->alive) continue;

                    Coord_t pre_move_coord = pixel_to_coord(arrow->pos.pixel);

                    if(arrow->element == ELEMENT_FIRE){
                         U8 light_height = (arrow->pos.z / HEIGHT_INTERVAL) * LIGHT_DECAY;
                         illuminate(pre_move_coord, 255 - light_height, &world);
                    }

                    if(arrow->stuck_time > 0.0f){
                         arrow->stuck_time += dt;

                         switch(arrow->stuck_type){
                         default:
                              break;
                         case STUCK_BLOCK:
                              arrow->pos = arrow->stuck_block->pos + arrow->stuck_offset;
                              break;
                         }

                         if(arrow->stuck_time > ARROW_DISINTEGRATE_DELAY){
                              arrow->alive = false;
                         }
                         continue;
                    }

                    F32 arrow_friction = 0.9999f;

                    if(arrow->pos.z > 0){
                         // TODO: fall based on the timer !
                         arrow->fall_time += dt;
                         if(arrow->fall_time > ARROW_FALL_DELAY){
                              arrow->fall_time -= ARROW_FALL_DELAY;
                              arrow->pos.z--;
                         }
                    }else{
                         arrow_friction = 0.9f;
                    }

                    Vec_t direction = {};
                    switch(arrow->face){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         direction.x = -1;
                         break;
                    case DIRECTION_RIGHT:
                         direction.x = 1;
                         break;
                    case DIRECTION_DOWN:
                         direction.y = -1;
                         break;
                    case DIRECTION_UP:
                         direction.y = 1;
                         break;
                    }

                    arrow->pos += (direction * dt * arrow->vel);
                    arrow->vel *= arrow_friction;
                    Coord_t post_move_coord = pixel_to_coord(arrow->pos.pixel);

                    Rect_t coord_rect = rect_surrounding_coord(post_move_coord);

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                    for(S16 b = 0; b < block_count; b++){
                         // blocks on the coordinate and on the ground block light
                         Rect_t block_rect = block_get_inclusive_rect(blocks[b]);
                         S16 block_index = (S16)(blocks[b] - world.blocks.elements);
                         S8 block_bottom = blocks[b]->pos.z;
                         S8 block_top = block_bottom + HEIGHT_INTERVAL;
                         if(pixel_in_rect(arrow->pos.pixel, block_rect) && arrow->element_from_block != block_index){
                              if(arrow->pos.z >= block_bottom && arrow->pos.z <= block_top){
                                   arrow->stuck_time = dt;
                                   arrow->stuck_offset = arrow->pos - blocks[b]->pos;
                                   arrow->stuck_type = STUCK_BLOCK;
                                   arrow->stuck_block = blocks[b];
                              }else if(arrow->pos.z > block_top && arrow->pos.z < (block_top + HEIGHT_INTERVAL)){
                                   arrow->element_from_block = block_index;
                                   if(arrow->element != blocks[b]->element){
                                        Element_t arrow_element = arrow->element;
                                        arrow->element = transition_element(arrow->element, blocks[b]->element);
                                        if(arrow_element){
                                             blocks[b]->element = transition_element(blocks[b]->element, arrow_element);
                                             if(blocks[b]->entangle_index >= 0 && blocks[b]->entangle_index < world.blocks.count){
                                                  S16 original_index = blocks[b] - world.blocks.elements;
                                                  S16 entangle_index = blocks[b]->entangle_index;
                                                  while(entangle_index != original_index && entangle_index >= 0){
                                                       Block_t* entangled_block = world.blocks.elements + entangle_index;
                                                       entangled_block->element = transition_element(entangled_block->element, arrow_element);
                                                       entangle_index = entangled_block->entangle_index;
                                                  }
                                             }
                                        }
                                   }
                              // the block is only iced so we just want to melt the ice, if the block isn't covered
                              }else if(arrow->pos.z >= block_bottom && arrow->pos.z <= (block_top + MELT_SPREAD_HEIGHT) &&
                                       !block_held_down_by_another_block(blocks[b], world.block_qt, world.interactive_qt, &world.tilemap).held()){
                                   if(arrow->element == ELEMENT_FIRE && blocks[b]->element == ELEMENT_ONLY_ICED){
                                        blocks[b]->element = ELEMENT_NONE;
                                   }else if(arrow->element == ELEMENT_ICE && blocks[b]->element == ELEMENT_NONE){
                                        blocks[b]->element = ELEMENT_ONLY_ICED;
                                   }
                              }
                         }
                    }

                    if(block_count == 0){
                         arrow->element_from_block = -1;
                    }

                    if(pre_move_coord != post_move_coord){
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, post_move_coord);
                         if(tile && tile_is_solid(tile)){
                              arrow->stuck_time = dt;
                         }

                         // catch or give elements
                         if(arrow->element == ELEMENT_FIRE){
                              melt_ice(post_move_coord, arrow->pos.z, 0, &world);
                         }else if(arrow->element == ELEMENT_ICE){
                              spread_ice(post_move_coord, arrow->pos.z, 0, &world);
                         }

                         Interactive_t* interactive = quad_tree_interactive_find_at(world.interactive_qt, post_move_coord);
                         if(interactive){
                              switch(interactive->type){
                              default:
                                   break;
                              case INTERACTIVE_TYPE_LEVER:
                                   if(arrow->pos.z >= HEIGHT_INTERVAL){
                                        activate(&world, post_move_coord);
                                   }else{
                                        arrow->stuck_time = dt;
                                   }
                                   break;
                              case INTERACTIVE_TYPE_DOOR:
                                   if(interactive->door.lift.ticks < arrow->pos.z){
                                        arrow->stuck_time = dt;
                                        // TODO: stuck in door
                                   }
                                   break;
                              case INTERACTIVE_TYPE_POPUP:
                                   if(interactive->popup.lift.ticks > arrow->pos.z){
                                        arrow->stuck_time = dt;
                                        // TODO: stuck in popup
                                   }
                                   break;
                              case INTERACTIVE_TYPE_PORTAL:
                                   if(!interactive->portal.on){
                                        arrow->stuck_time = dt;
                                        // TODO: arrow drops if portal turns on
                                   }else if(!portal_has_destination(post_move_coord, &world.tilemap, world.interactive_qt)){
                                        // TODO: arrow drops if portal turns on
                                        arrow->stuck_time = dt;
                                   }
                                   break;
                              }
                         }

                         auto teleport_result = teleport_position_across_portal(arrow->pos, Vec_t{}, &world,
                                                                                pre_move_coord, post_move_coord);
                         if(teleport_result.count > 0){
                              arrow->pos = teleport_result.results[0].pos;
                              arrow->face = direction_rotate_clockwise(arrow->face, teleport_result.results[0].rotations);

                              // TODO: entangle
                         }
                    }
               }

               // TODO: deal with all this for multiple players
               bool move_actions[DIRECTION_COUNT];
               bool user_stopping_x = false;
               bool user_stopping_y = false;

               // update player
               for(S16 i = 0; i < world.players.count; i++){
                    Player_t* player = world.players.elements + i;

                    build_move_actions_from_player(&player_action, player, move_actions, DIRECTION_COUNT);

                    player->accel = vec_zero();

                    if(move_actions[DIRECTION_RIGHT]){
                         if(move_actions[DIRECTION_LEFT]){
                              user_stopping_x = true;

                              if(player->vel.x > 0){
                                   player->accel.x -= PLAYER_ACCEL;
                              }else if(player->vel.x < 0){
                                   player->accel.x += PLAYER_ACCEL;
                              }
                         }else{
                              player->accel.x += PLAYER_ACCEL;
                         }
                    }else if(move_actions[DIRECTION_LEFT]){
                         player->accel.x -= PLAYER_ACCEL;
                    }else if(player->vel.x > 0){
                         user_stopping_x = true;
                         player->accel.x -= PLAYER_ACCEL;
                    }else if(player->vel.x < 0){
                         user_stopping_x = true;
                         player->accel.x += PLAYER_ACCEL;
                    }

                    if(move_actions[DIRECTION_UP]){
                         if(move_actions[DIRECTION_DOWN]){
                              user_stopping_y = true;

                              if(player->vel.y > 0){
                                   player->accel.y -= PLAYER_ACCEL;
                              }else if(player->vel.y < 0){
                                   player->accel.y += PLAYER_ACCEL;
                              }
                         }else{
                              player->accel.y += PLAYER_ACCEL;
                         }
                    }else if(move_actions[DIRECTION_DOWN]){
                         player->accel.y -= PLAYER_ACCEL;
                    }else if(player->vel.y > 0){
                         user_stopping_y = true;
                         player->accel.y -= PLAYER_ACCEL;
                    }else if(player->vel.y < 0){
                         user_stopping_y = true;
                         player->accel.y += PLAYER_ACCEL;
                    }

                    if(player_action.activate && !player_action.last_activate){
                         undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         activate(&world, pos_to_coord(player->pos) + player->face);
                    }

                    if(player_action.undo){
                         undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives, true);
                         undo_revert(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         quad_tree_free(world.interactive_qt);
                         world.interactive_qt = quad_tree_build(&world.interactives);
                         quad_tree_free(world.block_qt);
                         world.block_qt = quad_tree_build(&world.blocks);
                         player_action.undo = false;
                    }

                    if(player->has_bow && player_action.shoot && player->bow_draw_time < PLAYER_BOW_DRAW_DELAY){
                         player->bow_draw_time += dt;
                    }else if(!player_action.shoot){
                         if(player->bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Position_t arrow_pos = player->pos;
                              switch(player->face){
                              default:
                                   break;
                              case DIRECTION_LEFT:
                                   arrow_pos.pixel.y -= 2;
                                   arrow_pos.pixel.x -= 8;
                                   break;
                              case DIRECTION_RIGHT:
                                   arrow_pos.pixel.y -= 2;
                                   arrow_pos.pixel.x += 8;
                                   break;
                              case DIRECTION_UP:
                                   arrow_pos.pixel.y += 7;
                                   break;
                              case DIRECTION_DOWN:
                                   arrow_pos.pixel.y -= 11;
                                   break;
                              }
                              arrow_pos.z += ARROW_SHOOT_HEIGHT;
                              arrow_spawn(&world.arrows, arrow_pos, player->face);
                         }
                         player->bow_draw_time = 0.0f;
                    }

                    if(!player_action.move[DIRECTION_LEFT] && !player_action.move[DIRECTION_RIGHT] &&
                       !player_action.move[DIRECTION_UP] && !player_action.move[DIRECTION_DOWN]){
                         player->walk_frame = 1;
                    }else{
                         player->walk_frame_time += dt;

                         if(player->walk_frame_time > PLAYER_WALK_DELAY){
                              if(vec_magnitude(player->vel) > PLAYER_IDLE_SPEED){
                                   player->walk_frame_time = 0.0f;

                                   player->walk_frame += player->walk_frame_delta;
                                   if(player->walk_frame > 2 || player->walk_frame < 0){
                                        player->walk_frame = 1;
                                        player->walk_frame_delta = -player->walk_frame_delta;
                                   }
                              }else{
                                   player->walk_frame = 1;
                                   player->walk_frame_time = 0.0f;
                              }
                         }
                    }
               }

               Position_t room_center = coord_to_pos(Coord_t{8, 8});
               Position_t camera_movement = room_center - camera;
               camera += camera_movement * 0.05f;

               for(S16 i = 0; i < world.players.count; i++){
                    Player_t* player = world.players.elements + i;

                    auto pushing_block_dir = direction_rotate_clockwise(player->pushing_block_dir, player->pushing_block_rotation);
                    if(player->pushing_block >= 0 && player->face == pushing_block_dir){
                         player->prev_pushing_block = player->pushing_block;
                    }else{
                         player->prev_pushing_block = -1;
                    }
                    player->pushing_block = -1;
                    carried_pos_delta_reset(&player->carried_pos_delta);
                    player->held_up = false;

                    player->teleport = false;
                    player->teleport_pos = player->pos;
                    player->teleport_pos_delta = player->pos_delta;
                    player->teleport_pushing_block = -1;

                    player->prev_vel = player->vel;

                    player->pos_delta.x = calc_position_motion(player->vel.x, player->accel.x, dt);
                    player->vel.x = calc_velocity_motion(player->vel.x, player->accel.x, dt);

                    player->pos_delta.y = calc_position_motion(player->vel.y, player->accel.y, dt);
                    player->vel.y = calc_velocity_motion(player->vel.y, player->accel.y, dt);

                    // handle free form stopping
                    if(user_stopping_x){
                         if((player->prev_vel.x > 0 && player->vel.x < 0) ||
                            (player->prev_vel.x < 0 && player->vel.x > 0)){
                              float dt_consumed = -player->prev_vel.x / player->accel.x;

                              player->pos_delta.x = player->prev_vel.x * dt_consumed + 0.5f * player->accel.x * dt_consumed * dt_consumed;

                              player->accel.x = 0;
                              player->vel.x = 0;
                              player->prev_vel.x = 0;
                         }
                    }

                    if(user_stopping_y){
                         if((player->prev_vel.y > 0 && player->vel.y < 0) ||
                            (player->prev_vel.y < 0 && player->vel.y > 0)){
                              float dt_consumed = -player->prev_vel.y / player->accel.y;

                              player->pos_delta.y = player->prev_vel.y * dt_consumed + 0.5f * player->accel.y * dt_consumed * dt_consumed;

                              player->accel.y = 0;
                              player->vel.y = 0;
                              player->prev_vel.y = 0;
                         }
                    }

                    if(fabs(player->vel.x) > PLAYER_MAX_VEL){
                         // vf = v0 + a * dt
                         // (vf - v0) / a = dt
                         float max_vel_mag = PLAYER_MAX_VEL;
                         if(player->vel.x < 0) max_vel_mag = -max_vel_mag;
                         float dt_consumed = (max_vel_mag - player->prev_vel.x) / player->accel.x;
                         float dt_leftover = dt - dt_consumed;

                         player->pos_delta.x = player->prev_vel.x * dt + (0.5f * player->accel.x * dt * dt);
                         player->vel.x = (player->vel.x > 0) ? PLAYER_MAX_VEL : -PLAYER_MAX_VEL;

                         player->pos_delta.x += player->vel.x * dt_leftover;
                    }

                    if(fabs(player->vel.y) > PLAYER_MAX_VEL){
                         float max_vel_mag = PLAYER_MAX_VEL;
                         if(player->vel.y < 0) max_vel_mag = -max_vel_mag;
                         float dt_consumed = (max_vel_mag - player->prev_vel.y) / player->accel.y;
                         float dt_leftover = dt - dt_consumed;

                         player->pos_delta.y = player->prev_vel.y * dt + (0.5f * player->accel.y * dt * dt);
                         player->vel.y = (player->vel.y > 0) ? PLAYER_MAX_VEL : -PLAYER_MAX_VEL;

                         player->pos_delta.y += player->vel.y * dt_leftover;
                    }

                    // limit the max velocity and normalize the vector
                    float max_pos_delta = PLAYER_MAX_VEL * dt + 0.5f * PLAYER_ACCEL * dt * dt;
                    if(vec_magnitude(player->pos_delta) > max_pos_delta){
                         player->pos_delta = vec_normalize(player->pos_delta) * max_pos_delta;
                    }

                    if(player->stopping_block_from_time > 0){
                         player->stopping_block_from_time -= dt;
                         if(player->stopping_block_from_time < 0){
                              player->stopping_block_from_time = 0;
                              player->stopping_block_from = DIRECTION_COUNT;
                         }
                    }else{
                         player->stopping_block_from = DIRECTION_COUNT;
                    }

                    player->pos_delta_save = player->pos_delta;
               }

               // check for being held up
               for(S16 i = 0; i < world.players.count; i++){
                    auto player = world.players.elements + i;

                    Coord_t player_previous_coord = pos_to_coord(player->pos);

                    // drop the player if they are above 0 and not held up by anything. This also contains logic for following a block
                    Interactive_t* interactive = quad_tree_interactive_find_at(world.interactive_qt, player_previous_coord);
                    if(interactive){
                         if(interactive->type == INTERACTIVE_TYPE_POPUP){
                              if(interactive->popup.lift.ticks == player->pos.z + 1){
                                    hold_players(&world.players);
                              }else if(interactive->coord == player_previous_coord && interactive->popup.lift.ticks == player->pos.z + 2){
                                    raise_players(&world.players);
                              }
                         }
                    }

                    if(!player->held_up){
                         auto result = player_in_block_rect(player, &world.tilemap, world.interactive_qt, world.block_qt);
                         for(S8 e = 0; e < result.entry_count; e++){
                              auto& entry = result.entries[e];
                              if(entry.block_pos.z == player->pos.z - HEIGHT_INTERVAL){
                                   hold_players(&world.players);
                              }else if((entry.block_pos.z - 1) == player->pos.z - HEIGHT_INTERVAL){
                                   raise_players(&world.players);
                              }
                         }
                    }
               }

               // fall pass
               for(S16 i = 0; i < world.players.count; i++){
                    auto player = world.players.elements + i;
                    if(!player->held_up && player->pos.z > 0){
                         player->pos.z--;
                    }
               }

               // override portals knowing whether or not they have blocks inside
               for(S16 i = 0; i < world.interactives.count; i++){
                    Interactive_t* interactive = world.interactives.elements + i;
                    if(interactive->type == INTERACTIVE_TYPE_PORTAL){
                        if(interactive->portal.on && interactive->portal.wants_to_turn_off && !interactive->portal.has_block_inside){
                            // TODO: kill player if they are in the portal
                            interactive->portal.on = false;
                            interactive->portal.wants_to_turn_off = false;
                        }
                        interactive->portal.has_block_inside = false;
                    }
               }

               // block movement

               // do a pass moving the block as far as possible, so that collision doesn't rely on order of blocks in the array
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;

                    block->prev_vel = block->vel;

                    block->prev_push_mask = block->cur_push_mask;
                    block->cur_push_mask = DIRECTION_MASK_NONE;

                    S16 mass = get_block_stack_mass(&world, block);

                    if(block->previous_mass != mass){
                         // 1/2mivi^2 = 1/2mfvf^2
                         // (1/2mivi^2) / (1/2mf) = vf^2
                         // sqrt((1/2mivi^2) / (1/2mf)) = vf

                         if(block->vel.x != 0){
                              F32 vel = block->vel.x;
                              if(vel < 0) vel = -vel;

                              F32 initial_momentum = momentum_term(block->previous_mass, vel);
                              F32 vel_squared = initial_momentum / (0.5f * mass);
                              F32 final_vel = sqrt(vel_squared);

                              if(block->vel.x < 0){
                                   block->vel.x = -final_vel;
                              }else{
                                   block->vel.x = final_vel;
                              }
                         }

                         if(block->vel.y != 0){
                              F32 vel = block->vel.y;
                              if(vel < 0) vel = -vel;

                              F32 initial_momentum = momentum_term(block->previous_mass, vel);
                              F32 vel_squared = initial_momentum / (0.5f * mass);
                              F32 final_vel = sqrt(vel_squared);

                              if(block->vel.y < 0){
                                   block->vel.y = -final_vel;
                              }else{
                                   block->vel.y = final_vel;
                              }
                         }
                    }

                    block->previous_mass = mass;

                    block->pos_delta.x = calc_position_motion(block->vel.x, block->accel.x, dt);
                    block->vel.x = calc_velocity_motion(block->vel.x, block->accel.x, dt);

                    block->pos_delta.y = calc_position_motion(block->vel.y, block->accel.y, dt);
                    block->vel.y = calc_velocity_motion(block->vel.y, block->accel.y, dt);

                    carried_pos_delta_reset(&block->carried_pos_delta);
                    block->held_up = BLOCK_HELD_BY_NONE;

                    block->coast_horizontal = BLOCK_COAST_NONE;
                    block->coast_vertical = BLOCK_COAST_NONE;

                    block->teleport = false;
                    block->teleport_cut = block->cut;
                    block->teleport_vel = block->vel;
                    block->teleport_accel = block->accel;
                    block->teleport_split = false;
               }

               // TODO: for these next 2 passes, do we need to care about teleport position? Probably just the next loop?
               for(S16 i = 0; i < world.blocks.count; i++){
                    auto block = world.blocks.elements + i;

                    auto result = block_held_up_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap);
                    if(result.held()){
                         block->held_up |= BLOCK_HELD_BY_SOLID;
                    }

                    // TODO: should we care about the decimal component of the position ?
                    Coord_t rect_coords[4];
                    bool pushed_up = false;
                    auto final_pos = block->pos + block->pos_delta;
                    Pixel_t final_pixel = final_pos.pixel;

                    // check if we pass over a grid aligned pixel, then use that one if so for both axis
                    auto check_x_pixel = passes_over_grid_pixel(block->pos.pixel.x, final_pos.pixel.x);
                    if(check_x_pixel >= 0) final_pixel.x = check_x_pixel;
                    auto check_y_pixel = passes_over_grid_pixel(block->pos.pixel.y, final_pos.pixel.y);
                    if(check_y_pixel >= 0) final_pixel.y = check_y_pixel;

                    auto block_rect = block_get_inclusive_rect(final_pixel, block->cut);
                    get_rect_coords(block_rect, rect_coords);
                    for(S8 c = 0; c < 4; c++){
                         auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, rect_coords[c]);
                         if(interactive){
                              // TODO: it is not kewl to use block_get_inclusive_rect to get a rect for the interactive, we have
                              // functions for this in utils
                              auto interactive_rect = block_get_inclusive_rect(coord_to_pixel(rect_coords[c]), block->cut);
                              if(!rect_in_rect(block_rect, interactive_rect)) continue;

                              if(interactive->type == INTERACTIVE_TYPE_POPUP){
                                   if(!pushed_up && (block->pos.z == interactive->popup.lift.ticks - 2)){
                                        raise_above_blocks(&world, block);

                                        block->pos.z++;
                                        block->held_up |= BLOCK_HELD_BY_SOLID;

                                        raise_entangled_blocks(&world, block);

                                        pushed_up = true;
                                   }else if(!block->held_up && block->pos.z == (interactive->popup.lift.ticks - 1)){
                                        block->held_up |= BLOCK_HELD_BY_SOLID;
                                   }
                              }
                         }
                    }

                    // if a block is inside a portal, set a flag for that portal and any connected portals
                    for(S16 c = 0; c < 4; c++){
                         Interactive_t* interactive = quad_tree_interactive_find_at(world.interactive_qt, rect_coords[c]);
                         if(!is_active_portal(interactive)) continue;
                         interactive->portal.has_block_inside = true;

                         PortalExit_t portal_exits = find_portal_exits(rect_coords[c], &world.tilemap, world.interactive_qt, false);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              auto portal_exit = portal_exits.directions + d;

                              for(S8 p = 0; p < portal_exit->count; p++){
                                  auto portal_coord = portal_exit->coords[p];
                                  if(portal_coord == rect_coords[c]) continue;

                                  Interactive_t* through_portal_interactive = quad_tree_interactive_find_at(world.interactive_qt, portal_coord);
                                  if(!through_portal_interactive) continue;
                                  if(through_portal_interactive->type != INTERACTIVE_TYPE_PORTAL) continue;
                                  through_portal_interactive->portal.has_block_inside = true;
                              }
                         }
                    }
               }

               for(S16 i = 0; i < world.blocks.count; i++){
                    auto block = world.blocks.elements + i;

                    if(block->pos.z <= 0){
                         if(block->pos.z <= -HEIGHT_INTERVAL) continue;

                         bool over_pit = false;

                         auto coord = block_get_coord(block);
                         auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, coord);

                         if(interactive && interactive->type == INTERACTIVE_TYPE_PIT){
                              auto coord_rect = rect_surrounding_coord(coord);
                              auto block_rect = block_get_inclusive_rect(block);
                              if(rect_completely_in_rect(block_rect, coord_rect)){
                                   block->vel.x = 0;
                                   block->vel.y = 0;
                                   block->accel.x = 0;
                                   block->accel.y = 0;
                                   block->pos.decimal.x = 0;
                                   block->pos.decimal.y = 0;
                                   block->pos_delta.x = 0;
                                   block->pos_delta.y = 0;
                                   reset_move(&block->horizontal_move);
                                   reset_move(&block->vertical_move);
                                   over_pit = true;
                              }
                         }

                         if(!over_pit) continue;
                    }

                    if(block->entangle_index >= 0){
                         S16 entangle_index = block->entangle_index;
                         while(entangle_index != i && entangle_index >= 0){
                              auto entangled_block = world.blocks.elements + entangle_index;
                              if(entangled_block->held_up & BLOCK_HELD_BY_SOLID){
                                   block->held_up |= BLOCK_HELD_BY_ENTANGLE;
                                   break;
                              }
                              entangle_index = entangled_block->entangle_index;
                         }
                    }

                    if(!block->held_up){
                         block->fall_time -= dt;
                         if(block->fall_time < 0){
                              block->fall_time = FALL_TIME;
                              block->pos.z--;
                              block->fall_time = 0;
                         }
                    }
               }

               // do multiple passes here so that entangled blocks know for sure if their entangled counterparts are coasting and index order doesn't matter
               for(S16 j = 0; j < 2; j++){ // TODO: is this enough iterations or will we need more iterations for multiple entangled blocks?
                    for(S16 i = 0; i < world.blocks.count; i++){
                         Block_t* block = world.blocks.elements + i;

                         bool would_teleport_onto_ice = false;

                         auto block_center = block_get_center(block);
                         auto premove_coord = block_get_coord(block);
                         auto coord = block_get_coord(block->pos + block->pos_delta, block->cut);
                         auto teleport_result = teleport_position_across_portal(block_center, block->pos_delta, &world, premove_coord, coord, false);
                         if(teleport_result.count > block->clone_id){
                              auto pos = teleport_result.results[block->clone_id].pos;
                              pos.pixel -= block_center_pixel_offset(block->cut);
                              auto pos_delta = teleport_result.results[block->clone_id].delta;
                              would_teleport_onto_ice = block_on_ice(pos, pos_delta, block->cut, &world.tilemap, world.interactive_qt, world.block_qt);
                         }

                         if(block_on_ice(block->pos, block->pos_delta, block->cut, &world.tilemap, world.interactive_qt, world.block_qt) || would_teleport_onto_ice){
                              block->coast_horizontal = BLOCK_COAST_ICE;
                              block->coast_vertical = BLOCK_COAST_ICE;
                         }else if(block_on_air(block, &world.tilemap, world.interactive_qt, world.block_qt)){
                              block->coast_horizontal = BLOCK_COAST_AIR;
                              block->coast_vertical = BLOCK_COAST_AIR;
                         }

                         if(block->coast_vertical <= BLOCK_COAST_ICE  || block->coast_horizontal <= BLOCK_COAST_ICE){
                              for(S16 p = 0; p < world.players.count; p++){
                                   Player_t* player = world.players.elements + p;

                                   // is the player pushing us ?
                                   if(player->prev_pushing_block < 0) continue;

                                   Block_t* player_prev_pushing_block = world.blocks.elements + player->prev_pushing_block;
                                   if(player_prev_pushing_block == block){
                                        bool set_block_coasting_from_player = false;

                                        switch(player->face){
                                        default:
                                             break;
                                        case DIRECTION_LEFT:
                                        case DIRECTION_RIGHT:
                                        {
                                             // only coast the block is actually moving
                                             Vec_t block_vel = {block->vel.x, 0};
                                             if((player->pushing_block_rotation % 2)){
                                                  block_vel = Vec_t{0, block->vel.y};

                                                  if(player->pushing_block_dir == vec_direction(block_vel)){
                                                       block->coast_vertical = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }else{
                                                  if(player->pushing_block_dir == vec_direction(block_vel)){
                                                       block->coast_horizontal = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }
                                             break;
                                        }
                                        case DIRECTION_UP:
                                        case DIRECTION_DOWN:
                                        {
                                             Vec_t block_vel = {0, block->vel.y};
                                             if((player->pushing_block_rotation % 2)){
                                                  block_vel = Vec_t{block->vel.x, 0};
                                                  if(player->pushing_block_dir == vec_direction(block_vel)){
                                                       block->coast_horizontal = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }else{
                                                  if(player->pushing_block_dir == vec_direction(block_vel)){
                                                       block->coast_vertical = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }
                                             break;
                                        }
                                        }

                                        if(set_block_coasting_from_player){
                                            set_against_blocks_coasting_from_player(block, player->face, &world);
                                        }
                                   }else if(blocks_are_entangled(block, player_prev_pushing_block, &world.blocks) &&
                                            !block_on_ice(block->pos, block->pos_delta, block->cut, &world.tilemap, world.interactive_qt, world.block_qt) &&
                                            !block_on_air(block, &world.tilemap, world.interactive_qt, world.block_qt)){
                                        Block_t* entangled_block = player_prev_pushing_block;

                                        auto rotations_between = blocks_rotations_between(block, entangled_block);
                                        MoveState_t check_idle_move_state = MOVE_STATE_IDLING;

                                        if(entangled_block->coast_horizontal > BLOCK_COAST_NONE){
                                             if(rotations_between % 2 == 0){
                                                  if(entangled_block->coast_horizontal == BLOCK_COAST_PLAYER &&
                                                     block->horizontal_move.state != MOVE_STATE_STOPPING) block->coast_horizontal = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  check_idle_move_state = block->horizontal_move.state;
                                             }else{
                                                  if(entangled_block->coast_horizontal == BLOCK_COAST_PLAYER &&
                                                     block->horizontal_move.state != MOVE_STATE_STOPPING) block->coast_vertical = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  check_idle_move_state = block->vertical_move.state;
                                             }

                                             if(check_idle_move_state == MOVE_STATE_IDLING && player->push_time > BLOCK_PUSH_TIME &&
                                                !block_held_down_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap).held()){
                                                  Vec_t block_horizontal_vel = {entangled_block->vel.x, 0};
                                                  auto block_move_dir = vec_direction(block_horizontal_vel);
                                                  if(block_move_dir != DIRECTION_COUNT){
                                                       S16 block_mass = block_get_mass(player_prev_pushing_block);
                                                       S16 entangled_block_mass = block_get_mass(block);
                                                       F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
                                                       auto direction_to_push = direction_rotate_clockwise(block_move_dir, rotations_between);
                                                       auto allowed_result = allowed_to_push(&world, block, direction_to_push, mass_ratio);
                                                       if(allowed_result.push){
                                                            block_push(block, direction_to_push, &world, false, mass_ratio * allowed_result.mass_ratio);
                                                       }
                                                  }
                                             }
                                        }

                                        if(entangled_block->coast_vertical > BLOCK_COAST_NONE){
                                             if(rotations_between % 2 == 0){
                                                  if(entangled_block->coast_vertical == BLOCK_COAST_PLAYER &&
                                                     block->vertical_move.state != MOVE_STATE_STOPPING) block->coast_vertical = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  check_idle_move_state = block->vertical_move.state;
                                             }else{
                                                  if(entangled_block->coast_vertical == BLOCK_COAST_PLAYER &&
                                                     block->vertical_move.state != MOVE_STATE_STOPPING) block->coast_horizontal = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  check_idle_move_state = block->horizontal_move.state;
                                             }

                                             if(check_idle_move_state == MOVE_STATE_IDLING && player->push_time > BLOCK_PUSH_TIME &&
                                                !block_held_down_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap).held()){
                                                  Vec_t block_vertical_vel = {0, entangled_block->vel.y};
                                                  auto block_move_dir = vec_direction(block_vertical_vel);
                                                  if(block_move_dir != DIRECTION_COUNT){
                                                       S16 block_mass = block_get_mass(player_prev_pushing_block);
                                                       S16 entangled_block_mass = block_get_mass(block);
                                                       F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
                                                       auto direction_to_push = direction_rotate_clockwise(block_move_dir, rotations_between);
                                                       auto allowed_result = allowed_to_push(&world, block, direction_to_push, mass_ratio);
                                                       if(allowed_result.push){
                                                            block_push(block, direction_to_push, &world, false, mass_ratio * allowed_result.mass_ratio);
                                                       }
                                                  }
                                             }
                                        }
                                   }
                              }
                         }
                    }
               }

               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;

                    // TODO: creating this potentially big vector could lead to precision issues
                    Vec_t pos_vec = pos_to_vec(block->pos);

                    MotionComponent_t x_component = motion_x_component(block);
                    MotionComponent_t y_component = motion_y_component(block);

                    bool coasting_horizontally = false;
                    if(block->horizontal_move.state == MOVE_STATE_STARTING ||
                       block->horizontal_move.state == MOVE_STATE_COASTING){
                         coasting_horizontally = block->coast_horizontal != BLOCK_COAST_NONE;
                    }else if(block->horizontal_move.state == MOVE_STATE_STOPPING){
                         coasting_horizontally = (block->coast_horizontal == BLOCK_COAST_ICE ||
                                                  block->coast_horizontal == BLOCK_COAST_AIR ||
                                                  block->coast_horizontal == BLOCK_COAST_ENTANGLED_PLAYER);
                         if(block->stopped_by_player_horizontal){
                              coasting_horizontally = false;
                         }
                    }

                    bool coasting_vertically = false;
                    if(block->vertical_move.state == MOVE_STATE_STARTING ||
                       block->vertical_move.state == MOVE_STATE_COASTING){
                         coasting_vertically = block->coast_vertical != BLOCK_COAST_NONE;
                    }else if(block->vertical_move.state == MOVE_STATE_STOPPING){
                         coasting_vertically = (block->coast_vertical == BLOCK_COAST_ICE ||
                                                block->coast_vertical == BLOCK_COAST_AIR ||
                                                block->coast_vertical == BLOCK_COAST_ENTANGLED_PLAYER);
                         if(block->stopped_by_player_vertical){
                              coasting_vertically = false;
                         }
                    }

                    S16 mass = block_get_mass(block);

                    update_motion_grid_aligned(block->cut, &block->horizontal_move, &x_component,
                                               coasting_horizontally, dt,
                                               pos_vec.x, mass);

                    update_motion_grid_aligned(block->cut, &block->vertical_move, &y_component,
                                               coasting_vertically, dt,
                                               pos_vec.y, mass);
               }

               BlockPushes_t<128> all_block_pushes; // TODO: is 128 this enough ?
               BlockPushes_t<128> all_consolidated_block_pushes;

               // TODO: maybe one day make this happen only when we load the map
               CheckBlockCollisions_t collision_results;
               collision_results.init(world.blocks.count);

               // unbounded collision: this should be exciting
               // we have our initial position and our initial pos_delta, update pos_delta for all players and blocks until nothing is colliding anymore
               const S8 max_collision_attempts = 16;
               bool repeat_collision_pass = true;
               while(repeat_collision_pass && collision_attempts <= max_collision_attempts){
                    repeat_collision_pass = false;

                    // do a collision pass on each block
                    S16 update_blocks_count = world.blocks.count;
                    for(S16 i = 0; i < update_blocks_count; i++){
                         auto* block = world.blocks.elements + i;
                         auto collision_result = check_block_collision(&world, block);
                         // add collisions that we need to resolve
                         if(collision_result.collided){
                             collision_results.add_collision(&collision_result);
                         }
                    }

                    // on any collision repeat
                    if(collision_results.count > 0) repeat_collision_pass = true;

                    collision_results.sort_by_time();

                    for(S32 i = 0; i < collision_results.count; i++){
                        auto* collision = collision_results.collisions + i;
                        if(!collision->collided) continue;
                        apply_block_collision(&world, world.blocks.elements + collision->block_index, dt, collision);
                        // update collision for all blocks afterwards
                        for(S32 j = i + 1; j < collision_results.count; j++){
                            auto* block = world.blocks.elements + collision_results.collisions[j].block_index;
                            collision_results.collisions[j] = check_block_collision(&world, block);
                        }
                        all_block_pushes.merge(&collision->block_pushes);
                    }

                    collision_results.reset();

                    for(S16 i = 0; i < update_blocks_count; i++){
                         auto block = world.blocks.elements + i;
                         auto block_collision_result = do_block_collision(&world, block, update_blocks_count);
                         if(block_collision_result.repeat_collision_pass){
                             repeat_collision_pass = true;
                         }
                         update_blocks_count = block_collision_result.update_blocks_count;
                    }

                    // check if blocks extinguish elements of other blocks
                    for(S16 i = 0; i < world.blocks.count; i++){
                         auto block = world.blocks.elements + i;

                         // TODO: use teleport pos and pos_delta here?

                         // filter out blocks that couldn't extinguish
                         if(block->pos_delta.x == 0 && block->pos_delta.y == 0) continue;
                         if(block->pos.z == 0) continue; // TODO: if we bring back pits, remove this line

                         auto block_rect = block_get_inclusive_rect(block);
                         auto coord = block_get_coord(block);
                         auto search_rect = rect_surrounding_adjacent_coords(coord);

                         S16 block_count = 0;
                         Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                         quad_tree_find_in(world.block_qt, search_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                         for(S16 b = 0; b < block_count; b++){
                              auto check_block = blocks[b];

                              if(check_block->element != ELEMENT_FIRE && check_block->element != ELEMENT_ICE) continue;
                              if(check_block->pos.z + HEIGHT_INTERVAL != block->pos.z) continue;

                              if(pixel_in_rect(block_center_pixel(check_block), block_rect)){
                                   if(check_block->element == ELEMENT_FIRE){
                                        check_block->element = ELEMENT_NONE;
                                   }else if(check_block->element == ELEMENT_ICE){
                                        check_block->element = ELEMENT_ONLY_ICED;
                                   }
                              }
                         }
                    }

                    // update carried blocks pos_delta based on carrying blocks pos_delta
                    for(S16 i = 0; i < world.blocks.count; i++){
                         auto block = world.blocks.elements + i;

                         auto result = block_held_up_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap,
                                                                      BLOCK_FRICTION_AREA);
                         for(S16 b = 0; b < result.count; b++){
                              auto holder = result.blocks_held[b].block;

                              // a frictionless surface cannot carry a block
                              if(holder->element == ELEMENT_ICE || holder->element == ELEMENT_ONLY_ICED) continue;

                              if(holder && holder->pos_delta != vec_zero()){
                                   auto old_carried_pos_delta = block->carried_pos_delta.positive + block->carried_pos_delta.negative;

                                   auto holder_index = get_block_index(&world, holder);
                                   if(!get_carried_noob(&block->carried_pos_delta, holder->pos_delta, holder_index, false)){
                                        auto new_carried_pos_delta = block->carried_pos_delta.positive + block->carried_pos_delta.negative;

                                        if(block->teleport){
                                             block->teleport_pos_delta -= old_carried_pos_delta;
                                             block->teleport_pos_delta += new_carried_pos_delta;
                                        }else{
                                             block->pos_delta -= old_carried_pos_delta;
                                             block->pos_delta += new_carried_pos_delta;
                                        }

                                        S16 entangle_index = block->entangle_index;
                                        while(entangle_index != i && entangle_index >= 0){
                                             Block_t* entangled_block = world.blocks.elements + entangle_index;

                                             S8 rotations_between = blocks_rotations_between(block, entangled_block);
                                             auto rotated_pos_delta = vec_rotate_quadrants_clockwise(holder->pos_delta, rotations_between);

                                             old_carried_pos_delta = entangled_block->carried_pos_delta.positive + entangled_block->carried_pos_delta.negative;
                                             if(!get_carried_noob(&entangled_block->carried_pos_delta, rotated_pos_delta, holder_index, true)){
                                                  new_carried_pos_delta = entangled_block->carried_pos_delta.positive + entangled_block->carried_pos_delta.negative;

                                                  if(entangled_block->teleport){
                                                       entangled_block->teleport_pos_delta -= old_carried_pos_delta;
                                                       entangled_block->teleport_pos_delta += new_carried_pos_delta;
                                                  }else{
                                                       entangled_block->pos_delta -= old_carried_pos_delta;
                                                       entangled_block->pos_delta += new_carried_pos_delta;
                                                  }

                                                  repeat_collision_pass = true;
                                             }

                                             entangle_index = entangled_block->entangle_index;
                                        }

                                        repeat_collision_pass = true;
                                   }
                              }
                         }
                    }

                    // see if any moving blocks teleport
                    for(S16 i = 0; i < world.blocks.count; i++){
                         auto block = world.blocks.elements + i;
                         if(block->teleport) continue;

                         auto block_center = block_get_center(block);
                         auto premove_coord = block_get_coord(block);
                         auto coord = block_get_coord(block->pos + block->pos_delta, block->cut);
                         auto teleport_result = teleport_position_across_portal(block_center, block->pos_delta, &world, premove_coord, coord, false);
                         if(teleport_result.count > block->clone_id){
                              block->teleport = true;
                              block->teleport_pos = teleport_result.results[block->clone_id].pos;
                              block->teleport_cut = block_cut_rotate_clockwise(block->cut, teleport_result.results[block->clone_id].rotations);
                              block->teleport_pos.pixel -= block_center_pixel_offset(block->teleport_cut);

                              block->teleport_pos_delta = teleport_result.results[block->clone_id].delta;
                              block->teleport_vel = vec_rotate_quadrants_clockwise(block->vel, teleport_result.results[block->clone_id].rotations);
                              block->teleport_accel = vec_rotate_quadrants_clockwise(block->accel, teleport_result.results[block->clone_id].rotations);
                              block->teleport_rotation = teleport_result.results[block->clone_id].rotations;

                              if((block->teleport_rotation % 2)){
                                   block->teleport_vertical_move   = block->horizontal_move;
                                   block->teleport_horizontal_move = block->vertical_move;

                                   // figure out if we need to flip the horizontal or vertical move signs
                                   {
                                        auto prev_horizontal_dir = vec_direction(Vec_t{block->pos_delta.x, 0});
                                        auto prev_vertical_dir = vec_direction(Vec_t{0, block->pos_delta.y});

                                        auto cur_horizontal_dir = vec_direction(Vec_t{block->teleport_pos_delta.x, 0});
                                        auto cur_vertical_dir = vec_direction(Vec_t{0, block->teleport_pos_delta.y});

                                        if(direction_is_positive(prev_horizontal_dir) != direction_is_positive(cur_vertical_dir)){
                                             move_flip_sign(&block->teleport_vertical_move);
                                        }

                                        if(direction_is_positive(prev_vertical_dir) != direction_is_positive(cur_horizontal_dir)){
                                             move_flip_sign(&block->teleport_horizontal_move);
                                        }
                                   }

                                   for(S8 r = 0; r < block->teleport_rotation; r++){
                                        block->prev_push_mask = direction_mask_rotate_clockwise(block->prev_push_mask);
                                   }
                              }else{
                                   block->teleport_horizontal_move = block->horizontal_move;
                                   block->teleport_vertical_move = block->vertical_move;

                                   // figure out if we need to flip the horizontal or vertical move signs
                                   auto prev_horizontal_dir = vec_direction(Vec_t{block->pos_delta.x, 0});
                                   auto prev_vertical_dir = vec_direction(Vec_t{0, block->pos_delta.y});

                                   auto cur_horizontal_dir = vec_direction(Vec_t{block->teleport_pos_delta.x, 0});
                                   auto cur_vertical_dir = vec_direction(Vec_t{0, block->teleport_pos_delta.y});

                                   if(direction_is_positive(prev_vertical_dir) != direction_is_positive(cur_vertical_dir)){
                                        move_flip_sign(&block->teleport_vertical_move);
                                   }

                                   if(direction_is_positive(prev_horizontal_dir) != direction_is_positive(cur_horizontal_dir)){
                                        move_flip_sign(&block->teleport_horizontal_move);
                                   }
                              }

                              Interactive_t* src_portal = quad_tree_interactive_find_at(world.interactive_qt, teleport_result.results[block->clone_id].src_portal);
                              Interactive_t* dst_portal = quad_tree_interactive_find_at(world.interactive_qt, teleport_result.results[block->clone_id].dst_portal);
                              if(src_portal && src_portal->type == INTERACTIVE_TYPE_PORTAL &&
                                 dst_portal && dst_portal->type == INTERACTIVE_TYPE_PORTAL){
                                  Direction_t src_portal_dir = src_portal->portal.face;
                                  Direction_t dst_portal_dir = dst_portal->portal.face;

                                  // to fight the battle against floating point error, track any blocks we are connected to (going the same speed)
                                  {
                                      auto against_direction = direction_opposite(src_portal_dir);
                                      auto against_result = block_against_other_blocks(block->pos + block->pos_delta,
                                                                                       block->cut, against_direction, world.block_qt,
                                                                                       world.interactive_qt, &world.tilemap);

                                      F32 block_vel = 0;
                                      F32 block_pos_delta = 0;

                                      if(direction_is_horizontal(against_direction)){
                                          block_vel = block->vel.x;
                                          block_pos_delta = block->pos_delta.x;
                                      }else{
                                          block_vel = block->vel.y;
                                          block_pos_delta = block->pos_delta.y;
                                      }

                                      // TODO: we may need to handle the against portal rotation data it gives us
                                      for(S16 a = 0; a < against_result.count; a++){
                                          auto* against_other = against_result.againsts + a;

                                          F32 against_block_vel = 0;
                                          F32 against_block_pos_delta = 0;

                                          if(direction_is_horizontal(against_direction)){
                                              against_block_vel = against_other->block->vel.x;
                                              against_block_pos_delta = against_other->block->pos_delta.x;
                                          }else{
                                              against_block_vel = against_other->block->vel.y;
                                              against_block_pos_delta = against_other->block->pos_delta.y;
                                          }

                                          if(block_vel != against_block_vel || block_pos_delta != against_block_pos_delta) continue;

                                          auto final_against_dir = direction_rotate_clockwise(against_direction, teleport_result.results[block->clone_id].rotations);

                                          against_other->block->connected_teleport.block_index = i;
                                          against_other->block->connected_teleport.direction = direction_opposite(final_against_dir);
                                      }
                                  }

                                  if(block->connected_teleport.block_index >= 0)
                                  {
                                      auto against_result = block_against_other_blocks(block->teleport_pos + block->teleport_pos_delta,
                                                                                       block->cut, block->connected_teleport.direction, world.block_qt,
                                                                                       world.interactive_qt, &world.tilemap);

                                      F32 block_vel = 0;
                                      F32 block_pos_delta = 0;

                                      if(direction_is_horizontal(block->connected_teleport.direction)){
                                          block_vel = block->teleport_vel.x;
                                          block_pos_delta = block->teleport_pos_delta.x;
                                      }else{
                                          block_vel = block->teleport_vel.y;
                                          block_pos_delta = block->teleport_pos_delta.y;
                                      }

                                      // TODO: we may need to handle the against portal rotation data it gives us
                                      for(S16 a = 0; a < against_result.count; a++){
                                          auto* against_other = against_result.againsts + a;

                                          if(get_block_index(&world, against_other->block) != block->connected_teleport.block_index) continue;

                                          F32 against_block_vel = 0;
                                          F32 against_block_pos_delta = 0;

                                          if(direction_is_horizontal(block->connected_teleport.direction)){
                                              against_block_vel = against_other->block->vel.x;
                                              against_block_pos_delta = against_other->block->pos_delta.x;
                                          }else{
                                              against_block_vel = against_other->block->vel.y;
                                              against_block_pos_delta = against_other->block->pos_delta.y;
                                          }

                                          if(block_vel != against_block_vel || block_pos_delta != against_block_pos_delta) continue;

                                          switch(block->connected_teleport.direction){
                                          default:
                                              break;
                                          case DIRECTION_LEFT:
                                          {
                                              block->teleport_pos.pixel.x = against_other->block->pos.pixel.x + block_get_height_in_pixels(block->cut);
                                              block->teleport_pos.decimal.x = against_other->block->pos.decimal.x;
                                              break;
                                          }
                                          case DIRECTION_RIGHT:
                                          {
                                              block->teleport_pos.pixel.x = against_other->block->pos.pixel.x - block_get_height_in_pixels(against_other->block->cut);
                                              block->teleport_pos.decimal.x = against_other->block->pos.decimal.x;
                                              break;
                                          }
                                          case DIRECTION_DOWN:
                                          {
                                              block->teleport_pos.pixel.y = against_other->block->pos.pixel.y + block_get_height_in_pixels(against_other->block->cut);
                                              block->teleport_pos.decimal.y = against_other->block->pos.decimal.y;
                                              break;
                                          }
                                          case DIRECTION_UP:
                                          {
                                              block->teleport_pos.pixel.y = against_other->block->pos.pixel.y - block_get_height_in_pixels(block->cut);
                                              block->teleport_pos.decimal.y = against_other->block->pos.decimal.y;
                                              break;
                                          }
                                          }
                                      }

                                      // clear dis
                                      block->connected_teleport.block_index = -1;
                                  }

                                  if(src_portal->portal.wants_to_turn_off && dst_portal->portal.wants_to_turn_off){
                                      // BlockCut_t original_dst_cut = block->teleport_cut;
                                      BlockCut_t original_src_cut = block->cut;
                                      BlockCut_t final_src_cut = BLOCK_CUT_WHOLE;
                                      BlockCut_t final_dst_cut = BLOCK_CUT_WHOLE;
                                      Pixel_t final_dst_offset{0, 0};
                                      Pixel_t final_src_offset{0, 0};

                                      if(original_src_cut == BLOCK_CUT_TOP_LEFT_QUARTER ||
                                         original_src_cut == BLOCK_CUT_TOP_RIGHT_QUARTER ||
                                         original_src_cut == BLOCK_CUT_BOTTOM_LEFT_QUARTER ||
                                         original_src_cut == BLOCK_CUT_BOTTOM_RIGHT_QUARTER ||
                                         (direction_is_horizontal(src_portal_dir) &&
                                          (original_src_cut == BLOCK_CUT_LEFT_HALF || original_src_cut == BLOCK_CUT_RIGHT_HALF)) ||
                                         (!direction_is_horizontal(src_portal_dir) &&
                                          (original_src_cut == BLOCK_CUT_TOP_HALF || original_src_cut == BLOCK_CUT_BOTTOM_HALF))){
                                          // we kill the block
                                          // TODO: I'm hesitant to actually kill it because things use block index as references, so we move it to the origin
                                          block->teleport_pos.pixel = Pixel_t{-TILE_SIZE_IN_PIXELS, -TILE_SIZE_IN_PIXELS};
                                      }else{
                                          if(original_src_cut == BLOCK_CUT_WHOLE){
                                              switch(src_portal_dir){
                                              default:
                                                 break;
                                              case DIRECTION_LEFT:
                                                 final_src_cut = BLOCK_CUT_RIGHT_HALF;
                                                 final_dst_cut = BLOCK_CUT_LEFT_HALF;
                                                 break;
                                              case DIRECTION_RIGHT:
                                                 final_src_cut = BLOCK_CUT_LEFT_HALF;
                                                 final_dst_cut = BLOCK_CUT_RIGHT_HALF;
                                                 break;
                                              case DIRECTION_DOWN:
                                                 final_src_cut = BLOCK_CUT_TOP_HALF;
                                                 final_dst_cut = BLOCK_CUT_BOTTOM_HALF;
                                                 break;
                                              case DIRECTION_UP:
                                                 final_src_cut = BLOCK_CUT_BOTTOM_HALF;
                                                 final_dst_cut = BLOCK_CUT_TOP_HALF;
                                                 break;
                                              }
                                          }else if(original_src_cut == BLOCK_CUT_LEFT_HALF){
                                              switch(src_portal_dir){
                                              default:
                                                 break;
                                              case DIRECTION_DOWN:
                                                 final_src_cut = BLOCK_CUT_TOP_LEFT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_BOTTOM_LEFT_QUARTER;
                                                 break;
                                              case DIRECTION_UP:
                                                 final_src_cut = BLOCK_CUT_BOTTOM_LEFT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_TOP_LEFT_QUARTER;
                                                 break;
                                              }
                                          }else if(original_src_cut == BLOCK_CUT_RIGHT_HALF){
                                              switch(src_portal_dir){
                                              default:
                                                 break;
                                              case DIRECTION_DOWN:
                                                 final_src_cut = BLOCK_CUT_TOP_RIGHT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
                                                 break;
                                              case DIRECTION_UP:
                                                 final_src_cut = BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_TOP_RIGHT_QUARTER;
                                                 break;
                                              }
                                          }else if(original_src_cut == BLOCK_CUT_TOP_HALF){
                                              switch(src_portal_dir){
                                              default:
                                                 break;
                                              case DIRECTION_LEFT:
                                                 final_src_cut = BLOCK_CUT_TOP_RIGHT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_TOP_LEFT_QUARTER;
                                                 break;
                                              case DIRECTION_RIGHT:
                                                 final_src_cut = BLOCK_CUT_TOP_LEFT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_TOP_RIGHT_QUARTER;
                                                 break;
                                              }
                                          }else if(original_src_cut == BLOCK_CUT_BOTTOM_HALF){
                                              switch(src_portal_dir){
                                              default:
                                                 break;
                                              case DIRECTION_LEFT:
                                                 final_src_cut = BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_BOTTOM_LEFT_QUARTER;
                                                 break;
                                              case DIRECTION_RIGHT:
                                                 final_src_cut = BLOCK_CUT_BOTTOM_LEFT_QUARTER;
                                                 final_dst_cut = BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
                                                 break;
                                              }
                                          }

                                          final_dst_cut = block_cut_rotate_clockwise(final_dst_cut, teleport_result.results[block->clone_id].rotations);

                                          switch(src_portal_dir){
                                          default:
                                             break;
                                          case DIRECTION_LEFT:
                                             final_src_offset.x = block_center_pixel_offset(block->cut).x;
                                             break;
                                          case DIRECTION_RIGHT:
                                             break;
                                          case DIRECTION_DOWN:
                                             final_src_offset.y = block_center_pixel_offset(block->cut).y;
                                             break;
                                          case DIRECTION_UP:
                                             break;
                                          }

                                          switch(dst_portal_dir){
                                          default:
                                              break;
                                          case DIRECTION_LEFT:
                                              final_dst_offset.x = block_center_pixel_offset(block->teleport_cut).x;
                                              break;
                                          case DIRECTION_RIGHT:
                                              break;
                                          case DIRECTION_DOWN:
                                              final_dst_offset.y = block_center_pixel_offset(block->teleport_cut).y;
                                              break;
                                          case DIRECTION_UP:
                                              break;
                                          }

                                          S16 new_block_index = world.blocks.count;
                                          if(resize(&world.blocks, world.blocks.count + (S16)(1))){
                                              // a resize will kill our block ptr, so we gotta update it
                                              block = world.blocks.elements + i;
                                              Block_t* new_block = world.blocks.elements + new_block_index;
                                              *new_block = *block;
                                              new_block->teleport = false;
                                              new_block->cut = final_src_cut;
                                              new_block->pos.pixel += final_src_offset;
                                              new_block->previous_mass = get_block_stack_mass(&world, new_block);
                                          }

                                          block->teleport_pos.pixel += final_dst_offset;
                                          block->teleport_cut = final_dst_cut;
                                          block->teleport_split = true;
                                      }

                                      src_portal->portal.on = false;
                                      dst_portal->portal.on = false;
                                      src_portal->portal.wants_to_turn_off = false;
                                      dst_portal->portal.wants_to_turn_off = false;
                                  }
                              }

                              // TODO: maybe only do this one time per loop in case multiple blocks teleport in a frame
                              // re-calculate the quad tree using the new teleported position for the block
                              quad_tree_free(world.block_qt);
                              world.block_qt = quad_tree_build(&world.blocks);

                              repeat_collision_pass = true;
                         }
                    }

                    // player movement
                    S16 update_player_count = world.players.count; // save due to adding/removing players
                    for(S16 i = 0; i < update_player_count; i++){
                         Player_t* player = world.players.elements + i;

                         Coord_t player_coord = pos_to_coord(player->pos + player->pos_delta);

                         MovePlayerThroughWorldResult_t move_result {};

                         if(player->teleport){
                              move_result = move_player_through_world(player->teleport_pos, player->teleport_vel, player->teleport_pos_delta, player->teleport_face,
                                                                      player->clone_instance, i, player->teleport_pushing_block, player->teleport_pushing_block_dir,
                                                                      player->teleport_pushing_block_rotation, &world);

                              if(move_result.collided) repeat_collision_pass = true;
                              if(move_result.resetting) resetting = true;
                              player->teleport_pos_delta = move_result.pos_delta;
                              player->teleport_pushing_block = move_result.pushing_block;
                              player->teleport_pushing_block_dir = move_result.pushing_block_dir;
                              player->teleport_pushing_block_rotation = move_result.pushing_block_rotation;
                         }else{
                              move_result = move_player_through_world(player->pos, player->vel, player->pos_delta, player->face,
                                                                      player->clone_instance, i, player->pushing_block,
                                                                      player->pushing_block_dir, player->pushing_block_rotation,
                                                                      &world);

                              if(move_result.collided) repeat_collision_pass = true;
                              if(move_result.resetting) resetting = true;
                              player->pos_delta = move_result.pos_delta;
                              player->pushing_block = move_result.pushing_block;
                              player->pushing_block_dir = move_result.pushing_block_dir;
                              player->pushing_block_rotation = move_result.pushing_block_rotation;
                         }

                         auto* portal = player_is_teleporting(player, world.interactive_qt);

                         if(portal && player->clone_start.x == 0){
                              // at the first instant of the block teleporting, check if we should create an entangled_block

                              PortalExit_t portal_exits = find_portal_exits(portal->coord, &world.tilemap, world.interactive_qt);
                              S8 count = portal_exit_count(&portal_exits);
                              if(count >= 3){ // src portal, dst portal, clone portal
                                   world.clone_instance++;

                                   S8 clone_id = 0;
                                   for (auto &direction : portal_exits.directions) {
                                        for(int p = 0; p < direction.count; p++){
                                             if(direction.coords[p] == portal->coord) continue;

                                             if(clone_id == 0){
                                                  player->clone_id = clone_id;
                                                  player->clone_instance = world.clone_instance;
                                             }else{
                                                  S16 new_player_index = world.players.count;

                                                  if(resize(&world.players, world.players.count + (S16)(1))){
                                                       // a resize will kill our player ptr, so we gotta update it
                                                       player = world.players.elements + i;
                                                       player->clone_start = portal->coord;

                                                       Player_t* new_player = world.players.elements + new_player_index;
                                                       *new_player = *player;
                                                       new_player->clone_id = clone_id;
                                                  }
                                             }

                                             clone_id++;
                                        }
                                   }
                              }
                         }else if(!portal && player->clone_start.x > 0){
                              auto clone_portal_center = coord_to_pixel_at_center(player->clone_start);
                              F64 player_distance_from_portal = pixel_distance_between(clone_portal_center, player->pos.pixel);
                              bool from_clone_start = (player_distance_from_portal < TILE_SIZE_IN_PIXELS);

                              if(from_clone_start){
                                   // loop across all players after this one
                                   for(S16 p = 0; p < world.players.count; p++){
                                        if(p == i) continue;
                                        Player_t* other_player = world.players.elements + p;
                                        if(other_player->clone_instance == player->clone_instance){
                                             // TODO: I think I may have a really subtle bug here where we actually move
                                             // TODO: the i'th player around because it was the last in the array
                                             remove(&world.players, p);

                                             // update ptr since we could have resized
                                             player = world.players.elements + i;

                                             update_player_count--;
                                        }
                                   }
                              }else{
                                   for(S16 p = 0; p < world.players.count; p++){
                                        if(p == i) continue;
                                        Player_t* other_player = world.players.elements + p;
                                        if(other_player->clone_instance == player->clone_instance){
                                             other_player->clone_id = 0;
                                             other_player->clone_instance = 0;
                                             other_player->clone_start = Coord_t{};
                                        }
                                   }

                                   // turn off the circuit
                                   activate(&world, player->clone_start);
                                   auto* src_portal = quad_tree_find_at(world.interactive_qt, player->clone_start.x, player->clone_start.y);
                                   if(is_active_portal(src_portal)) src_portal->portal.on = false;
                              }

                              player->clone_id = 0;
                              player->clone_instance = 0;
                              player->clone_start = Coord_t{};
                         }

                         Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, player_coord.x, player_coord.y);
                         if(interactive && interactive->type == INTERACTIVE_TYPE_CLONE_KILLER){
                              if(i == 0){
                                   resize(&world.players, 1);
                                   update_player_count = 1;
                              }else{
                                   resetting = true;
                              }
                         }

                         auto result = player_in_block_rect(player, &world.tilemap, world.interactive_qt, world.block_qt);
                         for(S8 e = 0; e < result.entry_count; e++){
                              auto& entry = result.entries[e];
                              if(entry.block_pos.z == player->pos.z - HEIGHT_INTERVAL){
                                   auto block_index = get_block_index(&world, entry.block);
                                   auto block_pos_delta = entry.block->teleport ? entry.block->teleport_pos_delta : entry.block->pos_delta;
                                   auto rotated_pos_delta = vec_rotate_quadrants_clockwise(block_pos_delta, entry.portal_rotations);
                                   auto old_carried_pos_delta = player->carried_pos_delta.positive + player->carried_pos_delta.negative;
                                   bool carried = get_carried_noob(&player->carried_pos_delta, rotated_pos_delta, block_index, false);
                                   if(!carried){
                                        auto new_carried_pos_delta = player->carried_pos_delta.positive + player->carried_pos_delta.negative;

                                        if(player->teleport){
                                             player->teleport_pos_delta -= old_carried_pos_delta;
                                             player->teleport_pos_delta += new_carried_pos_delta;
                                        }else{
                                             player->pos_delta -= old_carried_pos_delta;
                                             player->pos_delta += new_carried_pos_delta;
                                        }

                                        for(S16 p = 0; p < world.players.count; p++){
                                             if(i == p) continue;

                                             auto tmp_player = world.players.elements + p;
                                             auto relative_rotation = direction_rotations_between((Direction_t)(player->rotation), (Direction_t)(tmp_player->rotation));
                                             auto local_pos_delta = vec_rotate_quadrants_clockwise(rotated_pos_delta, relative_rotation);
                                             old_carried_pos_delta = tmp_player->carried_pos_delta.positive + tmp_player->carried_pos_delta.negative;

                                             get_carried_noob(&tmp_player->carried_pos_delta, local_pos_delta, block_index, true);

                                             new_carried_pos_delta = tmp_player->carried_pos_delta.positive + tmp_player->carried_pos_delta.negative;

                                             if(tmp_player->teleport){
                                                  tmp_player->teleport_pos_delta -= old_carried_pos_delta;
                                                  tmp_player->teleport_pos_delta += new_carried_pos_delta;
                                             }else{
                                                  tmp_player->pos_delta -= old_carried_pos_delta;
                                                  tmp_player->pos_delta += new_carried_pos_delta;
                                             }
                                        }

                                        repeat_collision_pass = true;
                                   }
                              }
                         }
                    }

                    // based on changing pos_deltas, determine if we are teleporting
                    for(S16 i = 0; i < update_player_count; i++){
                         auto player = world.players.elements + i;

                         auto player_pos = player->pos;
                         auto player_pos_delta = player->pos_delta;

                         if(player->teleport){
                              player_pos = player->teleport_pos;
                              player_pos_delta = player->teleport_pos_delta;
                         }

                         auto player_prev_coord = pos_to_coord(player_pos);
                         auto player_cur_coord = pos_to_coord(player_pos + player_pos_delta);

                         // if the player has teleported, but stays in the portal coord, undo the teleport and shorten
                         // the pos_delta based on the collision that happened after teleporting
                         if(player->teleport && player_prev_coord == player_cur_coord){
                              player->teleport = false;
                              auto unrotated_pos_delta = vec_rotate_quadrants_counter_clockwise(player->teleport_pos_delta, player->teleport_rotation);
                              player->pos_delta = unrotated_pos_delta;
                              player_pos = player->pos;
                              player_pos_delta = player->pos_delta;
                              player_prev_coord = pos_to_coord(player_pos);
                              player_cur_coord = pos_to_coord(player_pos + player_pos_delta);
                         }

                         // teleport position
                         auto teleport_result = teleport_position_across_portal(player_pos, player_pos_delta, &world,
                                                                                player_prev_coord, player_cur_coord);
                         auto teleport_clone_id = player->clone_id;
                         if(player_cur_coord != player->clone_start){
                              // if we are going back to our clone portal, then all clones should go back

                              // find the index closest to our original clone portal
                              F32 shortest_distance = FLT_MAX;
                              auto clone_start_center = coord_to_pixel_at_center(player->clone_start);

                              for(S8 t = 0; t < teleport_result.count; t++){
                                   F32 distance = pixel_distance_between(clone_start_center, teleport_result.results[t].pos.pixel);
                                   if(distance < shortest_distance){
                                        shortest_distance = distance;
                                        teleport_clone_id = t;
                                   }
                              }
                         }

                         // if a teleport happened, update the position
                         if(teleport_result.count){
                              assert(teleport_result.count > teleport_clone_id);

                              player->teleport = true;
                              player->teleport_pos = teleport_result.results[teleport_clone_id].pos;
                              player->teleport_pos_delta = teleport_result.results[teleport_clone_id].delta;
                              player->teleport_vel = vec_rotate_quadrants_clockwise(player->vel, teleport_result.results[teleport_clone_id].rotations);
                              player->teleport_accel = vec_rotate_quadrants_clockwise(player->accel, teleport_result.results[teleport_clone_id].rotations);
                              player->teleport_rotation = teleport_result.results[teleport_clone_id].rotations;
                              player->teleport_face = direction_rotate_clockwise(player->face, teleport_result.results[teleport_clone_id].rotations);
                              player->teleport_pushing_block = player->pushing_block;
                              player->teleport_pushing_block_rotation = 0;
                              player->teleport_pushing_block_dir = player->pushing_block_dir;

                              repeat_collision_pass = true;
                         }
                    }

                    collision_attempts++;
               }

               collision_results.clear();

               // add entangled pushes
               for(S16 i = 0; i < all_block_pushes.count; i++){
                    auto& block_push = all_block_pushes.pushes[i];
                    if(block_push.invalidated) continue;

                    // if the block being pushed is entangled, add block pushes for those
                    Block_t* pushee = world.blocks.elements + block_push.pushee_index;

                    if(pushee->entangle_index < 0) continue;

                    DirectionMask_t pushable_direction_mask = DIRECTION_MASK_NONE;
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                        Direction_t direction = static_cast<Direction_t>(d);
                        if(!direction_in_mask(block_push.direction_mask, direction)) continue;
                        if(!block_pushable(pushee, direction, &world)) continue;
                        pushable_direction_mask = direction_mask_add(pushable_direction_mask, direction);
                    }

                    if(pushable_direction_mask == DIRECTION_MASK_NONE) continue;

                    if(!block_push.is_entangled()){
                         S16 current_entangle_index = pushee->entangle_index;
                         while(current_entangle_index != block_push.pushee_index && current_entangle_index >= 0){
                             Block_t* entangler = world.blocks.elements + current_entangle_index;
                             BlockPush_t new_block_push = block_push;
                             S8 rotations_between_blocks = blocks_rotations_between(entangler, pushee);
                             new_block_push.direction_mask = pushable_direction_mask;
                             new_block_push.pushee_index = current_entangle_index;
                             new_block_push.portal_rotations = block_push.portal_rotations;
                             new_block_push.entangle_rotations = rotations_between_blocks;
                             new_block_push.entangled_with_push_index = i;
                             all_block_pushes.add(&new_block_push);
                             current_entangle_index = entangler->entangle_index;
                         }
                    }
               }

               // pass to cause pushes to happen
               {
                    BlockMomentumChanges_t momentum_changes;
                    S16 simultaneous_block_pushes = 0;

                    consolidate_block_pushes(&all_block_pushes, &all_consolidated_block_pushes);

#if 0
                    if(all_block_pushes.count > 0){
                         LOG("all block pushes: %d\n", all_block_pushes.count);
                    }

                    log_block_pushes(all_block_pushes);

                    if(all_consolidated_block_pushes.count > 0){
                         LOG("pre collision push consolidated block pushes: %d\n", all_consolidated_block_pushes.count);
                    }

                    log_block_pushes(all_consolidated_block_pushes);
#endif

                    for(S16 i = 0; i < all_consolidated_block_pushes.count; i++){
                         auto& block_push = all_consolidated_block_pushes.pushes[i];
                         if(block_push.invalidated) continue;

                         auto result = block_collision_push(&block_push, &world);

                         // any momentum changes that stops or changes a blocks momentum, means that block could not have pushed anything, so invalidate it's pushes
                         for(S16 j = i + 1; j < all_consolidated_block_pushes.count; j++){
                             auto& check_block_push = all_consolidated_block_pushes.pushes[j];

                             // if the entangled push has already been executed, then we can't invalidate it
                             if(check_block_push.is_entangled()){
                                 if(i >= check_block_push.entangled_with_push_index) continue;
                             }

                             for(S16 m = 0; m < result.momentum_changes.count; m++){
                                 auto& block_change = result.momentum_changes.changes[m];

                                 for(S16 p = 0; p < check_block_push.pusher_count; p++){
                                     auto& check_pusher = check_block_push.pushers[p];

                                     if(check_pusher.index != block_change.block_index) continue;
                                     if(check_pusher.hit_entangler) continue;
                                     if(block_pushes_are_the_same_collision(all_consolidated_block_pushes, i, j, check_pusher.index)) continue;

                                     if(block_change.x){
                                         if(direction_in_mask(check_block_push.direction_mask, DIRECTION_LEFT) && block_change.vel >= 0){
                                             check_block_push.remove_pusher(p);
                                             p--;
                                         }else if(direction_in_mask(check_block_push.direction_mask, DIRECTION_RIGHT) && block_change.vel <= 0){
                                             check_block_push.remove_pusher(p);
                                             p--;
                                         }
                                     }else{
                                         if(direction_in_mask(check_block_push.direction_mask, DIRECTION_DOWN) && block_change.vel >= 0){
                                             check_block_push.remove_pusher(p);
                                             p--;
                                         }else if(direction_in_mask(check_block_push.direction_mask, DIRECTION_UP) && block_change.vel <= 0){
                                             check_block_push.remove_pusher(p);
                                             p--;
                                         }
                                     }
                                 }
                             }
                         }

                         momentum_changes.merge(&result.momentum_changes);

                         if(result.additional_block_pushes.count){
                             all_consolidated_block_pushes.merge(&result.additional_block_pushes);
                             // TODO: reconsolidate?
                         }

                         // TODO: I don't think getting the max collided with block count is right fore determining
                         // the cancellable block pushes
                         S16 max_collided_with_block_count = 0;
                         for(S16 p = 0; p < block_push.pusher_count; p++){
                              if(block_push.pushers[p].collided_with_block_count > max_collided_with_block_count){
                                   max_collided_with_block_count = block_push.pushers[p].collided_with_block_count;
                              }
                         }

                         // for simultaneous pushes, skip ahead because they should not be cancelled
                         S16 cancellable_block_pushes = i + 1;
                         if(simultaneous_block_pushes > 0){
                              simultaneous_block_pushes--;
                              cancellable_block_pushes += simultaneous_block_pushes;
                         }else if(max_collided_with_block_count > 1){
                              simultaneous_block_pushes = max_collided_with_block_count - 1;
                              cancellable_block_pushes += simultaneous_block_pushes;
                         }

                         if(result.reapply_push){
                             i--;
                         }
                    }

#if 0
                    if(momentum_changes.count > 0){
                         LOG("momentum changes: %d\n", momentum_changes.count);
                    }
                    for(S16 c = 0; c < momentum_changes.count; c++){
                         auto& block_change = momentum_changes.changes[c];
                         LOG("  block %d m: %d, v: %f in %s\n", block_change.block_index, block_change.mass, block_change.vel, block_change.x ? "x" : "y");
                    }
#endif

                    // TODO: Loop over momentum changes and build a list of blocks for us to loop over here

                    for(S16 i = 0; i < world.blocks.count; i++){
                         auto* block = world.blocks.elements + i;
                         auto block_mass = get_block_stack_mass(&world, block);

                         S16 x_changes = 0;
                         S16 y_changes = 0;
                         Vec_t new_vel = vec_zero();

                         // clear momentum for each impacted block
                         for(S16 c = 0; c < momentum_changes.count; c++){
                              auto& block_change = momentum_changes.changes[c];
                              if(block_change.block_index != i) continue;

                              F32 ratio = (F32)(block_change.mass) / (F32)(block_mass);

                              if(block_change.x){
                                   new_vel.x += block_change.vel * ratio;
                                   x_changes++;
                              }else{
                                   new_vel.y += block_change.vel * ratio;
                                   y_changes++;
                              }
                         }

                         if(x_changes) block->vel.x = new_vel.x;
                         if(y_changes) block->vel.y = new_vel.y;
                    }

                    // set coasting or idling based on velocity
                    for(S16 c = 0; c < momentum_changes.count; c++){
                         auto& block_change = momentum_changes.changes[c];
                         auto* block = world.blocks.elements + block_change.block_index;
                         if(block_change.x){
                              if(block->vel.x == 0){
                                   block->horizontal_move.state = MOVE_STATE_IDLING;
                                   block->horizontal_move.sign = MOVE_SIGN_ZERO;
                              }else{
                                   block->horizontal_move.state = MOVE_STATE_COASTING;
                                   block->horizontal_move.sign = move_sign_from_vel(block->vel.x);
                                   block->accel.x = 0;
                              }
                              block->horizontal_move.time_left = 0;
                         }else{
                              if(block->vel.y == 0){
                                   block->vertical_move.state = MOVE_STATE_IDLING;
                                   block->vertical_move.sign = MOVE_SIGN_ZERO;
                              }else{
                                   block->vertical_move.state = MOVE_STATE_COASTING;
                                   block->vertical_move.sign = move_sign_from_vel(block->vel.y);
                                   block->accel.y = 0;
                              }
                              block->vertical_move.time_left = 0;
                         }
                    }
               }

               // finalize positions
               for(S16 i = 0; i < world.players.count; i++){
                    auto player = world.players.elements + i;

                    if(player->teleport){
                         player->pos = player->teleport_pos + player->teleport_pos_delta;
                         player->pos_delta = player->teleport_pos_delta;

                         player->face = player->teleport_face;
                         player->vel = player->teleport_vel;
                         player->accel = player->teleport_accel;
                         player->pushing_block = player->teleport_pushing_block;
                         player->pushing_block_dir = player->teleport_pushing_block_dir;
                         player->pushing_block_rotation = player->teleport_pushing_block_rotation;
                         player->rotation = (player->rotation + player->teleport_rotation) % DIRECTION_COUNT;

                         auto first_player = world.players.elements + 0;

                         // set everyone's rotation relative to the first player
                         if(i != 0) player->rotation = direction_rotations_between((Direction_t)(player->rotation), (Direction_t)(first_player->rotation));

                         // set rotations for each direction the player wants to move
                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              if(player_action.move[d]){
                                   player->move_rotation[d] = (player->move_rotation[d] + first_player->teleport_rotation) % DIRECTION_COUNT;
                              }
                         }
                    }else{
                         player->pos += player->pos_delta;
                    }

                    build_move_actions_from_player(&player_action, player, move_actions, DIRECTION_COUNT);

                    auto pure_input_pos_delta = player->pos_delta;
                    pure_input_pos_delta -= player->carried_pos_delta.positive + player->carried_pos_delta.negative;

                    // if we have stopped short in either component, kill the movement for that component if we are no longer pressing keys for it
                    if(fabs(pure_input_pos_delta.x) < fabs(player->pos_delta_save.x)){
                         if((pure_input_pos_delta.x < 0 && !move_actions[DIRECTION_LEFT]) ||
                            (pure_input_pos_delta.x > 0 && !move_actions[DIRECTION_RIGHT])){
                              player->vel.x = 0;
                              player->accel.x = 0;
                         }
                    }

                    if(fabs(pure_input_pos_delta.y) < fabs(player->pos_delta_save.y)){
                         if((pure_input_pos_delta.y < 0 && !move_actions[DIRECTION_DOWN]) ||
                            (pure_input_pos_delta.y > 0 && !move_actions[DIRECTION_UP])){
                              player->vel.y = 0;
                              player->accel.y = 0;
                         }
                    }
               }

               // reset the first player's rotation
               if(world.players.count > 0){
                    world.players.elements[0].rotation = 0;
               }

               // calculate stop_on_pixels for blocks carrying other blocks
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;

                    auto result = block_held_up_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap,
                                                                 BLOCK_FRICTION_AREA);
                    for(S16 b = 0; b < result.count; b++){
                         auto holder = result.blocks_held[b].block;

                         if(block->stop_on_pixel_x == 0 && holder->stop_on_pixel_x != 0){
                              S16 pixel_diff = (holder->pos.pixel.x - block->pos.pixel.x);
                              block->stop_on_pixel_x = holder->stop_on_pixel_x + pixel_diff;
                         }

                         if(block->stop_on_pixel_y == 0 && holder->stop_on_pixel_y != 0){
                              S16 pixel_diff = (holder->pos.pixel.y - block->pos.pixel.y);
                              block->stop_on_pixel_y = holder->stop_on_pixel_y + pixel_diff;
                         }
                    }
               }

               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;

                    Position_t final_pos;

                    if(block->teleport){
                         final_pos = block->teleport_pos + block->teleport_pos_delta;

                         block->pos_delta = block->teleport_pos_delta;
                         block->vel = block->teleport_vel;
                         block->accel = block->teleport_accel;
                         block->stop_on_pixel_x = block->teleport_stop_on_pixel_x;
                         block->stop_on_pixel_y = block->teleport_stop_on_pixel_y;
                         block->rotation = (block->rotation + block->teleport_rotation) % static_cast<U8>(DIRECTION_COUNT);
                         block->horizontal_move = block->teleport_horizontal_move;
                         block->vertical_move = block->teleport_vertical_move;
                         block->cut = block->teleport_cut;

                         if(block->teleport_split){
                             // TODO: unsure about this *fix*
                             block->previous_mass = get_block_stack_mass(&world, block);
                         }

                         // reset started_on_pixel since we teleported and no longer want to follow those as a rule
                         block->started_on_pixel_x = 0;
                         block->started_on_pixel_y = 0;
                    }else{
                         final_pos = block->pos + block->pos_delta;
                    }

                    // finalize position for each component, stopping on a pixel boundary if we have to
                    if(block->stop_on_pixel_x != 0){
                         block->pos.pixel.x = block->stop_on_pixel_x;
                         block->pos.decimal.x = 0;
                         block->stop_on_pixel_x = 0;

                         block->stopped_by_player_horizontal = false;
                    }else{
                         block->pos.pixel.x = final_pos.pixel.x;
                         block->pos.decimal.x = final_pos.decimal.x;
                    }

                    if(block->stop_on_pixel_y != 0){
                         block->pos.pixel.y = block->stop_on_pixel_y;
                         block->pos.decimal.y = 0;
                         block->stop_on_pixel_y = 0;

                         block->stopped_by_player_vertical = false;
                    }else{
                         block->pos.pixel.y = final_pos.pixel.y;
                         block->pos.decimal.y = final_pos.decimal.y;
                    }

#if DEBUG_FILE
                    if(i == 2){
                         fprintf(debug_file, "%ld %d %d %f %f hm %s %s %f vm %s %s %f\n", frame_count,
                                 block->pos.pixel.x, block->pos.pixel.y, block->pos.decimal.x, block->pos.decimal.y,
                                 move_state_to_string(block->horizontal_move.state), move_sign_to_string(block->horizontal_move.sign), block->horizontal_move.distance,
                                 move_state_to_string(block->vertical_move.state), move_sign_to_string(block->vertical_move.sign), block->vertical_move.distance);
                         fflush(debug_file);
                    }
#endif
               }

               // have player push block
               for(S16 i = 0; i < world.players.count; i++){
                    auto player = world.players.elements + i;
                    if(player->prev_pushing_block >= 0 && player->prev_pushing_block == player->pushing_block){
                         Block_t* block_to_push = world.blocks.elements + player->prev_pushing_block;
                         DirectionMask_t block_move_dir_mask = vec_direction_mask(block_to_push->vel);
                         DirectionMask_t teleport_block_move_dir_mask = block_move_dir_mask;
                         auto push_block_dir = player->pushing_block_dir;
                         auto teleport_push_block_dir = push_block_dir;

                         if(block_to_push->teleport){
                             teleport_push_block_dir = direction_rotate_clockwise(push_block_dir, block_to_push->teleport_rotation);
                             teleport_block_move_dir_mask = direction_mask_rotate_clockwise(teleport_block_move_dir_mask, block_to_push->teleport_rotation);
                         }

                         auto total_block_mass = get_block_mass_in_direction(&world, block_to_push, teleport_push_block_dir);
                         auto mass_ratio = (F32)(block_get_mass(block_to_push)) / (F32)(total_block_mass);

                         auto expected_final_velocity = get_block_expected_player_push_velocity(&world, block_to_push, mass_ratio);
                         bool already_moving_fast_enough = false;

                         if(direction_in_mask(teleport_block_move_dir_mask, teleport_push_block_dir)){
                              block_to_push->cur_push_mask = direction_mask_add(block_to_push->cur_push_mask, teleport_push_block_dir);

                              switch(teleport_push_block_dir){
                              default:
                                   break;
                              case DIRECTION_LEFT:
                                   already_moving_fast_enough = (block_to_push->vel.x <= -expected_final_velocity);
                                   break;
                              case DIRECTION_RIGHT:
                                   already_moving_fast_enough = (block_to_push->vel.x >= expected_final_velocity);
                                   break;
                              case DIRECTION_DOWN:
                                   already_moving_fast_enough = (block_to_push->vel.y <= -expected_final_velocity);
                                   break;
                              case DIRECTION_UP:
                                   already_moving_fast_enough = (block_to_push->vel.y >= expected_final_velocity);
                                   break;
                              }
                         }

                         if(already_moving_fast_enough){
                              // pass
                              player->push_time += dt;
                              player->entangle_push_time += dt;
                         }else if(direction_in_mask(direction_mask_opposite(teleport_block_move_dir_mask), teleport_push_block_dir)){
                              // if the player is pushing against a block moving towards them, the block wins
                              player->push_time = 0;
                              player->pushing_block = -1;
                         }else{
                              F32 save_push_time = player->push_time;

                              // TODO: get back to this once we improve our demo tools
                              player->push_time += dt;
                              player->entangle_push_time += dt;
                              if(player->push_time > BLOCK_PUSH_TIME){
                                   // if this is the frame that causes the block to be pushed, make a commit
                                   if(save_push_time <= BLOCK_PUSH_TIME) undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                                   auto allowed_result = allowed_to_push(&world, block_to_push, push_block_dir);
                                   if(allowed_result.push){
                                        auto push_result = block_push(block_to_push, push_block_dir, &world, false, allowed_result.mass_ratio);

                                        if(!push_result.pushed && !push_result.busy){
                                             player->push_time = 0.0f;
                                        }else if(player->entangle_push_time > BLOCK_PUSH_TIME && block_to_push->entangle_index >= 0 && block_to_push->entangle_index < world.blocks.count){
                                             player->pushing_block_dir = push_block_dir;
                                             push_entangled_block(block_to_push, &world, push_block_dir, false, allowed_result.mass_ratio);
                                             player->entangle_push_time = 0.0f;
                                        }

                                        if(block_to_push->pos.z > 0) player->push_time = -0.5f; // TODO: wtf is this line?
                                   }
                              }
                         }
                    }else{
                         player->push_time = 0;
                         player->entangle_push_time = 0;
                    }
               }

               // update interactives
               for(S16 i = 0; i < world.interactives.count; i++){
                    Interactive_t* interactive = world.interactives.elements + i;
                    if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                         // if the tile is iced, the pressure plate shouldn't change state
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, interactive->coord);
                         if(tile && tile_is_iced(tile)) continue;

                         bool should_be_down = false;
                         for(S16 p = 0; p < world.players.count; p++){
                              if(world.players.elements[p].pos.z != 0) continue;
                              Coord_t player_coord = pos_to_coord(world.players.elements[p].pos);
                              if(interactive->coord == player_coord){
                                   should_be_down = true;
                                   break;
                              }
                         }

                         if(!should_be_down){
                              Position_t plate_pos = coord_to_pos(interactive->coord);
                              Quad_t plate_quad {0, 0, TILE_SIZE, TILE_SIZE};
                              Rect_t rect = rect_to_check_surrounding_blocks(coord_to_pixel_at_center(interactive->coord));
                              S16 mass_on_pressure_plate = 0;

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                              for(S16 b = 0; b < block_count; b++){
                                   if(blocks[b]->pos.z != 0) continue;

                                   Position_t block_pos = blocks[b]->teleport ? blocks[b]->teleport_pos : blocks[b]->pos;
                                   Position_t relative_block_pos = block_pos - plate_pos;
                                   Vec_t relative_block_pos_vec = pos_to_vec(relative_block_pos);
                                   S16 block_width = block_get_width_in_pixels(blocks[b]);
                                   S16 block_height = block_get_height_in_pixels(blocks[b]);
                                   Quad_t block_quad {relative_block_pos_vec.x, relative_block_pos_vec.y,
                                                      relative_block_pos_vec.x + ((F32)block_width * PIXEL_SIZE),
                                                      relative_block_pos_vec.y + ((F32)block_height * PIXEL_SIZE)};

                                   if(quad_in_quad_high_range_exclusive(&block_quad, &plate_quad)){
                                        mass_on_pressure_plate += get_block_stack_mass(&world, blocks[b]);
                                   }
                              }

                              if(mass_on_pressure_plate >= block_get_mass(BLOCK_CUT_TOP_HALF)){
                                   should_be_down = true;
                              }
                         }

                         if(should_be_down != interactive->pressure_plate.down){
                              activate(&world, interactive->coord);
                              interactive->pressure_plate.down = should_be_down;
                         }
                    }
               }

               // illuminate and spread ice
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    if(block->element == ELEMENT_FIRE){
                         U8 block_light_height = (block->pos.z / HEIGHT_INTERVAL) * LIGHT_DECAY;
                         illuminate(block_get_coord(block), 255 - block_light_height, &world);
                    }else if(block->element == ELEMENT_ICE){
                         auto block_coord = block_get_coord(block);
                         spread_ice(block_coord, block->pos.z + HEIGHT_INTERVAL, 1, &world);
                    }
               }

               // melt ice in a separate pass
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    if(block->element == ELEMENT_FIRE){
                         melt_ice(block_get_coord(block), block->pos.z + HEIGHT_INTERVAL, 1, &world);
                    }
               }

               // update light and ice detectors
               for(S16 i = 0; i < world.interactives.count; i++){
                    update_light_and_ice_detectors(world.interactives.elements + i, &world);
               }

               if(resetting){
                    reset_timer += dt;
                    if(reset_timer >= RESET_TIME){
                         resetting = false;
                         load_map_number_map(map_number, &world, &undo, &player_start, &player_action);
                    }
               }else{
                    reset_timer -= dt;
                    if(reset_timer <= 0) reset_timer = 0;
               }
          }

          if((suite && !show_suite) || play_demo.seek_frame >= 0) continue;

          // begin drawing
          Position_t screen_camera = camera - Vec_t{0.5f, 0.5f} + Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};

          Coord_t min = pos_to_coord(screen_camera);
          Coord_t max = min + Coord_t{ROOM_TILE_SIZE, ROOM_TILE_SIZE};
          min = coord_clamp_zero_to_dim(min, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));
          max = coord_clamp_zero_to_dim(max, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));
          Position_t tile_bottom_left = coord_to_pos(min);
          Vec_t camera_offset = pos_to_vec(tile_bottom_left - screen_camera);

          glClear(GL_COLOR_BUFFER_BIT);

          // draw flats
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          for(S16 y = max.y; y >= min.y; y--){
               draw_world_row_flats(y, min.x, max.y, &world.tilemap, world.interactive_qt, camera_offset);
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Coord_t coord {x, y};
                    Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, coord.x, coord.y);

                    if(is_active_portal(interactive)){
                         PortalExit_t portal_exits = find_portal_exits(coord, &world.tilemap, world.interactive_qt);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                                   Rect_t coord_rect = rect_surrounding_coord(portal_coord);
                                   coord_rect.left -= HALF_TILE_SIZE_IN_PIXELS;
                                   coord_rect.right += HALF_TILE_SIZE_IN_PIXELS;
                                   coord_rect.bottom -= TILE_SIZE_IN_PIXELS;
                                   coord_rect.top += HALF_TILE_SIZE_IN_PIXELS;

                                   S16 block_count = 0;
                                   Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);

                                   quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                                   if(block_count){
                                        sort_blocks_by_descending_height(blocks, block_count);

                                        draw_portal_blocks(blocks, block_count, portal_coord, coord, portal_rotations, camera_offset);
                                   }

                                   glEnd();
                                   glBindTexture(GL_TEXTURE_2D, player_texture);
                                   glBegin(GL_QUADS);
                                   glColor3f(1.0f, 1.0f, 1.0f);

                                   auto player_region = rect_surrounding_coord(portal_coord);
                                   player_region.left -= 4;
                                   player_region.right += 4;
                                   player_region.bottom -= 10;
                                   player_region.top += 4;
                                   draw_portal_players(&world.players, player_region, portal_coord, coord, portal_rotations, camera_offset);

                                   glEnd();
                                   glBindTexture(GL_TEXTURE_2D, theme_texture);
                                   glBegin(GL_QUADS);
                                   glColor3f(1.0f, 1.0f, 1.0f);
                              }
                         }
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Coord_t coord {x, y};
                    Tile_t* tile = tilemap_get_tile(&world.tilemap, coord);
                    if(tile && tile->id >= 16){
                         Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                         (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                         draw_tile_id(tile->id, tile_pos);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               draw_world_row_solids(y, min.x, max.y, &world.tilemap, world.interactive_qt, world.block_qt,
                                     &world.players, camera_offset, player_texture);

               glEnd();
               glBindTexture(GL_TEXTURE_2D, arrow_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               draw_world_row_arrows(y, min.x, max.x, &world.arrows, camera_offset);

               glEnd();

               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);
          }

          glEnd();

#if 0
          // light
          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_QUADS);
          for(S16 y = min.y; y <= max.y; y++){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = world.tilemap.tiles[y] + x;

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                    glColor4f(0.0f, 0.0f, 0.0f, (F32)(255 - tile->light) / 255.0f);
                    glVertex2f(tile_pos.x, tile_pos.y);
                    glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);
               }
          }
          glEnd();
#endif

          // player start
          draw_selection(player_start, player_start, screen_camera, 0.0f, 1.0f, 0.0f);

          // editor
          draw_editor(&editor, &world, screen_camera, mouse_screen, theme_texture, text_texture);

          if(reset_timer >= 0.0f){
               glBegin(GL_QUADS);
               glColor4f(0.0f, 0.0f, 0.0f, reset_timer / RESET_TIME);
               glVertex2f(0, 0);
               glVertex2f(0, 1);
               glVertex2f(1, 1);
               glVertex2f(1, 0);
               glEnd();
          }

          if(play_demo.mode == DEMO_MODE_PLAY){
               F32 demo_pct = (F32)(frame_count) / (F32)(play_demo.last_frame);
               Quad_t pct_bar_quad = {pct_bar_outline_quad.left, pct_bar_outline_quad.bottom, demo_pct, pct_bar_outline_quad.top};
               draw_quad_filled(&pct_bar_quad, 255.0f, 255.0f, 255.0f);
               draw_quad_wireframe(&pct_bar_outline_quad, 255.0f, 255.0f, 255.0f);

               char buffer[64];
               snprintf(buffer, 64, "F: %ld/%ld C: %d", frame_count, play_demo.last_frame, collision_attempts);

               glBindTexture(GL_TEXTURE_2D, text_texture);
               glBegin(GL_QUADS);

               Vec_t text_pos {0.005f, 0.965f};

               if(editor.mode) text_pos.y -= 0.09f;

               glColor3f(0.0f, 0.0f, 0.0f);
               draw_text(buffer, text_pos + Vec_t{0.002f, -0.002f});

               glColor3f(1.0f, 1.0f, 1.0f);
               draw_text(buffer, text_pos);

               draw_input_on_hud('L', Vec_t{0.965f - (7.5f * TEXT_CHAR_WIDTH), 0.965f}, player_action.move[DIRECTION_LEFT]);
               draw_input_on_hud('U', Vec_t{0.965f - (6.0f * TEXT_CHAR_WIDTH), 0.965f}, player_action.move[DIRECTION_UP]);
               draw_input_on_hud('R', Vec_t{0.965f - (4.5f * TEXT_CHAR_WIDTH), 0.965f}, player_action.move[DIRECTION_RIGHT]);
               draw_input_on_hud('D', Vec_t{0.965f - (3.0f * TEXT_CHAR_WIDTH), 0.965f}, player_action.move[DIRECTION_DOWN]);
               draw_input_on_hud('A', Vec_t{0.965f - (1.5f * TEXT_CHAR_WIDTH), 0.965f}, player_action.activate);
               draw_input_on_hud('B', Vec_t{0.965f - (0.0f * TEXT_CHAR_WIDTH), 0.965f}, player_action.activate);

               glEnd();

          }

          SDL_GL_SwapWindow(window);
     }

     if(play_demo.mode == DEMO_MODE_PLAY){
          fclose(play_demo.file);
     }

     if(record_demo.mode == DEMO_MODE_RECORD){
          player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_END_DEMO, record_demo.mode, record_demo.file, frame_count);

          // save map and player position
          save_map_to_file(record_demo.file, player_start, &world.tilemap, &world.blocks, &world.interactives);

          switch(record_demo.version){
          default:
               break;
          case 1:
               fwrite(&world.players.elements->pos.pixel, sizeof(world.players.elements->pos.pixel), 1, record_demo.file);
               break;
          case 2:
          {
               fwrite(&world.players.count, sizeof(world.players.count), 1, record_demo.file);
               for(S16 p = 0; p < world.players.count; p++){
                    Player_t* player = world.players.elements + p;
                    fwrite(&player->pos.pixel, sizeof(player->pos.pixel), 1, record_demo.file);
               }
          } break;
          }
          fclose(record_demo.file);
     }

     quad_tree_free(world.interactive_qt);
     quad_tree_free(world.block_qt);

     destroy(&world.blocks);
     destroy(&world.interactives);
     destroy(&undo);
     destroy(&world.tilemap);
     destroy(&editor);

     if(!suite){
          glDeleteTextures(1, &theme_texture);
          glDeleteTextures(1, &player_texture);
          glDeleteTextures(1, &arrow_texture);
          glDeleteTextures(1, &text_texture);

          SDL_GL_DeleteContext(opengl_context);
          SDL_DestroyWindow(window);
          SDL_Quit();
     }

#if DEBUG_FILE
     fclose(debug_file);
#endif

     Log_t::destroy();
     return 0;
}
