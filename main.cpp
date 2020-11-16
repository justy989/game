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
- a block travelling diagonally at a wall will stop on both axis' against the wall because of the collision
- player is able to get into a block sometimes when standing on a popup that is going down and pushing a block that has fallen off of it
- blocks moving fast enough can also cause the player to get inside
- arrows that fall to the ground slow to a stop, even on ice, which don't make no sense, they should detect ice
- player pushing a block into a stack and keeps pushing, the pushed block, while it doesn't move, still has a velocity in that direction
- infinite recursion for entangled against pushes (may or may not be important that the pushes go through portals)
- you can't stop a momentum transfer when the player is on the other side of a portal like you can when no portals are involved
- adjacent entangled blocks on ice being hit need to be sorted so the pushes happen in order of the force, this is shown on the top case of map 278
- when stopping a pair of adjacent blocks sliding on ice, there is a bug where the player can get the block closer to the player to break off of the chain
  and continue going one more tile slowly before stopping
- a whole block entangled with a corner block where you push the whole block and try to stop the corner block, the
  corner block is moving crazy fast and the player doesn't stop it as you would expect

- for collision, can shorten things pos_deltas before we cause pushes to happen on ice in the same frame due to collisions ?
- A block on the tile outside a portal pushed into the portal to clone, the clone has weird behavior and ends up on the portal block
- When pushing a block through a portal that turns off, the block keeps going
- Getting a block and it's rotated entangler to push into the centroid causes any other entangled blocks to alternate pushing
- Pushing a block and shooting an arrow causes the player to go invisible
- The -test command line option used on it's own with -play cannot load the map from the demo

Features:
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

// enable GLEXT
#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// linux specific
#include <dirent.h>

#include "log.h"
#include "direction.h"
#include "conversion.h"
#include "portal_exit.h"
#include "utils.h"
#include "map_format.h"
#include "draw.h"
#include "block_utils.h"
#include "collision.h"
#include "world.h"
#include "editor.h"
#include "utils.h"
#include "tags.h"

#define THUMBNAIL_DIMENSION 128

#define CHECKBOX_START_OFFSET_X (4.0f * PIXEL_SIZE)
#define CHECKBOX_START_OFFSET_Y (2.0f * PIXEL_SIZE)
#define CHECKBOX_INTERVAL (CHECKBOX_DIMENSION + 2.0f * PIXEL_SIZE)

#define CHECKBOX_THUMBNAIL_SPLIT 0.45f
#define THUMBNAILS_PER_ROW 4

#define SAME_TIME_ESPILON 0.00000001

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

enum GameMode_t{
     GAME_MODE_PLAYING,
     GAME_MODE_EDITOR,
     GAME_MODE_LEVEL_SELECT,
};

#define MAX_PLAYER_IN_BLOCK_RECT_RESULTS 16

struct PlayerInBlockRectResult_t{
     struct Entry_t{
          Block_t* block = nullptr;
          Position_t block_pos;
          S8 portal_rotations = 0;
     };

     StaticObjectArray_t<Entry_t, MAX_PLAYER_IN_BLOCK_RECT_RESULTS> entries;
};

struct DoBlockCollisionResults_t{
     bool repeat_collision_pass = false;
     S16 update_blocks_count;
};

struct FindMapResult_t{
     char* path = NULL;
     int map_number = 0;
};

struct FindAllMapsResult_t{
     FindMapResult_t* entries = NULL;
     U32 count = 0;
};

F32 get_collision_dt(CheckBlockCollisionResult_t* collision){
     F32 vel_mag = vec_magnitude(collision->original_vel);
     F32 pos_delta_mag = vec_magnitude(collision->pos_delta);

     // pi + vit = pf
     // pd = pf - pi
     // vit = (pf - pi)
     // vit = pd
     // t = pd / vi

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

         // put pairs together of collisions together that occur at the same time
         // eg. 3 hits 4 and 4 hits 3 because they move into each other
         for(S32 i = 0; i < count; i++){
              auto* collision = collisions + i;
              F32 collision_dt = get_collision_dt(collision);

              S32 swap_index = i + 1;
              for(S32 j = swap_index; j < count; j++){
                   auto* later_collision = collisions + j;
                   F32 later_collision_dt = get_collision_dt(later_collision);
                   if(fabs(collision_dt - later_collision_dt) > SAME_TIME_ESPILON) break;

                   if(collision->collided_block_index == later_collision->block_index &&
                      collision->block_index == later_collision->collided_block_index &&
                      collision->collided_dir_mask == direction_mask_opposite(later_collision->collided_dir_mask)){
                        CheckBlockCollisionResult_t tmp = collisions[swap_index];
                        collisions[swap_index] = *later_collision;
                        *later_collision = tmp;
                        collision->same_as_next = true;
                        break;
                   }
              }
         }

         // find pairs that share blocks, and put them together
         for(S32 i = 0; i < count; i++){
              auto* collision = collisions + i;
              if(collision->same_as_next == false) continue;
              F32 collision_dt = get_collision_dt(collision);

              auto* pair_collision = collisions + i + 1;

              // move any collisions at the same time with the block that we collided with up adjacent to us
              S32 swap_index = i + 2;
              for(S32 j = swap_index; j < count; j++){
                   auto* later_collision = collisions + j;
                   F32 later_collision_dt = get_collision_dt(later_collision);
                   if(fabs(collision_dt - later_collision_dt) <= SAME_TIME_ESPILON) break;

                   if(later_collision->same_as_next &&
                      ((collision->collided_block_index == later_collision->collided_block_index &&
                        collision->collided_dir_mask == later_collision->collided_dir_mask) ||
                       (collision->block_index == later_collision->block_index &&
                        collision->collided_dir_mask == later_collision->collided_dir_mask) ||
                       (pair_collision->collided_block_index == later_collision->collided_block_index &&
                        pair_collision->collided_dir_mask == later_collision->collided_dir_mask) ||
                       (pair_collision->block_index == later_collision->block_index &&
                        pair_collision->collided_dir_mask == later_collision->collided_dir_mask))){
                        CheckBlockCollisionResult_t tmp = collisions[swap_index];
                        collisions[swap_index] = *later_collision;
                        *later_collision = tmp;
                        swap_index++;

                        auto* later_pair_collision = collisions + j + 1;
                        tmp = collisions[swap_index];
                        collisions[swap_index] = *later_pair_collision;
                        *later_pair_collision = tmp;
                        swap_index++;

                        pair_collision->same_as_next = true;
                        break;
                   }
              }

              i++;
         }
     }
};

struct CentroidStart_t{
     Coord_t coord;
     Direction_t direction;
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
     LOG("testing demo %s: version %d with %" PRId64 " actions across %" PRId64 " frames\n", demo->filepath,
         demo->version, demo->entries.count, demo->last_frame);
     return true;
}

LogMapNumberResult_t load_map_number_map(S16 map_number, World_t* world, Undo_t* undo,
                                         Coord_t* player_start, PlayerAction_t* player_action,
                                         Camera_t* camera, bool* tags){
     auto result = load_map_number(map_number, player_start, world);
     if(result.success){
          reset_map(*player_start, world, undo, camera);
          *player_action = {};
          load_map_tags(result.filepath, tags);
          return result;
     }

     return result;
}

void reset_texture(GLuint* texture, Raw_t* raw){
    if(raw->bytes && raw->byte_count > 0){
        if(*texture > 0) glDeleteTextures(1, texture);
        *texture = transparent_texture_from_raw_bitmap(raw);
    }
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

PlayerInBlockRectResult_t player_in_block_rect(Player_t* player, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     PlayerInBlockRectResult_t result;

     auto player_pos = player->teleport ? player->teleport_pos + player->teleport_pos_delta : player->pos + player->pos_delta;

     auto player_coord = pos_to_coord(player_pos);
     Rect_t search_rect = rect_surrounding_adjacent_coords(player_coord);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

     quad_tree_find_in(block_qt, search_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     for(S16 b = 0; b < block_count; b++){
         auto block_pos = block_get_final_position(blocks[b]);
         auto block_rect = block_get_inclusive_rect(block_pos.pixel, block_get_cut(blocks[b]));
         if(pixel_in_rect(player->pos.pixel, block_rect)){
              PlayerInBlockRectResult_t::Entry_t entry;
              entry.block = blocks[b];
              entry.block_pos = block_pos;
              entry.portal_rotations = 0;
              result.entries.insert(&entry);
         }
     }

     auto found_blocks = find_blocks_through_portals(player_coord, tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;

         auto block_rect = block_get_inclusive_rect(found_block->position.pixel, found_block->rotated_cut);
         if(pixel_in_rect(player->pos.pixel, block_rect)){
              PlayerInBlockRectResult_t::Entry_t entry;
              entry.block = found_block->block;
              entry.block_pos = found_block->position;
              entry.portal_rotations = found_block->portal_rotations;
              result.entries.insert(&entry);
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
                    auto entangled_block_cut = block_get_cut(entangled_block);

                    S8 final_entangle_rotation = entangled_block->rotation - collision_result->collided_portal_rotations;
                    S8 total_rotations_between = direction_rotations_between((Direction_t)(block->rotation), (Direction_t)(final_entangle_rotation));

                    // the entangled block pos might be faked due to portals, so use the resulting collision pos instead of the actual position
                    auto entangled_block_collision_pos = collision_result->collided_pos;

                    // the result collided position is the center of the block, so handle this
                    entangled_block_collision_pos.pixel -= block_center_pixel_offset(entangled_block_cut);

                    Position_t block_pos = block_get_position(block);
                    Vec_t block_pos_delta = block->pre_collision_pos_delta;
                    auto block_cut = block_get_cut(block);
                    auto final_block_pos = block_pos + block_pos_delta;

                    Quad_t final_block_quad = {0, 0,
                                               (F32)block_get_width_in_pixels(block) * PIXEL_SIZE,
                                               (F32)block_get_height_in_pixels(block) * PIXEL_SIZE};

                    Vec_t final_block_center = {final_block_quad.right * 0.5f, final_block_quad.top * 0.5f};

                    auto entangled_block_offset_vec = pos_to_vec(entangled_block_collision_pos - final_block_pos);

                    S16 block_width_in_pixels = block_get_width_in_pixels(entangled_block);
                    S16 block_height_in_pixels = block_get_height_in_pixels(entangled_block);

                    if(total_rotations_between % 2 == 1){
                        S16 tmp = block_width_in_pixels;
                        block_width_in_pixels = block_height_in_pixels;
                        block_height_in_pixels = tmp;
                    }

                    Quad_t entangled_block_quad = {entangled_block_offset_vec.x,
                                                   entangled_block_offset_vec.y,
                                                   entangled_block_offset_vec.x + (F32)block_width_in_pixels * PIXEL_SIZE,
                                                   entangled_block_offset_vec.y + (F32)block_height_in_pixels * PIXEL_SIZE};

                    Vec_t entangled_block_center = {entangled_block_quad.left + (entangled_block_quad.right - entangled_block_quad.left) * 0.5f,
                                                    entangled_block_quad.bottom + (entangled_block_quad.top - entangled_block_quad.bottom) * 0.5f};

                    auto closest_final_vec = closest_vec_in_quad(final_block_center, entangled_block_quad);
                    auto closest_entangled_vec = closest_vec_in_quad(entangled_block_center, final_block_quad);

                    auto pos_diff = closest_entangled_vec - closest_final_vec;

                    // TODO: this 0.0001f is a hack, it used to be an equality check, but the
                    //       numbers were slightly off in the case of rotated portals but not rotated entangled blocks
                    auto pos_dimension_delta = fabs(fabs(pos_diff.x) - fabs(pos_diff.y));

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
                         Position_t entangled_block_pos = block_get_position(entangled_block);
                         Vec_t entangled_block_pos_delta = entangled_block->pre_collision_pos_delta;
                         auto entangle_inside_result = block_inside_others(entangled_block_pos, entangled_block_pos_delta, entangled_block_cut, get_block_index(world, entangled_block),
                                                                           entangled_block->clone_id > 0, world->block_qt, world->interactive_qt, &world->tilemap, &world->blocks);
                         if(entangle_inside_result.count > 0 && entangle_inside_result.objects[0].block == block){
                              // stop the blocks moving toward each other
                              static const VecMaskCollisionEntry_t table[] = {
                                   {static_cast<S8>(DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_LEFT  | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {static_cast<S8>(DIRECTION_MASK_LEFT  | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
                                   // TODO: single direction mask things
                              };

                              // TODO: figure out if we are colliding through a portal or not

                              auto delta_vec = pos_to_vec(block_pos - entangled_block_collision_pos);
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
                                   bool block_is_on_frictionless = block_on_frictionless(block_pos, block_pos_delta, block_cut,
                                                                                         &world->tilemap, world->interactive_qt, world->block_qt);

                                   bool entangled_block_is_on_frictionless = block_on_frictionless(entangled_block_pos, entangled_block_pos_delta, entangled_block_cut,
                                                                                                   &world->tilemap, world->interactive_qt, world->block_qt);

                                   if(block_is_on_frictionless && entangled_block_is_on_frictionless){
                                        // TODO: handle this case for blocks not entangled on ice
                                        // TODO: handle pushing blocks diagonally

                                        TransferMomentum_t block_momentum = get_block_momentum(world, block, move_dir_to_stop);
                                        TransferMomentum_t entangled_block_momentum = get_block_momentum(world, entangled_block, entangled_move_dir_to_stop);

                                        F32 dt_scale = 0;

                                        switch(move_dir_to_stop){
                                        default:
                                             break;
                                        case DIRECTION_LEFT:
                                        case DIRECTION_RIGHT:
                                             dt_scale = 1.0f - (collision_result->pos_delta.x / block->pos_delta.x);
                                             break;
                                        case DIRECTION_UP:
                                        case DIRECTION_DOWN:
                                             dt_scale = 1.0f - (collision_result->pos_delta.y / block->pos_delta.y);
                                             break;
                                        }

                                        if(block_push(block, entangled_move_dir_to_stop, world, true, 1.0f, &entangled_block_momentum).pushed){
                                             auto elastic_result = elastic_transfer_momentum_to_block(&entangled_block_momentum, world, block, entangled_move_dir_to_stop);

                                             switch(entangled_move_dir_to_stop){
                                             default:
                                                  break;
                                             case DIRECTION_LEFT:
                                             case DIRECTION_RIGHT:
                                                  block->pos_delta.x = elastic_result.second_final_velocity * dt * dt_scale;
                                                  break;
                                             case DIRECTION_UP:
                                             case DIRECTION_DOWN:
                                                  block->pos_delta.y = elastic_result.second_final_velocity * dt * dt_scale;
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
                                                  entangled_block->pos_delta.x = elastic_result.second_final_velocity * dt * dt_scale;
                                                  break;
                                             case DIRECTION_UP:
                                             case DIRECTION_DOWN:
                                                  entangled_block->pos_delta.y = elastic_result.second_final_velocity * dt * dt_scale;
                                                  break;
                                             }
                                        }
                                   }else{
                                        auto stop_entangled_dir = direction_rotate_clockwise(entangled_move_dir_to_stop, collision_result->collided_portal_rotations);

                                        stop_block_colliding_with_entangled(block, move_dir_to_stop);
                                        stop_block_colliding_with_entangled(entangled_block, stop_entangled_dir);

                                        add_global_tag(TAG_ENTANGLED_CENTROID_COLLISION);

                                        // TODO: compress this code, it's definitely used elsewhere
                                        for(S16 p = 0; p < world->players.count; p++){
                                             auto* player = world->players.elements + p;
                                             if(player->prev_pushing_block == block_index || player->prev_pushing_block == block->entangle_index){
                                                  player->push_time = 0.0f;
                                             }
                                        }
                                   }

                                   // we know this will be set because the entangled blocks are colliding with each other,
                                   // since we detected the centroid and resolved it here, don't do any special logic as
                                   // if the next collision is the same as this one
                                   collision_result->same_as_next = false;
                                   collision_result->unused = true;
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

void handle_blocks_colliding_moving_in_the_same_direction_horizontal(Block_t* block, Block_t* last_block_in_chain,
                                                                     S8 rotations_between_last_in_chain){
     if(block->vel.x == 0){
          block_stop_horizontally(block);
     }else{
          bool stopped_by_player = false;
          F32 accel = 0;
          if(rotations_between_last_in_chain % 2 == 0){
               stopped_by_player = last_block_in_chain->stopped_by_player_horizontal;
               accel = last_block_in_chain->accel.x;
          }else{
               stopped_by_player = last_block_in_chain->stopped_by_player_vertical;
               accel = last_block_in_chain->accel.y;
          }
          if(stopped_by_player){
               block->stopped_by_player_horizontal = stopped_by_player;
               block->horizontal_move.state = MOVE_STATE_STOPPING;
               block->accel.x = accel;
          }
     }
}

void handle_blocks_colliding_moving_in_the_same_direction_vertical(Block_t* block, Block_t* last_block_in_chain,
                                                                   S8 rotations_between_last_in_chain){
     if(block->vel.y == 0){
          block_stop_vertically(block);
     }else{
          bool stopped_by_player = false;
          F32 accel = 0;
          if(rotations_between_last_in_chain % 2 == 0){
               stopped_by_player = last_block_in_chain->stopped_by_player_vertical;
               accel = last_block_in_chain->accel.y;
          }else{
               stopped_by_player = last_block_in_chain->stopped_by_player_horizontal;
               accel = last_block_in_chain->accel.x;
          }
          if(stopped_by_player){
               block->stopped_by_player_horizontal = stopped_by_player;
               block->horizontal_move.state = MOVE_STATE_STOPPING;
               block->accel.y = accel;
          }
     }
}

void generate_pushes_from_collision(World_t* world, CheckBlockCollisionResult_t* collision_result, BlockPushes_t<128>* block_pushes){
     if(collision_result->unused) return;
     Block_t* block = world->blocks.elements + collision_result->block_index;
     Position_t block_pos = block_get_position(block);
     auto block_cut = block_get_cut(block);

     for(S16 i = 0; i < collision_result->collided_with_blocks.count; i++){
          auto* collided_with_block = collision_result->collided_with_blocks.objects + i;
          Block_t* collided_block = world->blocks.elements + collided_with_block->block_index;

          Vec_t block_pos_delta = block_get_pos_delta(block);
          Position_t collided_block_pos = block_get_position(collided_block);
          Vec_t collided_block_pos_delta = block_get_pos_delta(collided_block);
          auto collided_block_cut = block_get_cut(collided_block);

          bool block_is_on_frictionless = block_on_frictionless(block_pos, block_pos_delta, block_cut, &world->tilemap, world->interactive_qt, world->block_qt);
          bool collided_block_is_on_frictionless = block_on_frictionless(collided_block_pos, collided_block_pos_delta, collided_block_cut, &world->tilemap, world->interactive_qt, world->block_qt);

          if(!block_is_on_frictionless || !collided_block_is_on_frictionless) continue;

          S16 against_block_count = 0;
          bool should_push = true;

          if(direction_mask_is_single_direction(collided_with_block->direction_mask)){
               Direction_t direction = direction_from_single_mask(collision_result->collided_dir_mask);

               // if we are stopped or travelling the opposite direction of our collision, let the other
               if(direction == DIRECTION_LEFT){
                    if(block->pre_collision_pos_delta.x >= 0){
                         continue;
                    }
                    // to combat floating point precision error (collision_time_ratio is division, while pos_delta is
                    // usually subtraction), we check if the block was moving diagonally, then we know they'll just be
                    // reduced the same
                    if(block->pre_collision_pos_delta.x == block->pre_collision_pos_delta.y){
                         block_pos_delta.y = block->pos_delta.x;
                    }else{
                         block_pos_delta.y = block->pre_collision_pos_delta.y * block->collision_time_ratio.x;
                    }
               }else if(direction == DIRECTION_RIGHT){
                    if(block->pre_collision_pos_delta.x <= 0){
                         continue;
                    }
                    if(block->pre_collision_pos_delta.x == block->pre_collision_pos_delta.y){
                         block_pos_delta.y = block->pos_delta.x;
                    }else{
                         block_pos_delta.y = block->pre_collision_pos_delta.y * block->collision_time_ratio.x;
                    }
               }else if(direction == DIRECTION_DOWN){
                    if(block->pre_collision_pos_delta.y >= 0){
                         continue;
                    }
                    if(block->pre_collision_pos_delta.x == block->pre_collision_pos_delta.y){
                         block_pos_delta.x = block->pos_delta.y;
                    }else{
                         block_pos_delta.x = block->pre_collision_pos_delta.x * block->collision_time_ratio.y;
                    }
               }else if(direction == DIRECTION_UP){
                    if(block->pre_collision_pos_delta.y <= 0){
                         continue;
                    }
                    if(block->pre_collision_pos_delta.x == block->pre_collision_pos_delta.y){
                         block_pos_delta.x = block->pos_delta.y;
                    }else{
                         block_pos_delta.x = block->pre_collision_pos_delta.x * block->collision_time_ratio.y;
                    }
               }

               // TODO: it would be nice to check for this block specifically instead of doing a query again
               bool against_block = false;
               auto against_result = block_against_other_blocks(block_pos + block_pos_delta, block_cut, direction, world->block_qt, world->interactive_qt, &world->tilemap);
               bool all_on_frictionless = true;
               for(S16 a = 0; a < against_result.count; a++){
                    auto* against = against_result.objects + a;
                    if(against->block == collided_block){
                         against_block = true;
                    }

                    Position_t against_block_pos = block_get_position(against->block);
                    Vec_t against_block_pos_delta = block_get_pos_delta(against->block);
                    auto against_block_cut = block_get_cut(against->block);

                    all_on_frictionless &= block_on_frictionless(against_block_pos, against_block_pos_delta, against_block_cut, &world->tilemap, world->interactive_qt, world->block_qt);
               }

               if(!against_block){
                    continue;
               }

               if(all_on_frictionless){
                    against_block_count = against_result.count;
               }

               Block_t* last_block_in_chain = collided_block;
               Direction_t against_direction = direction;

               S8 rotations_between_last_in_chain = collided_with_block->portal_rotations;

               while(true){
                    // TODO: handle multiple against blocks
                    Position_t last_block_in_chain_final_pos = block_get_final_position(last_block_in_chain);
                    auto last_block_in_chain_cut = block_get_cut(last_block_in_chain);
                    auto chain_against_result = block_against_other_blocks(last_block_in_chain_final_pos, last_block_in_chain_cut,
                                                                     against_direction, world->block_qt, world->interactive_qt, &world->tilemap);
                    if(chain_against_result.count > 0){
                         last_block_in_chain = chain_against_result.objects[0].block;
                         against_direction = direction_rotate_clockwise(against_direction, chain_against_result.objects[0].rotations_through_portal);
                         rotations_between_last_in_chain += chain_against_result.objects[0].rotations_through_portal;
                    }else{
                         break;
                    }
               }

               Position_t last_in_chain_pos = block_get_position(last_block_in_chain);
               Vec_t last_in_chain_pos_delta = block_get_pos_delta(last_block_in_chain);
               auto last_in_chain_cut = block_get_cut(last_block_in_chain);

               bool last_in_chain_on_frictionless = block_on_frictionless(last_in_chain_pos, last_in_chain_pos_delta,
                                                                          last_in_chain_cut, &world->tilemap, world->interactive_qt, world->block_qt);

               bool being_stopped_by_player = direction_is_horizontal(direction) ?
                                              last_block_in_chain->stopped_by_player_horizontal :
                                              last_block_in_chain->stopped_by_player_vertical;

               if(being_stopped_by_player){
                   being_stopped_by_player &= block_against_player(last_block_in_chain, against_direction, &world->players) != NULL;
               }


               // if the blocks are headed in the same direction but the block is slowing down for either friction or
               // being stop stopped by the player, slow down with it
               // TODO: handle the case where a block with a lot of momentum collides with a block being stopped by the player
               if(!last_in_chain_on_frictionless || being_stopped_by_player){
                    // TODO: rotated collided block vel based on rotations in chain
                    switch(direction){
                    default:
                        break;
                    case DIRECTION_LEFT:
                    {
                         if(block->vel.x < 0 &&
                            collided_block->vel.x < 0 &&
                            block->vel.x < collided_block->vel.x){
                              block->vel.x = collided_block->vel.x;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    } break;
                    case DIRECTION_RIGHT:
                    {
                         if(block->vel.x > 0 &&
                            collided_block->vel.x > 0 &&
                            block->vel.x > collided_block->vel.x){
                              block->vel.x = collided_block->vel.x;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    } break;
                    case DIRECTION_DOWN:
                    {
                         if(block->vel.y < 0 &&
                            collided_block->vel.y < 0 &&
                            block->vel.y < collided_block->vel.y){
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    } break;
                    case DIRECTION_UP:
                    {
                         if(block->vel.y > 0 &&
                            collided_block->vel.y > 0 &&
                            block->vel.y > collided_block->vel.y){
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    } break;
                    }
               }
          }else{
               auto against = block_diagonally_against_block(block->pos + block->pos_delta, block_cut, collided_with_block->direction_mask, &world->tilemap,
                                                             world->interactive_qt, world->block_qt);

               if(against.block != collided_block) continue;

               against_block_count = 1;

               Block_t* last_block_in_chain = collided_block;
               DirectionMask_t against_direction_mask = collided_with_block->direction_mask;

               S8 rotations_between_last_in_chain = collided_with_block->portal_rotations;

               while(true){
                    // TODO: handle multiple against blocks
                    Position_t last_block_in_chain_final_pos = block_get_final_position(last_block_in_chain);
                    auto last_block_in_chain_cut = block_get_cut(last_block_in_chain);
                    against = block_diagonally_against_block(last_block_in_chain_final_pos, last_block_in_chain_cut,
                                                             against_direction_mask, &world->tilemap, world->interactive_qt, world->block_qt);
                    if(against.block){
                         last_block_in_chain = against.block;
                         against_direction_mask = direction_mask_rotate_clockwise(against_direction_mask, against.rotations_through_portal);
                         rotations_between_last_in_chain += against.rotations_through_portal;
                    }else{
                         break;
                    }
               }

               Position_t last_block_in_chain_pos = block_get_position(last_block_in_chain);
               Vec_t last_block_in_chain_pos_delta = block_get_pos_delta(last_block_in_chain);
               BlockCut_t last_block_in_chain_cut = block_get_cut(last_block_in_chain);

               bool last_in_chain_on_frictionless = block_on_frictionless(last_block_in_chain_pos, last_block_in_chain_pos_delta,
                                                                          last_block_in_chain_cut, &world->tilemap, world->interactive_qt, world->block_qt);

               // if the blocks are headed in the same direction but the block is slowing down for either friction or
               // being stop stopped by the player, slow down with it
               // TODO: handle the case where a block with a lot of momentum collides with a block being stopped by the player
               if(!last_in_chain_on_frictionless){
                    // TODO: rotate collided block vel

                    if(collided_with_block->direction_mask == (DIRECTION_MASK_LEFT | DIRECTION_MASK_DOWN)){
                         if(block->vel.x < 0 && block->vel.y < 0 &&
                            collided_block->vel.x < 0 && collided_block->vel.y < 0 &&
                            block->vel.x < collided_block->vel.x && block->vel.y < collided_block->vel.y){
                              block->vel.x = collided_block->vel.x;
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    }else if(collided_with_block->direction_mask == (DIRECTION_MASK_LEFT | DIRECTION_MASK_UP)){
                         if(block->vel.x < 0 && block->vel.y > 0 &&
                            collided_block->vel.x < 0 && collided_block->vel.y > 0 &&
                            block->vel.x < collided_block->vel.x && block->vel.y > collided_block->vel.y){
                              block->vel.x = collided_block->vel.x;
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    }else if(collided_with_block->direction_mask == (DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN)){
                         if(block->vel.x > 0 && block->vel.y < 0 &&
                            collided_block->vel.x > 0 && collided_block->vel.y < 0 &&
                            block->vel.x > collided_block->vel.x && block->vel.y < collided_block->vel.y){
                              block->vel.x = collided_block->vel.x;
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    }else if(collided_with_block->direction_mask == (DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP)){
                         if(block->vel.x > 0 && block->vel.y > 0 &&
                            collided_block->vel.x > 0 && collided_block->vel.y > 0 &&
                            block->vel.x > collided_block->vel.x && block->vel.y > collided_block->vel.y){
                              block->vel.x = collided_block->vel.x;
                              block->vel.y = collided_block->vel.y;
                              handle_blocks_colliding_moving_in_the_same_direction_horizontal(block, last_block_in_chain, rotations_between_last_in_chain);
                              handle_blocks_colliding_moving_in_the_same_direction_vertical(block, last_block_in_chain, rotations_between_last_in_chain);
                              should_push = false;
                         }
                    }
               }
          }

          if(!should_push || against_block_count == 0) continue;

          auto rotated_direction_mask = direction_mask_rotate_clockwise(collided_with_block->direction_mask, collided_with_block->portal_rotations);
          auto opposite_direction_mask = direction_mask_opposite(rotated_direction_mask);
          char o[256];
          direction_mask_to_string(opposite_direction_mask, o, 256);

          // if we already created a push (for the alternate block in the alternate direction), do not create this push
          bool found_pair_push = false;
          for(S16 p = 0; p < block_pushes->count; p++){
               BlockPush_t* push = block_pushes->pushes + p;
               if(push->pushee_index == collision_result->block_index &&
                  push->pushers[0].index == collided_with_block->block_index &&
                  push->direction_mask == opposite_direction_mask){
                    found_pair_push = true;
               }
          }

          if(!found_pair_push){
               BlockPush_t block_push {};

               block_push.add_pusher(collision_result->block_index, against_block_count);
               block_push.pushee_index = collided_with_block->block_index;
               block_push.direction_mask = collided_with_block->direction_mask;
               block_push.portal_rotations = collided_with_block->portal_rotations;

               char m[256];
               direction_mask_to_string(block_push.direction_mask, m, 256);

               block_pushes->add(&block_push);
          }
     }
}

S16 get_boundary_from_coord(Coord_t coord, Direction_t direction){
     Pixel_t pixel = coord_to_pixel(coord);

     // the values are opposite of what you would expect because this is about colliding from the direction specified
     switch(direction){
     default:
          break;
     case DIRECTION_RIGHT:
          return pixel.x;
     case DIRECTION_LEFT:
          return pixel.x + TILE_SIZE_IN_PIXELS;
     case DIRECTION_UP:
          return pixel.y;
     case DIRECTION_DOWN:
          return pixel.y + TILE_SIZE_IN_PIXELS;
     }

     return -1;
}

Pixel_t block_pos_in_solid_boundary(Position_t pos, BlockCut_t cut, Direction_t horizontal_direction, Direction_t vertical_direction, World_t* world){
     Coord_t bottom_left_coord = pixel_to_coord(pos.pixel);
     Coord_t bottom_right_coord = pixel_to_coord(block_bottom_right_pixel(pos.pixel, cut));
     Coord_t top_left_coord = pixel_to_coord(block_top_left_pixel(pos.pixel, cut));
     Coord_t top_right_coord = pixel_to_coord(block_top_right_pixel(pos.pixel, cut));

     if(tilemap_is_solid(&world->tilemap, bottom_left_coord)){
          return Pixel_t{get_boundary_from_coord(bottom_left_coord, horizontal_direction), get_boundary_from_coord(bottom_left_coord, vertical_direction)};
     }

     if(tilemap_is_solid(&world->tilemap, bottom_right_coord)){
          return Pixel_t{get_boundary_from_coord(bottom_right_coord, horizontal_direction), get_boundary_from_coord(bottom_right_coord, vertical_direction)};
     }

     if(tilemap_is_solid(&world->tilemap, top_left_coord)){
          return Pixel_t{get_boundary_from_coord(top_left_coord, horizontal_direction), get_boundary_from_coord(top_left_coord, vertical_direction)};
     }

     if(tilemap_is_solid(&world->tilemap, top_right_coord)){
          return Pixel_t{get_boundary_from_coord(top_right_coord, horizontal_direction), get_boundary_from_coord(top_right_coord, vertical_direction)};
     }

     Interactive_t* interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, bottom_left_coord, pos.z);
     if(interactive){
          return Pixel_t{get_boundary_from_coord(bottom_left_coord, horizontal_direction), get_boundary_from_coord(bottom_left_coord, vertical_direction)};
     }

     interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, bottom_right_coord, pos.z);
     if(interactive){
          return Pixel_t{get_boundary_from_coord(bottom_right_coord, horizontal_direction), get_boundary_from_coord(bottom_right_coord, vertical_direction)};
     }

     interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, top_left_coord, pos.z);
     if(interactive){
          return Pixel_t{get_boundary_from_coord(top_left_coord, horizontal_direction), get_boundary_from_coord(top_left_coord, vertical_direction)};
     }

     interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, top_right_coord, pos.z);
     if(interactive){
          return Pixel_t{get_boundary_from_coord(top_right_coord, horizontal_direction), get_boundary_from_coord(top_right_coord, vertical_direction)};
     }

     return Pixel_t {-1, -1};
}


DoBlockCollisionResults_t do_block_collision(World_t* world, Block_t* block, S16 update_blocks_count){
     DoBlockCollisionResults_t result;
     result.update_blocks_count = update_blocks_count;

     S16 block_index = get_block_index(world, block);

     StopOnBoundary_t stop_on_boundary_x = DO_NOT_STOP_ON_BOUNDARY;
     StopOnBoundary_t stop_on_boundary_y = DO_NOT_STOP_ON_BOUNDARY;

     // if we are being stopped by the player and have moved more than the player radius (which is
     // a check to ensure we don't stop a block instantaneously) then stop on the coordinate boundaries
     if(block->stopped_by_player_horizontal && block->vel.x != 0 && block->horizontal_move.state != MOVE_STATE_STARTING){
          stop_on_boundary_x = STOP_ON_BOUNDARY_TRACKING_START;
     }

     if(block->stopped_by_player_vertical && block->vel.y != 0 && block->vertical_move.state != MOVE_STATE_STARTING){
          stop_on_boundary_y = STOP_ON_BOUNDARY_TRACKING_START;
     }

     // TODO: we need to maintain a list of these blocks pushed i guess in the future
     Block_t* block_pushed = nullptr;
     for(S16 p = 0; p < world->players.count; p++){
          Player_t* player = world->players.elements + p;
          if(player->prev_pushing_block >= 0){
               block_pushed = world->blocks.elements + player->prev_pushing_block;
          }
     }

     Position_t block_pos = block_get_position(block);
     Vec_t block_pos_delta = block_get_pos_delta(block);
     BlockCut_t block_cut = block_get_cut(block);

     // this instance of last_block_pushed is to keep the pushing smooth and not have it stop at the tile boundaries
     if(block != block_pushed &&
        !block_on_ice(block_pos, block_pos_delta, block_cut, &world->tilemap, world->interactive_qt, world->block_qt) &&
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

               block_stop_horizontally(block);
               block->stop_on_pixel_x = boundary_x;
               block->coast_horizontal = BLOCK_COAST_NONE;

               // figure out new pos_delta which will be used for collision in the next iteration
               auto delta_pos = pixel_to_pos(Pixel_t{boundary_x, 0}) - block->pos;
               block->pos_delta.x = pos_to_vec(delta_pos).x;
          }
     }

     if(block->pos_delta.x != 0.0f){
          S16 boundary_x = range_passes_solid_boundary(block->pos.pixel.x, final_pos.pixel.x, block_cut,
                                                       true, block->pos.pixel.y, final_pos.pixel.y, block->pos.z,
                                                       &world->tilemap, world->interactive_qt);
          if(boundary_x){
               result.repeat_collision_pass = true;

               block_stop_horizontally(block);
               block->stop_on_pixel_x = boundary_x;
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

               block_stop_vertically(block);
               block->stop_on_pixel_y = boundary_y;
               block->coast_vertical = BLOCK_COAST_NONE;

               // figure out new pos_delta which will be used for collision in the next iteration
               auto delta_pos = pixel_to_pos(Pixel_t{0, boundary_y}) - block->pos;
               block->pos_delta.y = pos_to_vec(delta_pos).y;
          }
     }

     if(block->pos_delta.y != 0.0f){
          S16 boundary_y = range_passes_solid_boundary(block->pos.pixel.y, final_pos.pixel.y, block_cut,
                                                       false, block->pos.pixel.x, final_pos.pixel.x, block->pos.z,
                                                       &world->tilemap, world->interactive_qt);
          if(boundary_y){
               result.repeat_collision_pass = true;

               block_stop_vertically(block);
               block->stop_on_pixel_y = boundary_y;
               block->coast_vertical = BLOCK_COAST_NONE;

               // figure out new pos_delta which will be used for collision in the next iteration
               auto delta_pos = pixel_to_pos(Pixel_t{0, boundary_y}) - block->pos;
               block->pos_delta.y = pos_to_vec(delta_pos).y;
          }
     }

     // if we haven't stopped on any boundaries, check if we should diagonally stop on boundaries
     if(block->pos_delta.y != 0.0f && block->stop_on_pixel_y == 0 &&
        block->pos_delta.x != 0.0f && block->stop_on_pixel_x == 0){
          Direction_t horizontal_dir = (block->pos_delta.x > 0) ? DIRECTION_RIGHT : DIRECTION_LEFT;
          Direction_t vertical_dir = (block->pos_delta.y > 0) ? DIRECTION_UP : DIRECTION_DOWN;

          Pixel_t pre_move_stop_on_boundaries = block_pos_in_solid_boundary(block->pos, block_cut, horizontal_dir, vertical_dir, world);
          Pixel_t post_move_stop_on_boundaries = block_pos_in_solid_boundary(final_pos, block_cut, horizontal_dir, vertical_dir, world);

          if(pre_move_stop_on_boundaries == Pixel_t{-1, -1} && post_move_stop_on_boundaries != Pixel_t{-1, -1}){
               if(post_move_stop_on_boundaries.x >= 0){
                    result.repeat_collision_pass = true;

                    block_stop_horizontally(block);
                    block->stop_on_pixel_x = post_move_stop_on_boundaries.x;
                    block->coast_horizontal = BLOCK_COAST_NONE;

                    // figure out new pos_delta which will be used for collision in the next iteration
                    auto delta_pos = pixel_to_pos(Pixel_t{post_move_stop_on_boundaries.x, 0}) - block->pos;
                    block->pos_delta.x = pos_to_vec(delta_pos).x;
               }

               if(post_move_stop_on_boundaries.y >= 0){
                    result.repeat_collision_pass = true;

                    block_stop_vertically(block);
                    block->stop_on_pixel_y = post_move_stop_on_boundaries.y;
                    block->coast_vertical = BLOCK_COAST_NONE;

                    // figure out new pos_delta which will be used for collision in the next iteration
                    auto delta_pos = pixel_to_pos(Pixel_t{0, post_move_stop_on_boundaries.y}) - block->pos;
                    block->pos_delta.y = pos_to_vec(delta_pos).y;
               }
          }
     }

     StaticObjectArray_t<S16, ARROW_ARRAY_MAX> stuck_arrows;

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

                         for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
                              Arrow_t* arrow = world->arrows.arrows + a;
                              if(!arrow->alive) continue;
                              if(arrow->stuck_type == STUCK_BLOCK && arrow->stuck_index == block_index){
                                   stuck_arrows.insert(&a);
                              }
                         }
                    }else{
                         S16 new_block_index = world->blocks.count;
                         S16 old_block_index = (S16)(block - world->blocks.elements);

                         if(resize(&world->blocks, world->blocks.count + (S16)(1))){
                              add_global_tag(TAG_BLOCK_GETS_ENTANGLED);

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
                                   add_global_tag(TAG_THREE_PLUS_BLOCKS_ENTANGLED);
                                   entangled_block->entangle_index = block->entangle_index;
                              }

                              // entangle any arrows stuck in the original block
                              for(S16 a = 0; a < stuck_arrows.count; a++){
                                   Arrow_t* arrow = world->arrows.arrows + stuck_arrows.objects[a];

                                   Arrow_t* spawned_arrow = arrow_spawn(&world->arrows, arrow->pos, arrow->face);

                                   // TODO: compress this code with other entangled code
                                   spawned_arrow->element = arrow->element;
                                   spawned_arrow->vel = arrow->vel;
                                   spawned_arrow->fall_time = arrow->fall_time;
                                   // TODO: I'm not sure anything bad can happen, but we may need to make a full chain of entangles,
                                   //       rather than all entangling back to the original
                                   spawned_arrow->entangle_index = stuck_arrows.objects[a];
                                   arrow->entangle_index = spawned_arrow - world->arrows.arrows;
                                   // spawned_arrow->spawned_this_frame = true;
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
               auto* src_portal = quad_tree_find_at(world->interactive_qt, block->clone_start.x, block->clone_start.y);
               if(is_active_portal(src_portal)){
                    activate(world, block->clone_start);
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

bool block_pushes_are_the_same_collision(BlockPushes_t<128>* block_pushes, S16 start_index, S16 end_index, S16 block_index){
     if(start_index < 0 || start_index > block_pushes->count) return false;
     if(end_index < 0 || end_index > block_pushes->count) return false;
     if(start_index > end_index) return false;

     auto& start_push = block_pushes->pushes[start_index];
     auto& end_push = block_pushes->pushes[end_index];

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

void add_entangle_pushes_for_end_of_chain_blocks_on_ice(World_t* world, S16 push_index, BlockPushes_t<128>* block_pushes, S16 pusher_rotations = 0){
     BlockPush_t* push = block_pushes->pushes + push_index;

     Block_t* pushee = world->blocks.elements + push->pushee_index;

     Position_t pushee_pos = block_get_position(pushee);
     Vec_t pushee_pos_delta = block_get_pos_delta(pushee);
     BlockCut_t pushee_cut = block_get_cut(pushee);

     if(!block_on_frictionless(pushee_pos, pushee_pos_delta, pushee_cut, &world->tilemap,
                               world->interactive_qt, world->block_qt)) return;

     S8 push_rotations = (push->entangle_rotations + push->portal_rotations) % DIRECTION_COUNT;
     DirectionMask_t rotated_direction_mask = direction_mask_rotate_clockwise(push->direction_mask, push_rotations);

     for(S16 d = 0; d < DIRECTION_COUNT; d++){
          Direction_t direction = static_cast<Direction_t>(d);
          if(!direction_in_mask(rotated_direction_mask, direction)) continue;

          auto chain_result = find_block_chain(pushee, direction, world->block_qt, world->interactive_qt, &world->tilemap);

          if(chain_result.count > 0){
               push->no_entangled_pushes = true;
          }

          auto against_result = block_against_other_blocks(pushee_pos + pushee_pos_delta, pushee_cut,
                                                                  direction, world->block_qt, world->interactive_qt,
                                                                  &world->tilemap);

          S16 added_indices[MAX_BLOCKS_IN_CHAIN];
          S16 added_indices_count = 0;

          for(S16 c = 0; c < chain_result.count; c++){
               BlockChain_t* chain = chain_result.objects + c;
               if(chain->count <= 0) continue;

               Block_t* end_block = chain->objects[chain->count - 1].block;
               S16 end_index = get_block_index(world, end_block);

               if(end_block->entangle_index < 0) continue;

               Position_t against_pos = block_get_position(end_block);
               Vec_t against_pos_delta = block_get_pos_delta(end_block);

               // TODO: what do we set the force value to here ?
               if(!block_pushable(end_block, direction, world, 1.0f)) continue;
               if(!block_on_frictionless(against_pos, against_pos_delta, end_block->cut, &world->tilemap,
                                         world->interactive_qt, world->block_qt)) continue;

               // walk backwards from the end of the chain, find blocks that are entangled with this one and
               // add a push for them. we do this because the pushes need to happen in a certain order.
               for(S16 e = chain->count - 1; e >= 0; e--){
                    Block_t* chain_block = chain->objects[e].block;
                    if(!blocks_are_entangled(chain_block, end_block, &world->blocks)) continue;

                    BlockPush_t new_block_push = *push;
                    S8 rotations_between_blocks = blocks_rotations_between(chain_block, end_block);
                    new_block_push.direction_mask = direction_to_direction_mask(direction);
                    new_block_push.pushee_index = get_block_index(world, chain_block);
                    new_block_push.portal_rotations = push->portal_rotations;
                    new_block_push.entangle_rotations = rotations_between_blocks;
                    new_block_push.pusher_rotations = pusher_rotations;
                    new_block_push.entangled_with_push_index = push_index;
                    new_block_push.no_consolidate = true;
                    new_block_push.no_entangled_pushes = false;

                    if(against_result.count > 1){
                         for(S16 p = 0; p < new_block_push.pusher_count; p++){
                              new_block_push.pushers[p].collided_with_block_count = against_result.count;
                              new_block_push.pushers[p].hit_entangler = true;
                         }
                    }else{
                         for(S16 p = 0; p < new_block_push.pusher_count; p++){
                              new_block_push.pushers[p].hit_entangler = true;
                         }
                    }

                    added_indices[added_indices_count] = new_block_push.pushee_index;
                    added_indices_count++;
                    assert(added_indices_count <= MAX_BLOCKS_IN_CHAIN);

                    block_pushes->add(&new_block_push);
               }

               // add pushes for the rest of the entangled blocks we haven't yet added pushes for
               S16 current_entangle_index = end_block->entangle_index;
               while(current_entangle_index != end_index && current_entangle_index >= 0){
                    Block_t* entangler = world->blocks.elements + current_entangle_index;

                    bool already_added = false;
                    for(S16 a = 0; a < added_indices_count; a++){
                         if(current_entangle_index == added_indices[a]){
                              already_added = true;
                              break;
                         }
                    }

                    if(!already_added){
                         BlockPush_t new_block_push = *push;
                         S8 rotations_between_blocks = blocks_rotations_between(entangler, end_block);
                         new_block_push.direction_mask = direction_to_direction_mask(direction);
                         new_block_push.pushee_index = current_entangle_index;
                         new_block_push.portal_rotations = push->portal_rotations;
                         new_block_push.entangle_rotations = rotations_between_blocks;
                         new_block_push.pusher_rotations = pusher_rotations;
                         new_block_push.entangled_with_push_index = push_index;
                         new_block_push.no_consolidate = true;
                         new_block_push.no_entangled_pushes = false;

                         if(against_result.count > 1){
                              for(S16 p = 0; p < new_block_push.pusher_count; p++){
                                   new_block_push.pushers[p].collided_with_block_count = against_result.count;
                                   new_block_push.pushers[p].hit_entangler = true;
                              }
                         }else{
                              for(S16 p = 0; p < new_block_push.pusher_count; p++){
                                   new_block_push.pushers[p].hit_entangler = true;
                              }
                         }

                         block_pushes->add(&new_block_push);
                    }

                    current_entangle_index = entangler->entangle_index;
               }
          }
     }
}

void consolidate_block_pushes(BlockPushes_t<128>* block_pushes, BlockPushes_t<128>* consolidated_block_pushes){
     for(S16 i = 0; i < block_pushes->count; i++){
          auto* push = block_pushes->pushes + i;
          if(push->invalidated) continue;
          if(push->no_consolidate){
               consolidated_block_pushes->add(push);
               continue;
          }

          assert(push->pusher_count == 1);

          S8 rotations = (push->entangle_rotations + push->portal_rotations) % DIRECTION_COUNT;
          auto rot_direction_mask = direction_mask_rotate_clockwise(push->direction_mask, rotations);

          // consolidate pushes if we can
          bool consolidated_current_push = false;
          for(S16 j = 0; j < consolidated_block_pushes->count; j++){
               auto* consolidated_push = consolidated_block_pushes->pushes + j;

               if(consolidated_push->no_consolidate) continue;

               if(push->pushee_index == consolidated_push->pushee_index &&
                  rot_direction_mask == consolidated_push->direction_mask){
                    for(S16 p = 0; p < push->pusher_count; p++){
                         consolidated_push->add_pusher(push->pushers[p].index, push->pushers[p].collided_with_block_count,
                                                       push->is_entangled(), push->pushers[p].entangle_rotations, push->pushers[p].portal_rotations,
                                                       push->pushers[p].opposite_entangle_reversed);
                    }
                    consolidated_current_push = true;
               }
          }

          // otherwise just add it
          if(!consolidated_current_push) consolidated_block_pushes->add(push);
     }
}

void execute_block_pushes(BlockPushes_t<128>* block_pushes, World_t* world, BlockMomentumChanges_t* momentum_changes){
     S16 simultaneous_block_pushes = 0;

     for(S16 i = 0; i < block_pushes->count; i++){
          auto& block_push = block_pushes->pushes[i];
          if(block_push.invalidated) continue;

          auto result = block_collision_push(&block_push, world);

          // any momentum changes that stops or changes a blocks momentum, means that block could not have pushed anything, so invalidate it's pushes
          for(S16 j = i + 1; j < block_pushes->count; j++){
              auto& check_block_push = block_pushes->pushes[j];

              // if the entangled push has already been executed, then we can't invalidate it
              if(check_block_push.is_entangled()){
                  if(i <= check_block_push.entangled_with_push_index) continue;
              }

              for(S16 m = 0; m < result.momentum_changes.count; m++){
                  auto& block_change = result.momentum_changes.objects[m];

                  for(S16 p = 0; p < check_block_push.pusher_count; p++){
                      auto& check_pusher = check_block_push.pushers[p];

                      if(check_pusher.index != block_change.block_index) continue;
                      if(check_pusher.hit_entangler) continue;
                      if(block_pushes_are_the_same_collision(block_pushes, i, j, check_pusher.index)) continue;

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

          momentum_changes->merge(&result.momentum_changes);

          if(result.additional_block_pushes.count){
              block_pushes->merge(&result.additional_block_pushes);
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
}

void apply_momentum_changes(BlockMomentumChanges_t* momentum_changes, World_t* world){
     for(S16 i = 0; i < world->blocks.count; i++){
          auto* block = world->blocks.elements + i;
          auto block_mass = get_block_stack_mass(world, block);

          S16 x_changes = 0;
          S16 y_changes = 0;
          Vec_t new_vel = vec_zero();

          // clear momentum for each impacted block
          for(S16 c = 0; c < momentum_changes->count; c++){
               auto& block_change = momentum_changes->objects[c];
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
     for(S16 c = 0; c < momentum_changes->count; c++){
          auto& block_change = momentum_changes->objects[c];
          auto* block = world->blocks.elements + block_change.block_index;
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
    if(block_pushes.count > 0){
        LOG("block pushes: %d\n", block_pushes.count);
    }
    for(S16 i = 0; i < block_pushes.count; i++){
        log_block_push(block_pushes.pushes + i);
    }
}

void log_momentum_changes(BlockMomentumChanges_t* momentum_changes){
     if(momentum_changes->count > 0){
          LOG("momentum changes: %d\n", momentum_changes->count);
     }
     for(S16 c = 0; c < momentum_changes->count; c++){
          auto& block_change = momentum_changes->objects[c];
          LOG("  block %d m: %d, v: %f in %s\n", block_change.block_index, block_change.mass, block_change.vel, block_change.x ? "x" : "y");
     }
}

void log_collision_result(CheckBlockCollisionResult_t* collision, World_t* world){
     Block_t* block = world->blocks.elements + collision->block_index;
     LOG("  col: %d, block: %d, collided with blocks: %d, reduced pd: %.10f, %.10f to %.10f, %.10f\n",
         collision->collided, collision->block_index, collision->collided_with_blocks.count,
         block->pos_delta.x, block->pos_delta.y, collision->pos_delta.x, collision->pos_delta.y);
     for(S16 i = 0; i < collision->collided_with_blocks.count; i++){
          auto* collided_with = collision->collided_with_blocks.objects + i;
          char m[256];
          direction_mask_to_string(collided_with->direction_mask, m, 256);
          LOG("    index: %d, directions: %s, portal rot: %d\n", collided_with->block_index, m, collided_with->portal_rotations);
     }
}

void restart_demo(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                  ObjectArray_t<Interactive_t>* demo_starting_interactives, Demo_t* demo, S64* frame_count,
                  Coord_t* player_start, PlayerAction_t* player_action, Undo_t* undo, Camera_t* camera){
     fetch_cache_for_demo_seek(world, demo_starting_tilemap, demo_starting_blocks, demo_starting_interactives);

     reset_map(*player_start, world, undo, camera);

     // reset some vars
     *player_action = {};

     demo->entry_index = 0;
     *frame_count = 0;
}

void set_against_blocks_coasting_from_player(Block_t* block, Direction_t direction, World_t* world){
     auto block_pos = block_get_final_position(block);

     auto against_result = block_against_other_blocks(block_pos,
                                                      block_get_cut(block),
                                                      direction, world->block_qt, world->interactive_qt, &world->tilemap);

     for(S16 a = 0; a < against_result.count; a++){
         auto* against_other = against_result.objects + a;
         Direction_t against_direction = direction_rotate_clockwise(direction, against_other->rotations_through_portal);

         if(direction_is_horizontal(against_direction)){
             against_other->block->coast_horizontal = BLOCK_COAST_PLAYER;
         }else{
             against_other->block->coast_vertical = BLOCK_COAST_PLAYER;
         }

         set_against_blocks_coasting_from_player(against_other->block, against_direction, world);
     }
}

Raw_t create_thumbnail_bitmap(){
    Raw_t raw;

    U32 pixel_size = THUMBNAIL_DIMENSION * THUMBNAIL_DIMENSION * 3;
    raw.byte_count = sizeof(BitmapFileHeader_t) + sizeof(BitmapInfoHeader_t) + pixel_size;
    U32 pixel_offset = sizeof(BitmapFileHeader_t) + sizeof(BitmapInfoHeader_t);

    raw.bytes = (U8*)malloc(raw.byte_count);
    if(!raw.bytes) return raw;

    BitmapFileHeader_t* file_header = (BitmapFileHeader_t*)(raw.bytes);
    BitmapInfoHeader_t* info_header = (BitmapInfoHeader_t*)(raw.bytes + sizeof(BitmapFileHeader_t));

    file_header->file_type[0] = 'B';
    file_header->file_type[1] = 'M';
    file_header->file_size = pixel_size + pixel_offset;
    file_header->bitmap_offset = pixel_offset;

    info_header->size = BITMAP_SUPPORTED_SIZE;
    info_header->width = THUMBNAIL_DIMENSION;
    info_header->height = THUMBNAIL_DIMENSION;
    info_header->planes = 1;
    info_header->bit_count = 24;
    info_header->compression = 0;
    info_header->size_image = pixel_size;
    info_header->x_pels_per_meter = 2835;
    info_header->y_pels_per_meter = 2835;
    info_header->clr_used = 0;
    info_header->clr_important = 0;

    BitmapPixel_t* pixel = (BitmapPixel_t*)(raw.bytes + pixel_offset);

    for(U32 y = 0; y < THUMBNAIL_DIMENSION; y++){
        for(U32 x = 0; x < THUMBNAIL_DIMENSION; x++){
            glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

            // swap red and blue
            U8 tmp = pixel->red;
            pixel->red = pixel->blue;
            pixel->blue = tmp;

            pixel++;
        }
    }

    return raw;
}

int get_numbered_map(const char* path){
    char number_str[4];
    memset(number_str, 0, 4);

    U32 digits_left = 3;
    while(*path){
         if(digits_left > 0){
              if(!isdigit(*path)) return -1;
              number_str[3 - digits_left] = *path;
              digits_left--;
         }else{
              if(*path == '_') break;
              return -1;
         }
         path++;
    }

    return atoi(number_str);
}

FindAllMapsResult_t find_all_maps(){
     FindAllMapsResult_t result;
     U32 map_count = 0;

     {
          DIR* d = opendir("content");
          if(!d) return result;
          struct dirent* dir;
          while((dir = readdir(d)) != nullptr){
               if(strstr(dir->d_name, ".bm") && get_numbered_map(dir->d_name) >= 0){
                    map_count++;
               }
          }
          closedir(d);
     }

     result.count = map_count;
     result.entries = (FindMapResult_t*)(malloc(map_count * sizeof(*result.entries)));

     map_count = 0;
     {
          DIR* d = opendir("content");
          if(!d) return result;
          struct dirent* dir;
          char full_path[256];
          while((dir = readdir(d)) != nullptr){
               int map_number = get_numbered_map(dir->d_name);
               if(strstr(dir->d_name, ".bm") && map_number >= 0){
                    snprintf(full_path, 256, "content/%s", dir->d_name);
                    result.entries[map_count].path = strdup(full_path);
                    result.entries[map_count].map_number = map_number;
                    map_count++;
               }
          }
          closedir(d);
     }

     return result;
}

int map_thumbnail_comparor(const void* a, const void* b){
     MapThumbnail_t* thumbnail_a = (MapThumbnail_t*)a;
     MapThumbnail_t* thumbnail_b = (MapThumbnail_t*)b;

     return thumbnail_a->map_number > thumbnail_b->map_number;
}

S16 filter_thumbnails(ObjectArray_t<Checkbox_t>* tag_checkboxes, ObjectArray_t<MapThumbnail_t>* map_thumbnails){
     const F32 thumbnail_row_start_x = CHECKBOX_THUMBNAIL_SPLIT;
     F32 current_thumbnail_x = thumbnail_row_start_x;
     F32 current_thumbnail_y = TEXT_CHAR_HEIGHT + PIXEL_SIZE;

     bool none_checked = true;
     for(S16 c = 0; c < tag_checkboxes->count; c++){
          auto* checkbox = tag_checkboxes->elements + c + 1;
          if(checkbox->checked){
               none_checked = false;
               break;
          }
     }

     if(none_checked){
          for(S16 m = 0; m < map_thumbnails->count; m++){
               auto* map_thumbnail = map_thumbnails->elements + m;
               map_thumbnail->pos.x = current_thumbnail_x;
               map_thumbnail->pos.y = current_thumbnail_y;

               current_thumbnail_x += THUMBNAIL_UI_DIMENSION;
               if((m + 1) % THUMBNAILS_PER_ROW == 0){
                    current_thumbnail_x = thumbnail_row_start_x;
                    current_thumbnail_y += THUMBNAIL_UI_DIMENSION;
               }
          }

          // account for integer division truncation
          return map_thumbnails->count + THUMBNAILS_PER_ROW;
     }

     bool exclusive = tag_checkboxes->elements[0].checked;

     S16 match_index = 1;
     for(S16 m = 0; m < map_thumbnails->count; m++){
          auto* map_thumbnail = map_thumbnails->elements + m;

          bool matches = false;

          if(exclusive){
               matches = true;
               for(S16 c = 0; c < TAG_COUNT; c++){
                    auto* checkbox = tag_checkboxes->elements + c + 1;
                    if(checkbox->checked){
                         if(!map_thumbnail->tags[c]){
                              matches = false;
                              break;
                         }
                    }
               }
          }else{
               for(S16 c = 0; c < TAG_COUNT; c++){
                    auto* checkbox = tag_checkboxes->elements + c + 1;
                    if(checkbox->checked && map_thumbnail->tags[c]){
                         matches = true;
                         break;
                    }
               }
          }

          if(matches){
               map_thumbnail->pos.x = current_thumbnail_x;
               map_thumbnail->pos.y = current_thumbnail_y;

               current_thumbnail_x += THUMBNAIL_UI_DIMENSION;
               if(match_index % THUMBNAILS_PER_ROW == 0){
                    current_thumbnail_x = thumbnail_row_start_x;
                    current_thumbnail_y += THUMBNAIL_UI_DIMENSION;
               }
               match_index++;
          }else{
               map_thumbnail->pos.x = -THUMBNAIL_UI_DIMENSION;
               map_thumbnail->pos.y = -THUMBNAIL_UI_DIMENSION;
          }
     }

     return ((match_index - 1) + THUMBNAILS_PER_ROW);
}

Coord_t find_centroid(CentroidStart_t a, CentroidStart_t b){
     // find the missing corners of the rectangle
     Coord_t rect_corner_a;
     rect_corner_a.x = a.coord.x;
     rect_corner_a.y = b.coord.y;

     Coord_t rect_corner_b;
     rect_corner_b.x = b.coord.x;
     rect_corner_b.y = a.coord.y;

     // find the formula for the diagonals that cross of those corners
     // y = mx + b where m = 1 and -1

     // b1 = y - mx
     // b2 = y + mx

     S16 a_b_one = rect_corner_a.y - rect_corner_a.x;
     S16 a_b_two = rect_corner_a.y + rect_corner_a.x;

     S16 b_b_one = rect_corner_b.y - rect_corner_b.x;
     S16 b_b_two = rect_corner_b.y + rect_corner_b.x;

     // find the intersection of our 2 formulas
     // ay = max + ab
     // by = mbx + bb

     // 1x + ab1 = -1x + bb2
     // 2x = bb2 - ab1
     // x = (bb2 - ab1) / 2

     // there are 2 intersections, calculate them both
     Coord_t first_option {};
     first_option.x = (b_b_two - a_b_one) / 2;
     first_option.y = first_option.x + a_b_one;

     // -1x + ab2 = 1x + bb2
     // 2x = ab2 - bb1
     // x = (ab2 - bb1) / 2

     Coord_t second_option {};
     second_option.x = (a_b_two - b_b_one) / 2;
     second_option.y = -second_option.x + a_b_two;

     DirectionMask_t a_mask = coord_direction_mask_between(a.coord, first_option);
     DirectionMask_t b_mask = coord_direction_mask_between(b.coord, first_option);

     // find the intersection that they are both facing towards or away from
     bool a_facing = direction_in_mask(a_mask, a.direction);
     bool b_facing = direction_in_mask(b_mask, b.direction);

     bool a_facing_away = direction_in_mask(a_mask, direction_opposite(a.direction));
     bool b_facing_away = direction_in_mask(b_mask, direction_opposite(b.direction));

     char a_m[128];
     char b_m[128];
     direction_mask_to_string(a_mask, a_m, 128);
     direction_mask_to_string(b_mask, b_m, 128);

     if((a_facing && b_facing) || (a_facing_away && b_facing_away)){
          return first_option;
     }

     a_mask = coord_direction_mask_between(a.coord, second_option);
     b_mask = coord_direction_mask_between(b.coord, second_option);

     a_facing = direction_in_mask(a_mask, a.direction);
     b_facing = direction_in_mask(b_mask, b.direction);

     a_facing_away = direction_in_mask(a_mask, direction_opposite(a.direction));
     b_facing_away = direction_in_mask(b_mask, direction_opposite(b.direction));

     direction_mask_to_string(a_mask, a_m, 128);
     direction_mask_to_string(b_mask, b_m, 128);

     if((a_facing && b_facing) || (a_facing_away && b_facing_away)){
          return second_option;
     }

     // if we still haven't found it, check if we are on the same level as any of them
     bool a_on_same_level = false;
     bool b_on_same_level = false;

     if(direction_is_horizontal(a.direction)){
          a_on_same_level = (a.coord.x == first_option.x);
     }else{
          a_on_same_level = (a.coord.y == first_option.y);
     }

     if(direction_is_horizontal(b.direction)){
          b_on_same_level = (b.coord.x == first_option.x);
     }else{
          b_on_same_level = (b.coord.y == first_option.y);
     }

     if(a_on_same_level && b_on_same_level) return first_option;

     if(direction_is_horizontal(a.direction)){
          a_on_same_level = (a.coord.x == second_option.x);
     }else{
          a_on_same_level = (a.coord.y == second_option.y);
     }

     if(direction_is_horizontal(b.direction)){
          b_on_same_level = (b.coord.x == second_option.x);
     }else{
          b_on_same_level = (b.coord.y == second_option.y);
     }

     if(a_on_same_level && b_on_same_level) return second_option;

     return Coord_t{-1, -1};
}

bool find_and_update_connected_teleported_block(Block_t* block, Direction_t direction, World_t* world){
     Position_t block_pos = block_get_position(block);
     Vec_t block_pos_delta_vec = block_get_pos_delta(block);
     Vec_t block_vel_vec = block_get_vel(block);
     auto block_cut = block_get_cut(block);

     auto against_result = block_against_other_blocks(block_pos + block_pos_delta_vec,
                                                      block_cut, direction, world->block_qt,
                                                      world->interactive_qt, &world->tilemap, false);

     F32 block_vel = 0;
     F32 block_pos_delta = 0;

     if(direction_is_horizontal(direction)){
         block_vel = block_vel_vec.x;
         block_pos_delta = block_pos_delta_vec.x;
     }else{
         block_vel = block_vel_vec.y;
         block_pos_delta = block_pos_delta_vec.y;
     }

     for(S16 a = 0; a < against_result.count; a++){
         auto* against_other = against_result.objects + a;

         if(!against_other->through_portal && !block->teleport) continue;

         Vec_t against_block_vel_vec = block_get_vel(against_other->block);
         Vec_t against_block_pos_delta_vec = block_get_pos_delta(against_other->block);

         Vec_t rotated_against_vel = vec_rotate_quadrants_counter_clockwise(against_block_vel_vec, against_other->rotations_through_portal);
         Vec_t rotated_against_pos_delta = vec_rotate_quadrants_counter_clockwise(against_block_pos_delta_vec, against_other->rotations_through_portal);

         F32 against_block_vel = 0;
         F32 against_block_pos_delta = 0;

         if(direction_is_horizontal(direction)){
             against_block_vel = rotated_against_vel.x;
             against_block_pos_delta = rotated_against_pos_delta.x;
         }else{
             against_block_vel = rotated_against_vel.y;
             against_block_pos_delta = rotated_against_pos_delta.y;
         }

         if(block_vel != against_block_vel || block_pos_delta != against_block_pos_delta) continue;

         block->connected_teleport.block_index = get_block_index(world, against_other->block);
         block->connected_teleport.direction = direction;
         return true;
     }

     return false;
}

int main(int argc, char** argv){
     const char* load_map_filepath = nullptr;
     bool test = false;
     bool suite = false;
     bool show_suite = false;
     bool fail_slow = false;
     bool update_tags = false;
     char* map_number_filepath = NULL;
     S16 map_number = 0;
     S16 first_map_number = 0;
     S16 first_frame = 0;
     S16 fail_count = 0;
     int window_width = 800;
     int window_height = 800;
     int window_x = SDL_WINDOWPOS_CENTERED;
     int window_y = SDL_WINDOWPOS_CENTERED;

     GameMode_t game_mode = GAME_MODE_PLAYING;

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
          }else if(strcmp(argv[i], "-updatetags") == 0){
               update_tags = true;
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
               printf("  -updatetags             when running a test, at the end update the tags in the map file\n");
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

     clear_global_tags();

     SDL_Window* window = nullptr;
     SDL_GLContext opengl_context = nullptr;
     GLuint theme_texture = 0;
     GLuint player_texture = 0;
     GLuint arrow_texture = 0;
     GLuint text_texture = 0;
     GLuint render_framebuffer = 0;
     GLuint render_texture = 0;
     GLuint thumbnail_framebuffer = 0;
     GLuint thumbnail_texture = 0;

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

          glGenFramebuffers(1, &thumbnail_framebuffer);
          glBindFramebuffer(GL_FRAMEBUFFER, thumbnail_framebuffer);

          glGenTextures(1, &thumbnail_texture);
          glBindTexture(GL_TEXTURE_2D, thumbnail_texture);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, THUMBNAIL_DIMENSION, THUMBNAIL_DIMENSION, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, thumbnail_texture, 0);

          GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
          glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

          auto rc = glCheckFramebufferStatus(GL_FRAMEBUFFER);
          if(rc != GL_FRAMEBUFFER_COMPLETE){
              LOG("glCheckFramebufferStatus() rc: %x\n", rc);
              return 1;
          }

          glGenFramebuffers(1, &render_framebuffer);
          glBindFramebuffer(GL_FRAMEBUFFER, render_framebuffer);

          // TODO: need to resize if the window resizes
          glGenTextures(1, &render_texture);
          glBindTexture(GL_TEXTURE_2D, render_texture);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_width, window_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, render_texture, 0);

          glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

          rc = glCheckFramebufferStatus(GL_FRAMEBUFFER);
          if(rc != GL_FRAMEBUFFER_COMPLETE){
              LOG("glCheckFramebufferStatus() rc: %x\n", rc);
              return 1;
          }
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

     bool current_map_tags[TAG_COUNT];
     memset(current_map_tags, 0, TAG_COUNT * sizeof(*current_map_tags));

     Coord_t player_start {2, 8};

     S64 frame_count = 0;

     bool quit = false;
     bool seeked_with_mouse = false;
     bool resetting = false;
     F32 reset_timer = 1.0f;

     PlayerAction_t player_action {};

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

          load_map_tags(load_map_filepath, current_map_tags);

          if(play_demo.mode == DEMO_MODE_PLAY){
               cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
          }
     }else if(suite){
          auto load_result = load_map_number(map_number, &player_start, &world);
          if(!load_result.success){
               return 1;
          }
          load_map_tags(load_result.filepath, current_map_tags);

          map_number_filepath = load_result.filepath;

          cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

          play_demo.mode = DEMO_MODE_PLAY;
          if(!load_map_number_demo(&play_demo, map_number, &frame_count)){
               return 1;
          }
     }else if(map_number){
          auto load_result = load_map_number(map_number, &player_start, &world);
          if(!load_result.success){
               return 1;
          }

          load_map_tags(load_result.filepath, current_map_tags);

          map_number_filepath = load_result.filepath;

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

     Camera_t camera {};

     reset_map(player_start, &world, &undo, &camera);
     init(&editor);

     // init ui
     Vec_t checkbox_scroll {};
     ObjectArray_t<Checkbox_t> tag_checkboxes;
     init(&tag_checkboxes, TAG_COUNT + 1);

     for(S16 c = 0; c < tag_checkboxes.count; c++){
          Checkbox_t* checkbox = tag_checkboxes.elements + c;
          checkbox->pos.x = CHECKBOX_START_OFFSET_X;
          checkbox->pos.y = CHECKBOX_START_OFFSET_Y + CHECKBOX_INTERVAL * (F32)(c);
     }

     auto all_maps = find_all_maps();

     Vec_t map_scroll {};
     char* hovered_map_thumbnail_path = NULL;
     S16 hovered_map_thumbnail_index = 0;
     S16 visible_map_thumbnail_count = 0;
     ObjectArray_t<MapThumbnail_t> map_thumbnails;
     init(&map_thumbnails, all_maps.count);

     {
          for(U32 m = 0; m < all_maps.count; m++){
               auto* map_thumbnail = map_thumbnails.elements + m;
               map_thumbnail->map_filepath = strdup(all_maps.entries[m].path);
               map_thumbnail->map_number = all_maps.entries[m].map_number;
               load_map_tags(map_thumbnail->map_filepath, map_thumbnail->tags);
               Raw_t loaded_thumbnail {};
               if(load_map_thumbnail(map_thumbnail->map_filepath, &loaded_thumbnail)){
                    map_thumbnail->texture = transparent_texture_from_raw_bitmap(&loaded_thumbnail);
                    free(loaded_thumbnail.bytes);
               }else{
                    map_thumbnail->texture = 0;
               }
               free(all_maps.entries[m].path);
          }
          free(all_maps.entries);

          qsort(map_thumbnails.elements, map_thumbnails.count, sizeof(map_thumbnails.elements[0]), map_thumbnail_comparor);

          visible_map_thumbnail_count = filter_thumbnails(&tag_checkboxes, &map_thumbnails);
     }

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
               player_action.last_activate = player_action.activate;
          }

          for(S16 i = 0; i < world.players.count; i++) {
               world.players.elements[i].reface = false;
          }

          if(play_demo.mode == DEMO_MODE_PLAY){
               if(demo_play_frame(&play_demo, &player_action, &world.players, frame_count, &record_demo)){
                    if(update_tags){
                         World_t a_whole_new_world {};
                         bool* updated_tags = get_global_tags();
                         if(load_map(map_number_filepath, &player_start, &a_whole_new_world.tilemap, &a_whole_new_world.blocks, &a_whole_new_world.interactives)){
                              Raw_t* thumbnail_ptr = NULL;

                              Raw_t raw_thumbnail {};
                              if(load_map_thumbnail(map_number_filepath, &raw_thumbnail)){
                                   thumbnail_ptr = &raw_thumbnail;
                              }
                              save_map(map_number_filepath, player_start, &a_whole_new_world.tilemap, &a_whole_new_world.blocks, &a_whole_new_world.interactives, updated_tags, thumbnail_ptr);
                              if(thumbnail_ptr){
                                   free(raw_thumbnail.bytes);
                              }
                         }
                         clear_global_tags();
                    }
                    if(test){
                         bool passed = test_map_end_state(&world, &play_demo);
                         clear_global_tags();
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

                              auto load_result = load_map_number_map(map_number, &world, &undo, &player_start, &player_action, &camera, current_map_tags);
                              if(load_result.success){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
                                   free(map_number_filepath);
                                   map_number_filepath = load_result.filepath;

                                   if(load_map_number_demo(&play_demo, map_number, &frame_count)){
                                        init(&world.arrows);
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
                    if(record_demo.mode == DEMO_MODE_RECORD){
                         frame_count--;
                         break;
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
                    case SDL_SCANCODE_F1:
                    {
                         bool has_bow = !world.players.elements[0].has_bow;
                         for(S16 i = 0; i < world.players.count; i++){
                              world.players.elements[i].has_bow = has_bow;
                         }
                         break;
                    }
                    case SDL_SCANCODE_F4:
                         game_mode = GAME_MODE_PLAYING;
                         break;
                    case SDL_SCANCODE_F5:
                         if(game_mode == GAME_MODE_EDITOR){
                              if(world.tilemap.width <= 1) break;

                              TileMap_t map_copy = {};
                              deep_copy(&world.tilemap, &map_copy);

                              for(S16 i = 0; i < map_copy.height; i++){
                                   Coord_t coord{(S16)(map_copy.width - 1), i};
                                   coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                              }

                              destroy(&world.tilemap);
                              init(&world.tilemap, map_copy.width - 1, map_copy.height);

                              for(S16 i = 0; i < world.tilemap.height; i++){
                                   for(S16 j = 0; j < world.tilemap.width; j++){
                                        world.tilemap.tiles[i][j] = map_copy.tiles[i][j];
                                   }
                              }

                              destroy(&map_copy);
                              camera.center_on_tilemap(&world.tilemap);
                         }
                         break;
                    case SDL_SCANCODE_F6:
                         if(game_mode == GAME_MODE_EDITOR){
                              TileMap_t map_copy = {};
                              deep_copy(&world.tilemap, &map_copy);

                              destroy(&world.tilemap);
                              init(&world.tilemap, map_copy.width + 1, map_copy.height);

                              for(S16 i = 0; i < map_copy.height; i++){
                                   for(S16 j = 0; j < map_copy.width; j++){
                                        world.tilemap.tiles[i][j] = map_copy.tiles[i][j];
                                   }
                              }

                              destroy(&map_copy);
                              camera.center_on_tilemap(&world.tilemap);
                         }
                         break;
                    case SDL_SCANCODE_F7:
                         if(game_mode == GAME_MODE_EDITOR){
                              if(world.tilemap.height <= 1) break;

                              TileMap_t map_copy = {};
                              deep_copy(&world.tilemap, &map_copy);

                              for(S16 i = 0; i < map_copy.width; i++){
                                   Coord_t coord{i, (S16)(map_copy.height - 1)};
                                   coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                              }

                              destroy(&world.tilemap);
                              init(&world.tilemap, map_copy.width, map_copy.height - 1);

                              for(S16 i = 0; i < world.tilemap.height; i++){
                                   for(S16 j = 0; j < world.tilemap.width; j++){
                                        world.tilemap.tiles[i][j] = map_copy.tiles[i][j];
                                   }
                              }

                              destroy(&map_copy);
                              camera.center_on_tilemap(&world.tilemap);
                         }
                         break;
                    case SDL_SCANCODE_F8:
                         if(game_mode == GAME_MODE_EDITOR){
                              TileMap_t map_copy = {};
                              deep_copy(&world.tilemap, &map_copy);

                              destroy(&world.tilemap);
                              init(&world.tilemap, map_copy.width, map_copy.height + 1);

                              for(S16 i = 0; i < map_copy.height; i++){
                                   for(S16 j = 0; j < map_copy.width; j++){
                                        world.tilemap.tiles[i][j] = map_copy.tiles[i][j];
                                   }
                              }

                              destroy(&map_copy);
                              camera.center_on_tilemap(&world.tilemap);
                         }
                         break;
                    case SDL_SCANCODE_F12:
                         game_mode = GAME_MODE_LEVEL_SELECT;
                         break;
                    case SDL_SCANCODE_LEFT:
                    case SDL_SCANCODE_A:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_LEFT);
                         }else if(play_demo.mode == DEMO_MODE_PLAY){
                              if(frame_count > 0 && play_demo.seek_frame < 0){
                                   play_demo.seek_frame = frame_count - 1;

                                   restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                                &play_demo, &frame_count, &player_start, &player_action, &undo, &camera);
                              }
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_LEFT_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_D:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
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
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              move_selection(&editor, DIRECTION_UP);
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_UP_START,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_S:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
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
                    {
                         auto load_result = load_map_number_map(map_number, &world, &undo, &player_start, &player_action, &camera, current_map_tags);
                         if(load_result.success){
                              if(record_demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
                              }
                              free(map_number_filepath);
                              map_number_filepath = load_result.filepath;
                         }
                         break;
                    }
                    case SDL_SCANCODE_LEFTBRACKET:
                    {
                         map_number--;
                         auto load_result = load_map_number_map(map_number, &world, &undo, &player_start, &player_action, &camera, current_map_tags);
                         if(load_result.success){
                              free(map_number_filepath);
                              map_number_filepath = load_result.filepath;
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
                    }
                    case SDL_SCANCODE_RIGHTBRACKET:
                    {
                         map_number++;
                         auto load_result = load_map_number_map(map_number, &world, &undo, &player_start, &player_action, &camera, current_map_tags);
                         if(load_result.success){
                              free(map_number_filepath);
                              map_number_filepath = load_result.filepath;
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
                    }
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
                         if(game_mode == GAME_MODE_EDITOR){
                              Raw_t* thumbnail_ptr = NULL;
                              glBindFramebuffer(GL_FRAMEBUFFER, thumbnail_framebuffer);
                              Raw_t thumbnail = create_thumbnail_bitmap();
                              if(thumbnail.bytes) thumbnail_ptr = &thumbnail;
                              glBindFramebuffer(GL_FRAMEBUFFER, render_framebuffer);
                              char filepath[64];
                              snprintf(filepath, 64, "content/%03d.bm", map_number);
                              save_map(filepath, player_start, &world.tilemap, &world.blocks, &world.interactives, current_map_tags, thumbnail_ptr);
                              if(thumbnail.bytes) free(thumbnail.bytes);
                         }
                         break;
                    case SDL_SCANCODE_U:
                         if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_UNDO,
                                                    record_demo.mode, record_demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_N:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              Tile_t* tile = tilemap_get_tile(&world.tilemap, mouse_select_world_coord(mouse_screen, &camera));
                              if(tile) tile_toggle_wire_activated(tile);
                         }
                         break;
                    case SDL_SCANCODE_4:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world_coord(mouse_screen, &camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                              for(S16 i = 0; i < block_count; i++){
                                   blocks[i]->entangle_index = -1;
                              }
                         }
                    case SDL_SCANCODE_8:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world_coord(mouse_screen, &camera);
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
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto pixel = mouse_select_world_pixel(mouse_screen, &camera);

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
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world_coord(mouse_screen, &camera);
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
                         if(game_mode == GAME_MODE_PLAYING){
                              game_mode = GAME_MODE_EDITOR;
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }else{
                              game_mode = GAME_MODE_PLAYING;
                              editor.selection_start = {};
                              editor.selection_end = {};
                              destroy(&editor.entangle_indices);
                         }
                         break;
                    case SDL_SCANCODE_TAB:
                         if(game_mode == GAME_MODE_EDITOR){
                              if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                                   editor.mode = EDITOR_MODE_STAMP_HIDE;
                              }else{
                                   editor.mode = EDITOR_MODE_STAMP_SELECT;
                              }
                         }
                         break;
                    case SDL_SCANCODE_RETURN:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
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
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
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
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              destroy(&editor.clipboard);
                              shallow_copy(&editor.selection, &editor.clipboard);
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_P:
                         if(game_mode == GAME_MODE_EDITOR && editor.mode == EDITOR_MODE_CATEGORY_SELECT && editor.clipboard.count){
                              destroy(&editor.selection);
                              shallow_copy(&editor.clipboard, &editor.selection);
                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         }
                         break;
                    case SDL_SCANCODE_M:
                         if(game_mode == GAME_MODE_EDITOR &&
                            editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              player_start = mouse_select_world_coord(mouse_screen, &camera);
                         }else if(game_mode == GAME_MODE_PLAYING){
                              resetting = true;
                         }
                         break;
                    case SDL_SCANCODE_R:
                         if(game_mode == GAME_MODE_EDITOR &&
                            editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world_coord(mouse_screen, &camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                              for(S16 i = 0; i < block_count; i++){
                                   blocks[i]->rotation += 1;
                                   blocks[i]->rotation %= DIRECTION_COUNT;
                              }
                         }else if(game_mode == GAME_MODE_PLAYING){
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
                         player->pos.pixel = mouse_select_world_pixel(mouse_screen, &camera);
                         player->pos.decimal.x = 0;
                         player->pos.decimal.y = 0;
                         break;
                    }
                    case SDL_SCANCODE_H:
                    {
                         Pixel_t pixel = mouse_select_world_pixel(mouse_screen, &camera) + HALF_TILE_SIZE_PIXEL;
                         Coord_t coord = mouse_select_world_coord(mouse_screen, &camera);
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
                                               record_demo.mode, record_demo.file, frame_count);
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
                         switch(game_mode){
                         default:
                              break;
                         case GAME_MODE_PLAYING:
                              if(play_demo.mode == DEMO_MODE_PLAY){
                                   if(vec_in_quad(&pct_bar_outline_quad, mouse_screen)){
                                        seeked_with_mouse = true;

                                        play_demo.seek_frame = (S64)((F32)(play_demo.last_frame) * mouse_screen.x);

                                        if(play_demo.seek_frame < frame_count){
                                            restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                                         &play_demo, &frame_count, &player_start, &player_action, &undo, &camera);
                                        }else if(play_demo.seek_frame == frame_count){
                                             play_demo.seek_frame = -1;
                                        }
                                   }
                              }
                              break;
                         case GAME_MODE_EDITOR:
                              switch(editor.mode){
                              default:
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
                                        editor.selection_start = mouse_select_world_coord(mouse_screen, &camera);
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
                                        Coord_t select_coord = mouse_select_world_coord(mouse_screen, &camera);
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
                         case GAME_MODE_LEVEL_SELECT:
                              if(hovered_map_thumbnail_path){
                                   clear_global_tags();
                                   if(load_map(map_thumbnails.elements[hovered_map_thumbnail_index].map_filepath,
                                               &player_start, &world.tilemap, &world.blocks, &world.interactives)){
                                        load_map_tags(map_thumbnails.elements[hovered_map_thumbnail_index].map_filepath, current_map_tags);
                                        reset_map(player_start, &world, &undo, &camera);
                                        game_mode = GAME_MODE_PLAYING;
                                   }
                              }

                              for(S16 c = 0; c < tag_checkboxes.count; c++){
                                   Checkbox_t* checkbox = tag_checkboxes.elements + c;

                                   Quad_t checkbox_quad = checkbox->get_area(checkbox_scroll);
                                   if(vec_in_quad(&checkbox_quad, mouse_screen)){
                                        checkbox->checked = !checkbox->checked;
                                        visible_map_thumbnail_count = filter_thumbnails(&tag_checkboxes, &map_thumbnails);
                                        map_scroll.y = 0;
                                   }
                              }
                              break;
                         }
                         break;
                    case SDL_BUTTON_RIGHT:
                         if(game_mode == GAME_MODE_EDITOR){
                              switch(editor.mode){
                              default:
                                   break;
                              case EDITOR_MODE_CATEGORY_SELECT:
                                   undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                                   coord_clear(mouse_select_world_coord(mouse_screen, &camera), &world.tilemap, &world.interactives,
                                               world.interactive_qt, &world.blocks);
                                   break;
                              case EDITOR_MODE_STAMP_SELECT:
                              case EDITOR_MODE_STAMP_HIDE:
                              {
                                   undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                                   Coord_t start = mouse_select_world_coord(mouse_screen, &camera);
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
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEWHEEL:
                    {
                        if(mouse_screen.x <= CHECKBOX_THUMBNAIL_SPLIT){
                             const F32 max_scroll = -(CHECKBOX_INTERVAL * TAG_COUNT) + 1.0f;
                             F32 y_scroll = (F32)(-sdl_event.wheel.y) * CHECKBOX_INTERVAL;
                             F32 final_scroll = checkbox_scroll.y + y_scroll;
                             if(final_scroll > 0) final_scroll = 0;
                             if(final_scroll < max_scroll) final_scroll = max_scroll;
                             checkbox_scroll.y = final_scroll;
                        }else{
                             F32 max_scroll = -(THUMBNAIL_UI_DIMENSION * (visible_map_thumbnail_count / 4)) + 1.0f;
                             F32 y_scroll = (F32)(-sdl_event.wheel.y) * THUMBNAIL_UI_DIMENSION;
                             F32 final_scroll = map_scroll.y + y_scroll;
                             if(final_scroll > 0) final_scroll = 0;
                             if(final_scroll < max_scroll) final_scroll = max_scroll;
                             map_scroll.y = final_scroll;
                        }
                    } break;
               case SDL_MOUSEBUTTONUP:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         seeked_with_mouse = false;

                         if(game_mode == GAME_MODE_EDITOR){
                              switch(editor.mode){
                              default:
                                   break;
                              case EDITOR_MODE_CREATE_SELECTION:
                              {
                                   editor.selection_end = mouse_select_world_coord(mouse_screen, &camera);

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
                                                       stamp->block.cut = block->cut;
                                                  }
                                             }
                                        }
                                   }

                                   editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                              } break;
                              }
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEMOTION:
                    mouse_screen = Vec_t{((F32)(sdl_event.button.x) / (F32)(window_width)),
                                         1.0f - ((F32)(sdl_event.button.y) / (F32)(window_height))};
                    mouse_world = vec_to_pos(camera.normalized_to_world(mouse_screen));

                    if(game_mode == GAME_MODE_EDITOR){
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CREATE_SELECTION:
                              if(editor.selection_start.x >= 0 && editor.selection_start.y >= 0){
                                   editor.selection_end = pos_to_coord(mouse_world + camera.bottom_left());
                              }
                              break;
                         }
                    }else if(game_mode == GAME_MODE_PLAYING){
                         if(seeked_with_mouse && play_demo.mode == DEMO_MODE_PLAY){
                              play_demo.seek_frame = (S64)((F32)(play_demo.last_frame) * mouse_screen.x);

                              if(play_demo.seek_frame < frame_count){
                                   restart_demo(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives,
                                                &play_demo, &frame_count, &player_start, &player_action, &undo, &camera);
                              }else if(play_demo.seek_frame == frame_count){
                                   play_demo.seek_frame = -1;
                              }
                         }
                    }else if(game_mode == GAME_MODE_LEVEL_SELECT){
                         if(hovered_map_thumbnail_path) free(hovered_map_thumbnail_path);
                         hovered_map_thumbnail_path = NULL;
                         for(S16 m = 0; m < map_thumbnails.count; m++){
                              auto* map_thumbnail = map_thumbnails.elements + m;
                              auto quad = map_thumbnail->get_area(map_scroll);
                              if(vec_in_quad(&quad, mouse_screen)){
                                   hovered_map_thumbnail_path = strdup(map_thumbnail->map_filepath);
                                   hovered_map_thumbnail_index = m;
                                   char* itr = hovered_map_thumbnail_path;
                                   while(*itr){
                                        *itr = toupper(*itr);
                                        itr++;
                                   }
                                   break;
                              }
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
                    if(arrow->spawned_this_frame){
                         arrow->spawned_this_frame = false;
                         continue;
                    }

                    Coord_t pre_move_coord = pixel_to_coord(arrow->pos.pixel);

                    if(arrow->element == ELEMENT_FIRE){
                         U8 light_height = (arrow->pos.z / HEIGHT_INTERVAL) * LIGHT_DECAY;
                         illuminate(pre_move_coord, 255 - light_height, &world);
                    }

                    Coord_t post_move_coord = pre_move_coord;

                    if(arrow->stuck_time > 0.0f){
                         arrow->stuck_time += dt;

                         switch(arrow->stuck_type){
                         default:
                              break;
                         case STUCK_BLOCK:
                              if(arrow->stuck_index >= 0){
                                   Block_t* stuck_block = world.blocks.elements + arrow->stuck_index;
                                   arrow->pos = block_get_center(stuck_block) + arrow->stuck_offset;
                              }
                         case STUCK_POPUP:
                              if(arrow->stuck_index >= 0){
                                   Interactive_t* stuck_popup = world.interactives.elements + arrow->stuck_index;
                                   if(stuck_popup->type == INTERACTIVE_TYPE_POPUP){
                                        arrow->pos.z = stuck_popup->popup.lift.ticks + arrow->stuck_offset.z;
                                   }
                              }
                              break;
                         case STUCK_DOOR:
                              if(arrow->stuck_index >= 0){
                                   Interactive_t* stuck_popup = world.interactives.elements + arrow->stuck_index;
                                   if(stuck_popup->type == INTERACTIVE_TYPE_DOOR){
                                        arrow->pos.z = stuck_popup->door.lift.ticks + arrow->stuck_offset.z;
                                   }
                              }
                              break;
                         }

                         post_move_coord = pixel_to_coord(arrow->pos.pixel);

                         if(arrow->stuck_time > ARROW_DISINTEGRATE_DELAY){
                              arrow->alive = false;
                         }
                    }else{
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
                         post_move_coord = pixel_to_coord(arrow->pos.pixel);

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
                                        add_global_tag(TAG_ARROW_STICKS_INTO_BLOCK);
                                        arrow->stuck_time = dt;
                                        arrow->stuck_offset = arrow->pos - block_get_center(blocks[b]);
                                        arrow->stuck_type = STUCK_BLOCK;
                                        arrow->stuck_index = get_block_index(&world, blocks[b]);
                                        arrow->vel = 0;
                                   }else if(arrow->pos.z > block_top && arrow->pos.z < (block_top + HEIGHT_INTERVAL)){
                                        // TODO(jtardiff): being held down is probably not quite enough to block us from lighting the block
                                        auto held_down_result = block_held_down_by_another_block(blocks[b], world.block_qt, world.interactive_qt, &world.tilemap);
                                        if(!held_down_result.held()){
                                             arrow->element_from_block = block_index;
                                             if(arrow->element != blocks[b]->element){
                                                  Element_t arrow_element = arrow->element;
                                                  arrow->element = transition_element(arrow->element, blocks[b]->element);
                                                  if(arrow->entangle_index >= 0){
                                                       Arrow_t* entangled_arrow = world.arrows.arrows + arrow->entangle_index;
                                                       entangled_arrow->element = transition_element(entangled_arrow->element, blocks[b]->element);
                                                  }
                                                  if(arrow_element){
                                                       blocks[b]->element = transition_element(blocks[b]->element, arrow_element);
                                                       add_global_tag(TAG_ARROW_CHANGES_BLOCK_ELEMENT);
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
                    }

                    if(pre_move_coord != post_move_coord){
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, post_move_coord);
                         if(tile && tile_is_solid(tile) && arrow->stuck_time == 0){
                              arrow->stuck_time = dt;
                              arrow->vel = 0;
                         }

                         // catch or give elements
                         if(arrow->element == ELEMENT_FIRE){
                              if(arrow->stuck_time == 0){
                                   melt_ice(post_move_coord, arrow->pos.z, 0, &world);
                              }else{
                                   melt_ice(post_move_coord - arrow->face, arrow->pos.z, 0, &world);
                              }
                         }else if(arrow->element == ELEMENT_ICE){
                              if(arrow->stuck_time == 0){
                                   spread_ice(post_move_coord, arrow->pos.z, 0, &world);
                              }else{
                                   spread_ice(post_move_coord - arrow->face, arrow->pos.z, 0, &world);
                              }
                         }

                         if(arrow->stuck_time == 0){
                              Interactive_t* interactive = quad_tree_interactive_find_at(world.interactive_qt, post_move_coord);
                              if(interactive){
                                   switch(interactive->type){
                                   default:
                                        break;
                                   case INTERACTIVE_TYPE_LEVER:
                                        if(arrow->pos.z >= HEIGHT_INTERVAL){
                                             activate(&world, post_move_coord);
                                             add_global_tag(TAB_ARROW_ACTIVATES_LEVER);
                                        }else{
                                             arrow->stuck_time = dt;
                                             arrow->vel = 0;
                                        }
                                        break;
                                   case INTERACTIVE_TYPE_DOOR:
                                        if(interactive->door.lift.ticks < arrow->pos.z){
                                             arrow->stuck_time = dt;
                                             arrow->stuck_type = STUCK_DOOR;
                                             arrow->stuck_index = interactive - world.interactives.elements;
                                             arrow->stuck_offset.z = arrow->pos.z - interactive->door.lift.ticks;
                                             arrow->vel = 0;
                                        }
                                        break;
                                   case INTERACTIVE_TYPE_POPUP:
                                        if(interactive->popup.lift.ticks > arrow->pos.z){
                                             arrow->stuck_time = dt;
                                             arrow->stuck_type = STUCK_POPUP;
                                             arrow->stuck_index = interactive - world.interactives.elements;
                                             arrow->stuck_offset.z = arrow->pos.z - interactive->popup.lift.ticks;
                                             arrow->vel = 0;
                                        }
                                        break;
                                   case INTERACTIVE_TYPE_PORTAL:
                                        if(!interactive->portal.on){
                                             arrow->stuck_time = dt;
                                             arrow->vel = 0;
                                             // TODO: arrow drops if portal turns on
                                        }else if(!portal_has_destination(post_move_coord, &world.tilemap, world.interactive_qt)){
                                             // TODO: arrow drops if portal turns on
                                             arrow->stuck_time = dt;
                                             arrow->vel = 0;
                                        }
                                        break;
                                   }
                              }
                         }

                         auto teleport_result = teleport_position_across_portal(arrow->pos, Vec_t{}, &world,
                                                                                pre_move_coord, post_move_coord);
                         if(teleport_result.count > 0){
                              add_global_tag(TAG_TELEPORT_ARROW);

                              S16 last_entangle_index = i;

                              for(S16 t = 1; t < teleport_result.count; t++){
                                   Arrow_t* spawned_arrow = arrow_spawn(&world.arrows, teleport_result.results[t].pos,
                                                                        direction_rotate_clockwise(arrow->face, teleport_result.results[t].rotations));
                                   spawned_arrow->element = arrow->element;
                                   spawned_arrow->vel = rotate_vec_counter_clockwise_to_see_if_negates(arrow->vel, direction_is_horizontal(arrow->face), teleport_result.results[t].rotations);
                                   spawned_arrow->fall_time = arrow->fall_time;
                                   spawned_arrow->spawned_this_frame = true;

                                   spawned_arrow->entangle_index = last_entangle_index;
                                   last_entangle_index = spawned_arrow - world.arrows.arrows;
                              }

                              arrow->pos = teleport_result.results[0].pos;
                              arrow->face = direction_rotate_clockwise(arrow->face, teleport_result.results[0].rotations);

                              if(teleport_result.count > 1){
                                   arrow->entangle_index = last_entangle_index;
                                   // TODO: compress this code with block/player entanglement
                                   auto* src_portal = quad_tree_find_at(world.interactive_qt, teleport_result.results[0].src_portal.x, teleport_result.results[0].src_portal.y);
                                   if(is_active_portal(src_portal)){
                                        src_portal->portal.on = false;
                                        activate(&world, teleport_result.results[0].src_portal);
                                   }
                              }
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
                              add_global_tag(TAG_ARROW);
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
                                    add_global_tag(TAB_POPUP_RAISES_PLAYER);
                              }
                         }
                    }

                    if(!player->held_up){
                         auto result = player_in_block_rect(player, &world.tilemap, world.interactive_qt, world.block_qt);
                         for(S8 e = 0; e < result.entries.count; e++){
                              auto& entry = result.entries.objects[e];
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
                                        add_global_tag(TAB_POPUP_RAISES_BLOCK);
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

               // pass to detect if blocks are in a pit or just held up by the floor
               for(S16 i = 0; i < world.blocks.count; i++){
                    auto block = world.blocks.elements + i;

                    if(block->pos.z <= 0){
                         if(block->pos.z <= -HEIGHT_INTERVAL) continue;

                         block->over_pit = false;

                         auto coord = block_get_coord(block);
                         auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, coord);

                         if(interactive && interactive->type == INTERACTIVE_TYPE_PIT){
                              auto coord_rect = rect_surrounding_coord(coord);
                              auto block_pos = block_get_position(block);
                              auto block_pos_delta = block_get_pos_delta(block);
                              auto final_block_pos = block_pos + block_pos_delta;
                              block_pos.pixel.x = passes_over_pixel(block_pos.pixel.x, final_block_pos.pixel.x);
                              block_pos.pixel.y = passes_over_pixel(block_pos.pixel.y, final_block_pos.pixel.y);
                              auto block_rect = block_get_inclusive_rect(block_pos.pixel, block_get_cut(block));
                              if(rect_completely_in_rect(block_rect, coord_rect) &&
                                 !block->held_up){
                                   block->over_pit = true;
                              }
                         }

                         if(!block->over_pit && !block->held_up && block->pos.z == 0){
                              block->held_up |= BLOCK_HELD_BY_FLOOR;
                         }
                    }
               }

               for(S16 i = 0; i < world.blocks.count; i++){
                    auto block = world.blocks.elements + i;

                    if(block->entangle_index >= 0){
                         S16 entangle_index = block->entangle_index;
                         while(entangle_index != i && entangle_index >= 0){
                              auto entangled_block = world.blocks.elements + entangle_index;
                              if(entangled_block->held_up & BLOCK_HELD_BY_SOLID ||
                                 entangled_block->held_up & BLOCK_HELD_BY_FLOOR){
                                   block->held_up |= BLOCK_HELD_BY_ENTANGLE;
                                   break;
                              }
                              entangle_index = entangled_block->entangle_index;
                         }
                    }

                    if(!block->held_up){
                         if(block->pos.z <= -HEIGHT_INTERVAL) continue;

                         if(block->over_pit){
                              add_global_tag(TAG_BLOCK_FALLS_IN_PIT);
                         }

                         block->fall_time -= dt;
                         if(block->fall_time < 0){
                              block->fall_time = FALL_TIME;
                              block->pos.z--;
                              block->fall_time = 0;
                         }
                    }else if(block->held_up == BLOCK_HELD_BY_ENTANGLE){
                         add_global_tag(TAG_ENTANGLED_BLOCK_FLOATS);
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
                              would_teleport_onto_ice = block_on_frictionless(pos, pos_delta, block->cut, &world.tilemap, world.interactive_qt, world.block_qt);
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
                                        Direction_t block_move_dir = block_axis_move(block, direction_is_horizontal(player->face));

                                        switch(player->face){
                                        default:
                                             break;
                                        case DIRECTION_LEFT:
                                        case DIRECTION_RIGHT:
                                        {
                                             // only coast the block is actually moving
                                             if((player->pushing_block_rotation % 2) == 1){
                                                  block_move_dir = block_axis_move(block, false);

                                                  if(player->prev_pushing_block == i && player->pushing_block_dir == block_move_dir){
                                                       block->coast_vertical = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }else{
                                                  block_move_dir = block_axis_move(block, true);

                                                  if(player->prev_pushing_block == i && player->pushing_block_dir == block_move_dir){
                                                       block->coast_horizontal = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }
                                             break;
                                        }
                                        case DIRECTION_UP:
                                        case DIRECTION_DOWN:
                                        {
                                             if((player->pushing_block_rotation % 2) == 1){
                                                  block_move_dir = block_axis_move(block, true);

                                                  if(player->prev_pushing_block == i && player->pushing_block_dir == block_move_dir){
                                                       block->coast_horizontal = BLOCK_COAST_PLAYER;
                                                       set_block_coasting_from_player = true;
                                                  }
                                             }else{
                                                  block_move_dir = block_axis_move(block, false);

                                                  if(player->prev_pushing_block == i && player->pushing_block_dir == block_move_dir){
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

                                        if(entangled_block->coast_horizontal > BLOCK_COAST_NONE){
                                             if(rotations_between % 2 == 0){
                                                  if(entangled_block->coast_horizontal == BLOCK_COAST_PLAYER &&
                                                     block->horizontal_move.state != MOVE_STATE_STOPPING &&
                                                     entangled_block->horizontal_move.state != MOVE_STATE_IDLING){
                                                       block->coast_horizontal = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  }
                                             }else{
                                                  if(entangled_block->coast_horizontal == BLOCK_COAST_PLAYER &&
                                                     block->vertical_move.state != MOVE_STATE_STOPPING &&
                                                     entangled_block->horizontal_move.state != MOVE_STATE_IDLING){
                                                       block->coast_vertical = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  }
                                             }
                                        }

                                        if(entangled_block->coast_vertical > BLOCK_COAST_NONE){
                                             if(rotations_between % 2 == 0){
                                                  if(entangled_block->coast_vertical == BLOCK_COAST_PLAYER &&
                                                     block->vertical_move.state != MOVE_STATE_STOPPING &&
                                                     entangled_block->vertical_move.state != MOVE_STATE_IDLING){
                                                       block->coast_vertical = BLOCK_COAST_ENTANGLED_PLAYER;
                                                  }
                                             }else{
                                                  if(entangled_block->coast_vertical == BLOCK_COAST_PLAYER &&
                                                     block->horizontal_move.state != MOVE_STATE_STOPPING &&
                                                     entangled_block->vertical_move.state != MOVE_STATE_IDLING){
                                                       block->coast_horizontal = BLOCK_COAST_ENTANGLED_PLAYER;
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
                                               block->pos.pixel.x, block->pos.decimal.x, mass);

                    update_motion_grid_aligned(block->cut, &block->vertical_move, &y_component,
                                               coasting_vertically, dt,
                                               block->pos.pixel.y, block->pos.decimal.y, mass);
               }

               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;

                    if(block->coast_vertical <= BLOCK_COAST_ICE  || block->coast_horizontal <= BLOCK_COAST_ICE){
                         for(S16 p = 0; p < world.players.count; p++){
                              Player_t* player = world.players.elements + p;

                              // is the player pushing us ?
                              if(player->prev_pushing_block < 0) continue;

                              Block_t* player_prev_pushing_block = world.blocks.elements + player->prev_pushing_block;
                              if(blocks_are_entangled(block, player_prev_pushing_block, &world.blocks) &&
                                 !block_on_ice(block->pos, block->pos_delta, block->cut, &world.tilemap, world.interactive_qt, world.block_qt) &&
                                 !block_on_air(block, &world.tilemap, world.interactive_qt, world.block_qt)){
                                   Block_t* entangled_block = player_prev_pushing_block;

                                   auto rotations_between = blocks_rotations_between(block, entangled_block);
                                   MoveState_t check_idle_move_state = MOVE_STATE_IDLING;

                                   if(entangled_block->coast_horizontal > BLOCK_COAST_NONE){
                                        if(rotations_between % 2 == 0){
                                             check_idle_move_state = block->horizontal_move.state;
                                        }else{
                                             check_idle_move_state = block->vertical_move.state;
                                        }

                                        bool held_down = block_held_down_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap).held();

                                        if(check_idle_move_state == MOVE_STATE_IDLING && player->push_time > BLOCK_PUSH_TIME){
                                             if(!held_down){
                                                  Direction_t block_move_dir = block_axis_move(entangled_block, true);
                                                  // The block may be going through a portal, so if that is the case, rotate the direction the player is pushing
                                                  auto player_face = direction_rotate_counter_clockwise(player->face, player->pushing_block_rotation);
                                                  if(player_face == block_move_dir){
                                                       S16 block_mass = block_get_mass(player_prev_pushing_block);
                                                       S16 entangled_block_mass = block_get_mass(block);
                                                       F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
                                                       auto direction_to_push = direction_rotate_clockwise(block_move_dir, rotations_between);
                                                       auto allowed_result = allowed_to_push(&world, block, direction_to_push, mass_ratio);
                                                       if(allowed_result.push){
                                                            block_push(block, direction_to_push, &world, false, mass_ratio * allowed_result.mass_ratio);
                                                       }
                                                  }
                                             }else{
                                                  add_global_tag(TAG_ENTANGLED_BLOCK_HELD_DOWN_UNMOVABLE);
                                             }
                                        }
                                   }

                                   if(entangled_block->coast_vertical > BLOCK_COAST_NONE){
                                        if(rotations_between % 2 == 0){
                                             check_idle_move_state = block->vertical_move.state;
                                        }else{
                                             check_idle_move_state = block->horizontal_move.state;
                                        }

                                        bool held_down = block_held_down_by_another_block(block, world.block_qt, world.interactive_qt, &world.tilemap).held();

                                        if(check_idle_move_state == MOVE_STATE_IDLING && player->push_time > BLOCK_PUSH_TIME){
                                             if(!held_down){
                                                  Direction_t block_move_dir = block_axis_move(entangled_block, false);
                                                  auto player_face = direction_rotate_counter_clockwise(player->face, player->pushing_block_rotation);
                                                  if(player_face == block_move_dir){
                                                       S16 block_mass = block_get_mass(player_prev_pushing_block);
                                                       S16 entangled_block_mass = block_get_mass(block);
                                                       F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
                                                       auto direction_to_push = direction_rotate_clockwise(block_move_dir, rotations_between);
                                                       auto allowed_result = allowed_to_push(&world, block, direction_to_push, mass_ratio);
                                                       if(allowed_result.push){
                                                            block_push(block, direction_to_push, &world, false, mass_ratio * allowed_result.mass_ratio);
                                                       }
                                                  }
                                             }else{
                                                  add_global_tag(TAG_ENTANGLED_BLOCK_HELD_DOWN_UNMOVABLE);
                                             }
                                        }
                                   }
                              }
                         }
                    }
               }

               BlockPushes_t<128> all_block_pushes; // TODO: is 128 this enough ?
               BlockPushes_t<128> all_consolidated_block_pushes;

               // TODO: maybe one day make this happen only when we load the map
               CheckBlockCollisions_t collision_results;
               collision_results.init(world.blocks.count);

               // before collision, track the pos delta
               S16 update_blocks_count = world.blocks.count;
               for(S16 i = 0; i < update_blocks_count; i++){
                    auto block = world.blocks.elements + i;
                    block->pre_collision_pos_delta = block->pos_delta;
               }

               // unbounded collision: this should be exciting
               // we have our initial position and our initial pos_delta, update pos_delta for all players and blocks until nothing is colliding anymore
               const S8 max_collision_attempts = 16;
               bool repeat_collision_pass = true;
               while(repeat_collision_pass && collision_attempts <= max_collision_attempts){
                    repeat_collision_pass = false;

                    // do a collision pass on blocks against the world
                    for(S16 i = 0; i < update_blocks_count; i++){
                         auto block = world.blocks.elements + i;
                         block->pre_collision_pos_delta = block->pos_delta;
                         auto block_collision_result = do_block_collision(&world, block, update_blocks_count);
                         if(block_collision_result.repeat_collision_pass){
                             repeat_collision_pass = true;
                         }
                         update_blocks_count = block_collision_result.update_blocks_count;
                    }

                    // do a collision pass on blocks against other blocks
                    for(S16 i = 0; i < update_blocks_count; i++){
                         auto* block = world.blocks.elements + i;
                         auto collision_result = check_block_collision(&world, block);
                         // add collisions that we need to resolve
                         if(collision_result.collided){
                              if(block->pos_delta.x == 0){
                                   block->collision_time_ratio.x = 0;
                              }else{
                                   block->collision_time_ratio.x = collision_result.pos_delta.x / block->pre_collision_pos_delta.x;
                              }

                              if(block->pos_delta.y == 0){
                                   block->collision_time_ratio.y = 0;
                              }else{
                                   block->collision_time_ratio.y = collision_result.pos_delta.y / block->pre_collision_pos_delta.y;
                              }

                              collision_results.add_collision(&collision_result);
                         }
                    }

                    // on any collision repeat
                    if(collision_results.count > 0) repeat_collision_pass = true;

                    collision_results.sort_by_time();

                    // update block positions based on collisions
                    for(S32 i = 0; i < collision_results.count; i++){
                        auto* collision = collision_results.collisions + i;
                        apply_block_collision(&world, world.blocks.elements + collision->block_index, dt, collision);
                    }

#if 0
                    if(collision_results.count > 0){
                         LOG("collision_result: %d\n", collision_results.count);
                         for(S32 i = 0; i < collision_results.count; i++){
                             auto* collision = collision_results.collisions + i;
                             log_collision_result(collision, &world);
                         }
                    }
#endif

                    // generate pushes based on collisions
                    for(S32 i = 0; i < collision_results.count; i++){
                        auto* collision = collision_results.collisions + i;
                        generate_pushes_from_collision(&world, collision, &all_block_pushes);
                    }

                    collision_results.reset();

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
                                        add_global_tag(TAG_BLOCK_EXTINGUISHED_BY_STOMP);
                                   }else if(check_block->element == ELEMENT_ICE){
                                        check_block->element = ELEMENT_ONLY_ICED;
                                        add_global_tag(TAG_BLOCK_EXTINGUISHED_BY_STOMP);
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
                              add_global_tag(TAG_TELEPORT_BLOCK);
                              block->teleport = true;
                              block->teleport_pos = teleport_result.results[block->clone_id].pos;

                              // LOG("block %d at %d, %d - %.10f, %.10f teleports to %d, %d - %.10f, %.10f\n",
                              //     i, block->pos.pixel.x, block->pos.pixel.y, block->pos.decimal.x, block->pos.decimal.y,
                              //     block->teleport_pos.pixel.x, block->teleport_pos.pixel.y, block->teleport_pos.decimal.x, block->teleport_pos.decimal.y);

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

                              // update any stuck offsets
                              for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
                                   Arrow_t* arrow = world.arrows.arrows + a;
                                   if(!arrow->alive) continue;
                                   if(arrow->stuck_type == STUCK_BLOCK && arrow->stuck_index == i){
                                        arrow->face = direction_rotate_clockwise(arrow->face, teleport_result.results[block->clone_id].rotations);
                                        arrow->stuck_offset = position_rotate_quadrants_clockwise(arrow->stuck_offset, teleport_result.results[block->clone_id].rotations);
                                   }
                              }

                              Interactive_t* src_portal = quad_tree_interactive_find_at(world.interactive_qt, teleport_result.results[block->clone_id].src_portal);
                              Interactive_t* dst_portal = quad_tree_interactive_find_at(world.interactive_qt, teleport_result.results[block->clone_id].dst_portal);
                              if(src_portal && src_portal->type == INTERACTIVE_TYPE_PORTAL &&
                                 dst_portal && dst_portal->type == INTERACTIVE_TYPE_PORTAL){
                                  Direction_t src_portal_dir = src_portal->portal.face;
                                  Direction_t dst_portal_dir = dst_portal->portal.face;

                                  find_and_update_connected_teleported_block(block, direction_opposite(dst_portal_dir), &world);

                                  if(block->connected_teleport.block_index >= 0)
                                  {
                                      auto against_result = block_against_other_blocks(block->teleport_pos + block->teleport_pos_delta,
                                                                                       block->teleport_cut, block->connected_teleport.direction, world.block_qt,
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

                                      for(S16 a = 0; a < against_result.count; a++){
                                          auto* against_other = against_result.objects + a;

                                          if(get_block_index(&world, against_other->block) != block->connected_teleport.block_index) continue;

                                          Vec_t rotated_against_vel = vec_rotate_quadrants_counter_clockwise(against_other->block->vel, against_other->rotations_through_portal);
                                          Vec_t rotated_against_pos_delta = vec_rotate_quadrants_counter_clockwise(against_other->block->pos_delta, against_other->rotations_through_portal);

                                          F32 against_block_vel = 0;
                                          F32 against_block_pos_delta = 0;

                                          if(direction_is_horizontal(block->connected_teleport.direction)){
                                              against_block_vel = rotated_against_vel.x;
                                              against_block_pos_delta = rotated_against_pos_delta.x;
                                          }else{
                                              against_block_vel = rotated_against_vel.y;
                                              against_block_pos_delta = rotated_against_pos_delta.y;
                                          }

                                          if(block_vel != against_block_vel || block_pos_delta != against_block_pos_delta) continue;

                                          switch(block->connected_teleport.direction){
                                          default:
                                              break;
                                          case DIRECTION_LEFT:
                                          {
                                              block->teleport_pos.pixel.x = against_other->block->pos.pixel.x + block_get_width_in_pixels(against_other->block->cut);
                                              block->teleport_pos.decimal.x = against_other->block->pos.decimal.x;
                                              break;
                                          }
                                          case DIRECTION_RIGHT:
                                          {
                                              block->teleport_pos.pixel.x = against_other->block->pos.pixel.x - block_get_width_in_pixels(against_other->block->cut);
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
                                              block->teleport_pos.pixel.y = against_other->block->pos.pixel.y - block_get_height_in_pixels(against_other->block->cut);
                                              block->teleport_pos.decimal.y = against_other->block->pos.decimal.y;
                                              break;
                                          }
                                          }
                                      }

                                      // clear dis
                                      block->connected_teleport.block_index = -1;
                                  }

                                  if(src_portal->portal.wants_to_turn_off && dst_portal->portal.wants_to_turn_off){
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
                                          add_global_tag(TAG_BLOCK_GETS_DESTROYED);
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

                                              if(direction_is_horizontal(src_portal_dir)){
                                                   new_block->stop_on_pixel_x = closest_pixel(new_block->pos.pixel.x, new_block->pos.decimal.x);
                                              }else{
                                                   new_block->stop_on_pixel_y = closest_pixel(new_block->pos.pixel.y, new_block->pos.decimal.y);
                                              }
                                          }

                                          block->teleport_pos.pixel += final_dst_offset;
                                          block->teleport_cut = final_dst_cut;
                                          block->teleport_split = true;

                                          add_global_tag(TAG_BLOCK_GETS_SPLIT);
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

                    // to fight the battle against floating point error, track any blocks we are connected to (going the same speed)
                    for(S16 i = 0; i < world.blocks.count; i++){
                         Block_t* block = world.blocks.elements + i;

                         Vec_t block_pos_delta_vec = block_get_pos_delta(block);

                         if(block_pos_delta_vec.x == 0 && block_pos_delta_vec.y == 0) continue;

                         bool against_any = false;

                         for(S16 d = 0; d < DIRECTION_COUNT; d++){
                              auto direction = static_cast<Direction_t>(d);

                              against_any |= find_and_update_connected_teleported_block(block, direction, &world);
                         }

                         if(!against_any) block->connected_teleport.block_index = -1;
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
                                                  add_global_tag(TAG_PLAYER_GETS_ENTANGLED);

                                                  if(resize(&world.players, world.players.count + (S16)(1))){
                                                       // a resize will kill our player ptr, so we gotta update it
                                                       player = world.players.elements + i;
                                                       player->clone_start = portal->coord;

                                                       Player_t* new_player = world.players.elements + new_player_index;
                                                       *new_player = *player;
                                                       new_player->clone_id = clone_id;
                                                  }

                                                  if(world.players.count > 2){
                                                       add_global_tag(TAG_THREE_PLUS_PLAYERS_ENTANGLED);
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
                                   auto* src_portal = quad_tree_find_at(world.interactive_qt, player->clone_start.x, player->clone_start.y);
                                   if(is_active_portal(src_portal)){
                                        activate(&world, player->clone_start);
                                        src_portal->portal.on = false;
                                   }
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
                         for(S8 e = 0; e < result.entries.count; e++){
                              auto& entry = result.entries.objects[e];
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
                              add_global_tag(TAG_TELEPORT_PLAYER);

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

               // If the final block in an ice chain, is entangled then create entangled pushes for it
               S16 original_all_block_pushes_count = all_block_pushes.count;
               for(S16 i = 0; i < original_all_block_pushes_count; i++){
                    add_entangle_pushes_for_end_of_chain_blocks_on_ice(&world, i, &all_block_pushes);
               }

               // add entangled pushes
               original_all_block_pushes_count = all_block_pushes.count;
               for(S16 i = 0; i < original_all_block_pushes_count; i++){
                    auto& block_push = all_block_pushes.pushes[i];
                    if(block_push.invalidated) continue;
                    if(block_push.no_entangled_pushes) continue;

                    // if the block being pushed is entangled, add block pushes for those
                    Block_t* pushee = world.blocks.elements + block_push.pushee_index;

                    if(pushee->entangle_index < 0) continue;
                    if(block_push.is_entangled()) continue;

                    // TODO: the logic around the use of this bool could apply when only one of the pushers is entangled, not sure what the logic would be there
                    bool entangled_with_all_pushers = true;
                    for(S16 p = 0; p < block_push.pusher_count; p++){
                        Block_t* pusher = world.blocks.elements + block_push.pushers[p].index;
                        entangled_with_all_pushers &= blocks_are_entangled(pushee, pusher, &world.blocks);
                    }

                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                        Direction_t direction = static_cast<Direction_t>(d);
                        if(!direction_in_mask(block_push.direction_mask, direction)) continue;

                        S8 block_push_rotations = (block_push.portal_rotations + block_push.entangle_rotations) % DIRECTION_COUNT;
                        Direction_t push_rotated_direction = direction_rotate_clockwise(direction, block_push_rotations);

                        if(!block_pushable(pushee, push_rotated_direction, &world, block_push.force)) continue;

                        if(entangled_with_all_pushers &&
                           block_on_frictionless(pushee, &world.tilemap, world.interactive_qt, world.block_qt)){
                             auto total_pusher_momentum = get_block_push_pusher_momentum(&block_push, &world, direction);
                             auto pushee_momentum = get_block_momentum(&world, pushee, push_rotated_direction);
                             auto rotated_pushee_vel = rotate_vec_counter_clockwise_to_see_if_negates(pushee_momentum.vel, direction_is_horizontal(push_rotated_direction), block_push_rotations);

                              // if the push isn't going to cause it to start moving in that direction, don't add the entangle pushes
                             auto elastic_result = elastic_transfer_momentum(total_pusher_momentum.mass, total_pusher_momentum.vel, pushee_momentum.mass, rotated_pushee_vel);
                             if(elastic_result.second_final_velocity == 0) continue;

                             block_push.opposite_entangle_reversed = ((rotated_pushee_vel < 0 && elastic_result.second_final_velocity > 0) ||
                                                                      (rotated_pushee_vel > 0 && elastic_result.second_final_velocity < 0));

                             S16 pushee_momentum_mass_split = pushee_momentum.mass / block_push.pusher_count;
                             for(S16 p = 0; p < block_push.pusher_count; p++){
                                  auto* push = block_push.pushers + p;
                                  Block_t* pusher = world.blocks.elements + push->index;
                                  S8 push_rotations = (push->portal_rotations + push->entangle_rotations) % DIRECTION_COUNT;
                                  auto rotated_direction = direction_rotate_clockwise(direction, push_rotations);
                                  rotated_pushee_vel = rotate_vec_counter_clockwise_to_see_if_negates(pushee_momentum.vel, direction_is_horizontal(rotated_direction), DIRECTION_COUNT - (block_push_rotations));
                                  auto pusher_momentum = get_block_momentum(&world, pusher, rotated_direction);

                                  elastic_result = elastic_transfer_momentum(pusher_momentum.mass, pusher_momentum.vel, pushee_momentum_mass_split, rotated_pushee_vel);

                                  push->opposite_entangle_reversed = ((pusher_momentum.vel < 0 && elastic_result.first_final_velocity > 0) ||
                                                                      (pusher_momentum.vel > 0 && elastic_result.first_final_velocity < 0));
                             }
                        }

                        if(!block_push.opposite_entangle_reversed){
                             S16 current_entangle_index = pushee->entangle_index;
                             while(current_entangle_index != block_push.pushee_index && current_entangle_index >= 0){
                                 Block_t* entangler = world.blocks.elements + current_entangle_index;
                                 BlockPush_t new_block_push = block_push;
                                 S8 rotations_between_blocks = blocks_rotations_between(entangler, pushee);
                                 new_block_push.direction_mask = direction_to_direction_mask(direction);
                                 new_block_push.pushee_index = current_entangle_index;
                                 new_block_push.portal_rotations = block_push.portal_rotations;
                                 new_block_push.entangle_rotations = rotations_between_blocks;
                                 new_block_push.entangled_with_push_index = i;
                                 all_block_pushes.add(&new_block_push);

                                 add_entangle_pushes_for_end_of_chain_blocks_on_ice(&world, all_block_pushes.count - 1, &all_block_pushes, DIRECTION_COUNT - rotations_between_blocks);

                                 current_entangle_index = entangler->entangle_index;
                             }
                        }
                    }
               }

               // pass to cause pushes to happen
               {
                    BlockMomentumChanges_t momentum_changes;

                    consolidate_block_pushes(&all_block_pushes, &all_consolidated_block_pushes);

#if 0
                    log_block_pushes(all_consolidated_block_pushes);
#endif
                    execute_block_pushes(&all_consolidated_block_pushes, &world, &momentum_changes);


#if 0
                    log_momentum_changes(&momentum_changes);
#endif

                    // TODO: Loop over momentum changes and build a list of blocks for us to loop over here
                    apply_momentum_changes(&momentum_changes, &world);
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
                         if(i != 0){
                             player->rotation = direction_rotations_between((Direction_t)(player->rotation), (Direction_t)(first_player->rotation));
                         }

                         // set rotations for each direction the player wants to move
                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              if(player_action.move[d]){
                                   player->move_rotation[d] = (player->move_rotation[d] + first_player->rotation) % DIRECTION_COUNT;
                              }
                         }
                    }else{
                         player->pos += player->pos_delta;
                    }

                    auto pos_delta_save = player->pos_delta_save;
                    if(player->teleport){
                         pos_delta_save = vec_rotate_quadrants_clockwise(pos_delta_save, player->teleport_rotation);
                    }

                    // if we are updating player 0, have this move action ignore player rotation as we always clear player 0s rotation anyways.
                    // however, we want to bring it back since future iterations will rely on the first player's rotation. this is really bad
                    // but I can get away with it because of this descriptive comment and being the only programmer on the proj.
                    S8 save_rotation = player->rotation;
                    if(i == 0) player->rotation = 0;

                    build_move_actions_from_player(&player_action, player, move_actions, DIRECTION_COUNT);

                    if(i == 0) player->rotation = save_rotation;

                    auto pure_input_pos_delta = player->pos_delta;
                    pure_input_pos_delta -= player->carried_pos_delta.positive + player->carried_pos_delta.negative;

                    // if we have stopped short in either component, kill the movement for that component if we are no longer pressing keys for it
                    if(fabs(pure_input_pos_delta.x) < fabs(pos_delta_save.x)){
                         if((pure_input_pos_delta.x < 0 && !move_actions[DIRECTION_LEFT]) ||
                            (pure_input_pos_delta.x > 0 && !move_actions[DIRECTION_RIGHT])){
                              player->vel.x = 0;
                              player->accel.x = 0;
                         }
                    }

                    if(fabs(pure_input_pos_delta.y) < fabs(pos_delta_save.y)){
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
                              Position_t diff = block->pos - holder->pos;
                              S16 pixel_diff = closest_pixel(diff.pixel.x, diff.decimal.x);
                              block->stop_on_pixel_x = holder->stop_on_pixel_x + pixel_diff;
                         }

                         if(block->stop_on_pixel_y == 0 && holder->stop_on_pixel_y != 0){
                              Position_t diff = block->pos - holder->pos;
                              S16 pixel_diff = closest_pixel(diff.pixel.y, diff.decimal.y);
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

                         if((block->teleport_rotation % 2) == 1){
                              bool tmp = block->stopped_by_player_horizontal;
                              block->stopped_by_player_horizontal = block->stopped_by_player_vertical;
                              block->stopped_by_player_vertical = tmp;
                         }

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
               }

               // have player push block
               for(S16 i = 0; i < world.players.count; i++){
                    auto player = world.players.elements + i;
                    if(player->prev_pushing_block >= 0 && player->prev_pushing_block == player->pushing_block){
                         Block_t* block_to_push = world.blocks.elements + player->prev_pushing_block;
                         DirectionMask_t block_move_dir_mask = vec_direction_mask(block_to_push->vel);
                         auto push_block_dir = player->pushing_block_dir;

                         auto total_block_mass = get_block_mass_in_direction(&world, block_to_push, push_block_dir);
                         auto mass_ratio = (F32)(block_get_mass(block_to_push)) / (F32)(total_block_mass);

                         auto expected_final_velocity = get_block_expected_player_push_velocity(&world, block_to_push, mass_ratio);
                         bool already_moving_fast_enough = false;

                         if(direction_in_mask(block_move_dir_mask, push_block_dir)){
                              block_to_push->cur_push_mask = direction_mask_add(block_to_push->cur_push_mask, push_block_dir);

                              switch(push_block_dir){
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
                         }else if(block_move_dir_mask == DIRECTION_MASK_NONE){
                              DirectionMask_t block_prev_move_dir_mask = vec_direction_mask(block_to_push->prev_vel);
                              if(direction_in_mask(block_prev_move_dir_mask, push_block_dir)){
                                   player->push_time = 0;
                              }
                         }

                         if(already_moving_fast_enough){
                              // pass
                              player->push_time += dt;
                         }else{
                              F32 save_push_time = player->push_time;

                              // TODO: get back to this once we improve our demo tools
                              player->push_time += dt;
                              if(player->push_time > BLOCK_PUSH_TIME){
                                   // if this is the frame that causes the block to be pushed, make a commit
                                   if(save_push_time <= BLOCK_PUSH_TIME) undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                                   auto allowed_result = allowed_to_push(&world, block_to_push, push_block_dir);
                                   if(allowed_result.push){
                                        auto push_result = block_push(block_to_push, push_block_dir, &world, false, allowed_result.mass_ratio);

                                        if(!push_result.pushed || push_result.busy){
                                             player->push_time = 0.0f;
                                        }else{
                                             player->pushing_block_dir = push_block_dir;
                                             push_entangled_block(block_to_push, &world, push_block_dir, false, allowed_result.mass_ratio);
                                             for(S16 a = 0; a < push_result.againsts_pushed.count; a++){
                                                  push_entangled_block(push_result.againsts_pushed.objects[a].block, &world,
                                                                       push_result.againsts_pushed.objects[a].direction,
                                                                       false, allowed_result.mass_ratio);
                                             }
                                        }

                                        if(block_to_push->pos.z > 0) player->push_time = -0.5f; // TODO: wtf is this line?
                                   }
                              }
                         }
                    }else{
                         player->push_time = 0;
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

                                   auto collision_result = quad_in_quad_high_range_exclusive(&block_quad, &plate_quad);
                                   if(collision_result.inside){
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
                         // TODO: maybe rather than relying on the file system, we can store the starting state in memory ?
                         auto load_result = load_map_number_map(map_number, &world, &undo, &player_start, &player_action, &camera, current_map_tags);
                         if(load_result.success){
                              free(map_number_filepath);
                              map_number_filepath = load_result.filepath;
                         }
                    }
               }else{
                    reset_timer -= dt;
                    if(reset_timer <= 0) reset_timer = 0;
               }
          }

          if((suite && !show_suite) || play_demo.seek_frame >= 0) continue;

          // begin drawing
          Coord_t min = Coord_t{};
          Coord_t max = min + Coord_t{world.tilemap.width, world.tilemap.height};
          min = coord_clamp_zero_to_dim(min, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));
          max = coord_clamp_zero_to_dim(max, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));

          glClear(GL_COLOR_BUFFER_BIT);

          if(game_mode == GAME_MODE_PLAYING || game_mode == GAME_MODE_EDITOR){
               glMatrixMode(GL_PROJECTION);
               glLoadIdentity();
               glOrtho(camera.view.left, camera.view.right, camera.view.bottom, camera.view.top, 0.0, 1.0);

               // draw flats
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               // draw pits in our first pass
               for(S16 y = max.y; y >= min.y; y--){
                    for(S16 x = min.x; x <= max.x; x++){
                         Coord_t coord{x, y};
                         Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, x, y);
                         if(interactive && interactive->type == INTERACTIVE_TYPE_PIT){
                              auto draw_pos = Vec_t{(float)(x) * TILE_SIZE, (float)(y) * TILE_SIZE} + camera.world_offset;
                              draw_interactive(interactive, draw_pos, coord, &world.tilemap, world.interactive_qt);
                         }
                    }
               }

               {
                    glEnd();

                    GLint save_texture;
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &save_texture);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    // draw ice on pits
                    for(S16 y = max.y; y >= min.y; y--){
                         for(S16 x = min.x; x <= max.x; x++){
                              Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, x, y);
                              if(interactive && interactive->type == INTERACTIVE_TYPE_PIT){
                                   auto draw_pos = Vec_t{(float)(x) * TILE_SIZE, (float)(y) * TILE_SIZE} + camera.world_offset;
                                   if(interactive->pit.iced){
                                        draw_pos.y -= (PIXEL_SIZE * HEIGHT_INTERVAL);

                                        Quad_t quad;
                                        quad.left = draw_pos.x;
                                        quad.right = draw_pos.x + TILE_SIZE;
                                        quad.top = draw_pos.y;
                                        quad.bottom = draw_pos.y + TILE_SIZE;

                                        draw_color_quad(quad, 196.0f / 255.0f, 217.0f / 255.0f, 1.0f, 0.45f);
                                        draw_color_quad(quad, 0.0f, 0.0f, 0.0f, 0.3f);
                                   }
                              }
                         }
                    }

                    glBindTexture(GL_TEXTURE_2D, save_texture);
                    glBegin(GL_QUADS);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
               }

               for(S16 y = max.y; y >= min.y; y--){
                    draw_world_row_flats(y, min.x, max.x, &world.tilemap, world.interactive_qt, camera.world_offset);

                    // TODO: compress with logic in draw_world_row_solids()
                    auto search_rect = Rect_t{(S16)(min.x * TILE_SIZE_IN_PIXELS), (S16)(y * TILE_SIZE_IN_PIXELS),
                                              (S16)((max.x * TILE_SIZE_IN_PIXELS) + TILE_SIZE_IN_PIXELS),
                                              (S16)((y * TILE_SIZE_IN_PIXELS) + TILE_SIZE_IN_PIXELS)};

                    S16 block_count = 0;
                    Block_t* blocks[256]; // TODO: oh god i hope we don't need more than that?
                    quad_tree_find_in(world.block_qt, search_rect, blocks, &block_count, 256);

                    sort_blocks_by_descending_height(blocks, block_count);

                    for(S16 i = 0; i < block_count; i++){
                         auto block = blocks[i];
                         if(block->pos.z >= 0) continue;

                         auto draw_block_pos = block->pos;
                         draw_block_pos.pixel.y += block->pos.z;
                         draw_block(block, pos_to_vec(draw_block_pos) + camera.world_offset, 0);
                    }
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

                                        draw_solid_interactive(portal_coord, coord, &world.tilemap, world.interactive_qt, camera.world_offset);

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
                                             draw_portal_blocks(blocks, block_count, portal_coord, coord, portal_rotations, camera.world_offset);
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
                                        draw_portal_players(&world.players, player_region, portal_coord, coord, portal_rotations, camera.world_offset);

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
                              Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera.world_offset.x,
                                              (F32)(y - min.y) * TILE_SIZE + camera.world_offset.y};
                              draw_tile_id(tile->id, tile_pos);
                         }
                    }
               }
               for(S16 y = max.y; y >= min.y; y--){
                    draw_world_row_solids(y, min.x, max.x, &world.tilemap, world.interactive_qt, world.block_qt,
                                          &world.players, camera.world_offset, player_texture);

                    glEnd();
                    glBindTexture(GL_TEXTURE_2D, arrow_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);

                    draw_world_row_arrows(y, min.x, max.x, &world.arrows, camera.world_offset);

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

                         F32 light_value = (F32)(255 - tile->light) / 255.0f;

                         Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera.world_offset.x,
                                         (F32)(y - min.y) * TILE_SIZE + camera.world_offset.y};
                         glColor4f(0.0f, 0.0f, 0.0f, light_value);
                         glVertex2f(tile_pos.x, tile_pos.y);
                         glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                         glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                         glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);
                    }
               }
               glEnd();
#endif

               // before we draw the UI, lets write to the thumbnail buffer
               {
                    glBindFramebuffer(GL_FRAMEBUFFER, thumbnail_framebuffer);

                    glViewport(0, 0, THUMBNAIL_DIMENSION, THUMBNAIL_DIMENSION);

                    glBindTexture(GL_TEXTURE_2D, render_texture);
                    glBegin(GL_QUADS);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

                    glTexCoord2f(0, 0);
                    glVertex2f(0.0f, 0.0f);

                    glTexCoord2f(0, 1.0f);
                    glVertex2f(0.0f, 1.0f);

                    glTexCoord2f(1.0f, 1.0f);
                    glVertex2f(1.0f, 1.0f);

                    glTexCoord2f(1.0f, 0);
                    glVertex2f(1.0f, 0.0f);

                    glEnd();

                    glViewport(0, 0, window_width, window_height);

                    glBindFramebuffer(GL_FRAMEBUFFER, render_framebuffer);
               }
          }

          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          glOrtho(0.0f, 1.0f, 0.0f, 1.0f, 0.0, 1.0);

          if(game_mode == GAME_MODE_LEVEL_SELECT){
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
               for(S16 c = 0; c < tag_checkboxes.count; c++){
                    Checkbox_t* checkbox = tag_checkboxes.elements + c;
                    Vec_t final_pos = checkbox->pos + checkbox_scroll;
                    if(final_pos.y < -CHECKBOX_DIMENSION || final_pos.y > 1.0f) continue;
                    draw_checkbox(checkbox, checkbox_scroll);
               }
               glEnd();

               {
                    glBindTexture(GL_TEXTURE_2D, text_texture);
                    glBegin(GL_QUADS);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

                    Vec_t text_pos {CHECKBOX_START_OFFSET_X + 10.0f * PIXEL_SIZE, 2.0f * CHECKBOX_START_OFFSET_Y};
                    text_pos += checkbox_scroll;
                    for(S16 c = 0; c < tag_checkboxes.count; c++){
                         if(text_pos.y < 0 || text_pos.y > 1){
                              // pass
                         }else{
                              const char* text = NULL;
                              if(c == 0){
                                   text = "EXCLUSIVE";
                              }else{
                                   text = tag_to_string((Tag_t)(c - 1));
                              }
                              draw_text(text, text_pos, Vec_t{TEXT_CHAR_WIDTH * 0.5f, TEXT_CHAR_HEIGHT * 0.5f},
                                        TEXT_CHAR_SPACING * 0.5f);
                         }
                         text_pos.y += CHECKBOX_INTERVAL;
                    }

                    if(hovered_map_thumbnail_path){
                         text_pos = Vec_t{CHECKBOX_THUMBNAIL_SPLIT, 2.0f * PIXEL_SIZE};
                         draw_text(hovered_map_thumbnail_path, text_pos, Vec_t{TEXT_CHAR_WIDTH * 0.5f, TEXT_CHAR_HEIGHT * 0.5f},
                                        TEXT_CHAR_SPACING * 0.5f);
                    }

                    glEnd();
               }

               for(S16 m = 0; m < map_thumbnails.count; m++){
                    auto* map_thumbnail = map_thumbnails.elements + m;

                    if(map_thumbnail->texture == 0) continue;

                    Vec_t pos = map_thumbnail->pos + map_scroll;
                    Vec_t bounds = pos + Vec_t{THUMBNAIL_UI_DIMENSION, THUMBNAIL_UI_DIMENSION};

                    if(pos.y < 0 || pos.y > 1.0f ) continue;

                    glBindTexture(GL_TEXTURE_2D, map_thumbnail->texture);
                    glBegin(GL_QUADS);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

                    glTexCoord2f(0, 0);
                    glVertex2f(pos.x, pos.y);

                    glTexCoord2f(0, 1.0f);
                    glVertex2f(pos.x, bounds.y);

                    glTexCoord2f(1.0f, 1.0f);
                    glVertex2f(bounds.x, bounds.y);

                    glTexCoord2f(1.0f, 0);
                    glVertex2f(bounds.x, pos.y);

                    glEnd();
               }
          }

          if(game_mode == GAME_MODE_EDITOR){
               // player start
               draw_selection(player_start, player_start, &camera, 0.0f, 1.0f, 0.0f);

               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    if(block->entangle_index < 0) continue;

                    Block_t* entangled_block = world.blocks.elements + block->entangle_index;

                    if((blocks_rotations_between(block, entangled_block) % 2) == 0) continue;

                    CentroidStart_t a;
                    a.coord = block_get_coord(block);
                    a.direction = static_cast<Direction_t>((block->rotation + 3) % DIRECTION_COUNT);

                    CentroidStart_t b;
                    b.coord = block_get_coord(entangled_block);
                    b.direction = static_cast<Direction_t>((entangled_block->rotation + 3) % DIRECTION_COUNT);

                    auto centroid = find_centroid(a, b);
                    draw_selection(centroid, centroid, &camera, 0.0f, 0.0f, 1.0f);
               }

               // editor
               draw_editor(&editor, &world, &camera, mouse_screen, theme_texture, text_texture);
          }

          if(reset_timer >= 0.0f){
               glBegin(GL_QUADS);
               glColor4f(0.0f, 0.0f, 0.0f, reset_timer / RESET_TIME);
               glVertex2f(0, 0);
               glVertex2f(0, 1);
               glVertex2f(1, 1);
               glVertex2f(1, 0);
               glEnd();
          }

          if(game_mode == GAME_MODE_PLAYING || game_mode == GAME_MODE_EDITOR){
               if(play_demo.mode == DEMO_MODE_PLAY){
                    F32 demo_pct = (F32)(frame_count) / (F32)(play_demo.last_frame);
                    Quad_t pct_bar_quad = {pct_bar_outline_quad.left, pct_bar_outline_quad.bottom, demo_pct, pct_bar_outline_quad.top};
                    glBindTexture(GL_TEXTURE_2D, 0);
                    draw_quad_filled(&pct_bar_quad, 255.0f, 255.0f, 255.0f);
                    draw_quad_wireframe(&pct_bar_outline_quad, 255.0f, 255.0f, 255.0f);

                    char buffer[64];
                    snprintf(buffer, 64, "F: %" PRId64 "/%" PRId64 " C: %d", frame_count, play_demo.last_frame, collision_attempts);

                    glBindTexture(GL_TEXTURE_2D, text_texture);
                    glBegin(GL_QUADS);

                    Vec_t text_pos {0.005f, 0.965f};

                    if(game_mode == GAME_MODE_EDITOR) text_pos.y -= 0.09f;

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
          }

          glBindFramebuffer(GL_FRAMEBUFFER, 0);

          glBindTexture(GL_TEXTURE_2D, render_texture);
          glBegin(GL_QUADS);
          glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

          glTexCoord2f(0, 0);
          glVertex2f(0.0f, 0.0f);

          glTexCoord2f(0, 1.0f);
          glVertex2f(0.0f, 1.0f);

          glTexCoord2f(1.0f, 1.0f);
          glVertex2f(1.0f, 1.0f);

          glTexCoord2f(1.0f, 0);
          glVertex2f(1.0f, 0.0f);

          glEnd();

          SDL_GL_SwapWindow(window);

          glBindFramebuffer(GL_FRAMEBUFFER, render_framebuffer);
     }

     if(play_demo.mode == DEMO_MODE_PLAY){
          fclose(play_demo.file);
     }

     if(record_demo.mode == DEMO_MODE_RECORD){
          player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_END_DEMO, record_demo.mode, record_demo.file, frame_count);

          // save map and player position
          save_map_to_file(record_demo.file, player_start, &world.tilemap, &world.blocks, &world.interactives, NULL, NULL);

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
          glDeleteTextures(1, &render_texture);
          glDeleteTextures(1, &thumbnail_texture);

          glDeleteFramebuffers(1, &render_framebuffer);
          glDeleteFramebuffers(1, &thumbnail_framebuffer);

          SDL_GL_DeleteContext(opengl_context);
          SDL_DestroyWindow(window);
          SDL_Quit();
     }

     Log_t::destroy();
     return 0;
}
