#include "world.h"
#include "defines.h"
#include "conversion.h"
#include "map_format.h"
#include "portal_exit.h"
#include "utils.h"
#include "collision.h"
#include "block_utils.h"
#include "tags.h"

// linux
#include <dirent.h>
#include <float.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

struct PitFilledIn_t{
     bool bottom_left_filled_in = false;
     bool bottom_right_filled_in = false;
     bool top_left_filled_in = false;
     bool top_right_filled_in = false;

     Rect_t bottom_left_corner;
     Rect_t bottom_right_corner;
     Rect_t top_left_corner;
     Rect_t top_right_corner;
};

static PitFilledIn_t calculate_pit_area_filled_in(Coord_t coord, QuadTreeNode_t<Block_t>* block_qt){
     PitFilledIn_t result {};

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     Rect_t pit_rect = rect_surrounding_coord(coord);
     quad_tree_find_in(block_qt, pit_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     result.top_left_corner = Rect_t{pit_rect.left, (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS), (S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS - 1), pit_rect.top};
     result.top_right_corner = Rect_t{(S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS), (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS), pit_rect.right, pit_rect.top};
     result.bottom_left_corner = Rect_t{pit_rect.left, pit_rect.bottom, (S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS - 1), (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS - 1)};
     result.bottom_right_corner = Rect_t{(S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS), pit_rect.bottom, pit_rect.right, (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS - 1)};

     for(S16 b = 0; b < block_count; b++){
          if(blocks[b]->pos.z > -HEIGHT_INTERVAL || blocks[b]->pos.z > 0) continue;

          auto block_rect = block_get_inclusive_rect(blocks[b]);

          if(!rect_completely_in_rect(block_rect, pit_rect)) continue;

          if(rect_in_rect(result.top_left_corner, block_rect)){
              result.top_left_filled_in = true;
          }

          if(rect_in_rect(result.top_right_corner, block_rect)){
              result.top_right_filled_in = true;
          }

          if(rect_in_rect(result.bottom_left_corner, block_rect)){
              result.bottom_left_filled_in = true;
          }

          if(rect_in_rect(result.bottom_right_corner, block_rect)){
              result.bottom_right_filled_in = true;
          }
     }

     return result;
}

static int ascending_block_height_comparer(const void* a, const void* b){
    Block_t* real_a = *(Block_t**)(a);
    Block_t* real_b = *(Block_t**)(b);

    return real_a->pos.z < real_b->pos.z;
}

static int descending_block_height_comparer(const void* a, const void* b){
    Block_t* real_a = *(Block_t**)(a);
    Block_t* real_b = *(Block_t**)(b);

    return real_a->pos.z > real_b->pos.z;
}


void sort_blocks_by_ascending_height(Block_t** blocks, S16 block_count){
    qsort(blocks, block_count, sizeof(*blocks), ascending_block_height_comparer);
}

void sort_blocks_by_descending_height(Block_t** blocks, S16 block_count){
    qsort(blocks, block_count, sizeof(*blocks), descending_block_height_comparer);
}

LogMapNumberResult_t load_map_number(S32 map_number, Coord_t* player_start, World_t* world){
     LogMapNumberResult_t result;

     // search through directory to find file starting with 3 digit map number
     DIR* d = opendir("content");
     if(!d){
         LOG("load_map_number(): opendir() failed: %s\n", strerror(errno));
         return result;
     }
     struct dirent* dir;
     char filepath[512] = {};
     char match[4] = {};
     snprintf(match, 4, "%03d", map_number);
     while((dir = readdir(d)) != nullptr){
          if(strncmp(dir->d_name, match, 3) == 0 &&
             strstr(dir->d_name, ".bm")){ // TODO: create strendswith() func for this?
               snprintf(filepath, 512, "content/%s", dir->d_name);
               break;
          }
     }

     closedir(d);

     if(!filepath[0]) return result;

     result.success = load_map(filepath, player_start, &world->tilemap, &world->blocks, &world->interactives,
                               &world->rooms);
     if(result.success) result.filepath = strdup(filepath);
     return result;
}

Position_t center_camera(TileMap_t* tilemap){
     return coord_to_pos(Coord_t{(S16)(tilemap->width / 2), (S16)(tilemap->height / 2)});
}

void reset_map(Coord_t player_start, World_t* world, Undo_t* undo, Camera_t* camera){
     destroy(&world->players);
     init(&world->players, 1);
     Player_t* player = world->players.elements;
     *player = {};
     player->walk_frame_delta = 1;
     player->pos = coord_to_pos_at_tile_center(player_start);
     player->has_bow = true;

     init(&world->arrows);

     quad_tree_free(world->interactive_qt);
     world->interactive_qt = quad_tree_build(&world->interactives);

     quad_tree_free(world->block_qt);
     world->block_qt = quad_tree_build(&world->blocks);

     destroy(undo);
     init(undo, UNDO_MEMORY, world->tilemap.width, world->tilemap.height, world->blocks.count, world->interactives.count);
     undo_snapshot(undo, &world->players, &world->tilemap, &world->blocks, &world->interactives);

     camera->center_on_tilemap(&world->tilemap);
}

static void toggle_electricity(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_qt, Coord_t coord,
                               Direction_t direction, bool from_wire, bool activated_by_door){
     Coord_t adjacent_coord = coord + direction;
     Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
     if(!tile) return;

     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_qt, adjacent_coord);
     if(interactive){
          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_POPUP:
          {
               interactive->popup.lift.up = !interactive->popup.lift.up;
               if(tile->flags & TILE_FLAG_ICED){
                    tile->flags &= ~TILE_FLAG_ICED;
               }
          } break;
          case INTERACTIVE_TYPE_DOOR:
               interactive->door.lift.up = !interactive->door.lift.up;
               // open connecting door
               if(!activated_by_door) toggle_electricity(tilemap, interactive_qt,
                                                         coord_move(coord, interactive->door.face, 3),
                                                         interactive->door.face, from_wire, true);
               break;
          case INTERACTIVE_TYPE_PORTAL:
               if(from_wire){
                   if(interactive->portal.has_block_inside){
                       interactive->portal.wants_to_turn_off = true;
                   }else{
                       interactive->portal.on = !interactive->portal.on;
                   }
               }else{
                    PortalExit_t portal_exits = find_portal_exits(adjacent_coord, tilemap, interactive_qt);
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         Direction_t current_portal_dir = (Direction_t)(d);
                         auto portal_exit = portal_exits.directions + d;

                         for(S8 p = 0; p < portal_exit->count; p++){
                              auto portal_dst_coord = portal_exit->coords[p];
                              if(portal_dst_coord == adjacent_coord) continue;

                              toggle_electricity(tilemap, interactive_qt, portal_dst_coord, direction_opposite(current_portal_dir), from_wire, false);
                         }
                    }
               }
               break;
          }
     }

     if((tile->flags & (TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN)) ||
        (interactive && interactive->type == INTERACTIVE_TYPE_WIRE_CROSS &&
         interactive->wire_cross.mask & (TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN))){
          bool wire_cross = false;

          if(interactive && interactive->type == INTERACTIVE_TYPE_WIRE_CROSS){
               switch(direction){
               default:
                    return;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_RIGHT){
                         TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
                    }else if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT){
                         interactive->wire_cross.on = !interactive->wire_cross.on;
                         wire_cross = true;
                    }else{
                         return;
                    }
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_LEFT){
                         TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
                    }else if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT){
                         interactive->wire_cross.on = !interactive->wire_cross.on;
                         wire_cross = true;
                    }else{
                         return;
                    }
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_DOWN){
                         TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
                    }else if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN){
                         interactive->wire_cross.on = !interactive->wire_cross.on;
                         wire_cross = true;
                    }else{
                         return;
                    }
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_UP){
                         TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
                    }else if(interactive->wire_cross.mask & DIRECTION_MASK_UP){
                         interactive->wire_cross.on = !interactive->wire_cross.on;
                         wire_cross = true;
                    }else{
                         return;
                    }
                    break;
               }
          }else{
               switch(direction){
               default:
                    return;
               case DIRECTION_LEFT:
                    if(!(tile->flags & TILE_FLAG_WIRE_RIGHT)){
                         return;
                    }
                    break;
               case DIRECTION_RIGHT:
                    if(!(tile->flags & TILE_FLAG_WIRE_LEFT)){
                         return;
                    }
                    break;
               case DIRECTION_UP:
                    if(!(tile->flags & TILE_FLAG_WIRE_DOWN)){
                         return;
                    }
                    break;
               case DIRECTION_DOWN:
                    if(!(tile->flags & TILE_FLAG_WIRE_UP)){
                         return;
                    }
                    break;
               }

               // toggle wire state
               TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
          }

          if(wire_cross){
               if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT && direction != DIRECTION_RIGHT){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_LEFT, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT && direction != DIRECTION_LEFT){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_RIGHT, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN && direction != DIRECTION_UP){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_DOWN, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_UP && direction != DIRECTION_DOWN){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_UP, true, false);
               }
          }else{
               if(tile->flags & TILE_FLAG_WIRE_LEFT && direction != DIRECTION_RIGHT){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_LEFT, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_RIGHT && direction != DIRECTION_LEFT){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_RIGHT, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_DOWN && direction != DIRECTION_UP){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_DOWN, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_UP && direction != DIRECTION_DOWN){
                    toggle_electricity(tilemap, interactive_qt, adjacent_coord, DIRECTION_UP, true, false);
               }
          }
     }else if(tile->flags & (TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT)){
          bool all_on_before = tile_flags_cluster_all_on(tile->flags);

          Direction_t cluster_direction = tile_flags_cluster_direction(tile->flags);
          switch(cluster_direction){
          default:
               break;
          case DIRECTION_LEFT:
               switch(direction){
               default:
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_RIGHT:
               switch(direction){
               default:
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_DOWN:
               switch(direction){
               default:
                    break;
               case DIRECTION_DOWN:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          case DIRECTION_UP:
               switch(direction){
               default:
                    break;
               case DIRECTION_UP:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                    break;
               case DIRECTION_RIGHT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                    break;
               case DIRECTION_LEFT:
                    if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                    break;
               }
               break;
          }

          bool all_on_after = tile_flags_cluster_all_on(tile->flags);

          if(all_on_before != all_on_after){
               toggle_electricity(tilemap, interactive_qt, adjacent_coord, cluster_direction, true, false);
          }
     }
}

void activate(World_t* world, Coord_t coord){
     Interactive_t* interactive = quad_tree_interactive_find_at(world->interactive_qt, coord);
     if(!interactive) return;

     if(interactive->type != INTERACTIVE_TYPE_LEVER &&
        interactive->type != INTERACTIVE_TYPE_PRESSURE_PLATE &&
        interactive->type != INTERACTIVE_TYPE_LIGHT_DETECTOR &&
        interactive->type != INTERACTIVE_TYPE_ICE_DETECTOR &&
        interactive->type != INTERACTIVE_TYPE_PORTAL) return;

     toggle_electricity(&world->tilemap, world->interactive_qt, coord, DIRECTION_LEFT, false, false);
     toggle_electricity(&world->tilemap, world->interactive_qt, coord, DIRECTION_RIGHT, false, false);
     toggle_electricity(&world->tilemap, world->interactive_qt, coord, DIRECTION_UP, false, false);
     toggle_electricity(&world->tilemap, world->interactive_qt, coord, DIRECTION_DOWN, false, false);
}

void slow_block_toward_gridlock(World_t* world, Block_t* block, Direction_t direction){
     if(!block_on_frictionless(block->pos, block->pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt)) return;

     Move_t* move = direction_is_horizontal(direction) ? &block->horizontal_move : &block->vertical_move;

     if(move->state == MOVE_STATE_STOPPING || move->state == MOVE_STATE_IDLING) return;

     move->state = MOVE_STATE_STOPPING;
     move->distance = 0;

     auto block_mass = get_block_stack_mass(world, block);
     F32 block_vel = 0;

     add_global_tag(TAB_PLAYER_STOPS_COASTING_BLOCK);

     if(direction_is_horizontal(direction)){
          block->stopped_by_player_horizontal = true;
          auto motion = motion_x_component(block);
          block->accel.x = begin_stopping_grid_aligned_motion(block->cut, &motion, block->pos.pixel.x, block->pos.decimal.x, block_mass); // adjust by one tick since we missed this update
          block_vel = block->vel.x;
     }else{
          block->stopped_by_player_vertical = true;
          auto motion = motion_y_component(block);
          block->accel.y = begin_stopping_grid_aligned_motion(block->cut, &motion, block->pos.pixel.y, block->pos.decimal.y, block_mass); // adjust by one tick since we missed this update
          block_vel = block->vel.y;
     }

     auto against_result = block_against_other_blocks(block->pos + block->pos_delta, block->cut, direction_opposite(direction),
                                                      world->block_qt, world->interactive_qt, &world->tilemap);
     for(S16 i = 0; i < against_result.count; i++){
         Direction_t against_result_direction = direction_rotate_clockwise(direction, against_result.objects[i].rotations_through_portal);
         Block_t* against_result_block = against_result.objects[i].block;

         // we don't want to impact a block that is colliding with the block as we are stopping it, but rather a
         // gaggle of blocks sliding together that the player is stopping
         // TODO: Seems like we'll want to rotated the block_vel based on the against rotations...
         if(direction_is_horizontal(against_result_direction)){
             if(fabs(block_vel - against_result_block->vel.x) > FLT_EPSILON) continue;
         }else{
             if(fabs(block_vel - against_result_block->vel.y) > FLT_EPSILON) continue;
         }

         slow_block_toward_gridlock(world, against_result_block, against_result_direction);
     }
}

Block_t* player_against_block(Player_t* player, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt,
                              QuadTreeNode_t<Interactive_t>* interactive_qt, TileMap_t* tilemap){
     auto player_coord = pos_to_coord(player->pos);
     auto check_rect = rect_surrounding_adjacent_coords(player_coord);

     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, check_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          Rect_t block_rect = block_get_inclusive_rect(block);

          if(pixel_in_rect(pos_a.pixel, block_rect) || pixel_in_rect(pos_b.pixel, block_rect)){
               return block;
          }
     }

     auto found_blocks = find_blocks_through_portals(player_coord, tilemap, interactive_qt, block_qt);
     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;

         if(!block_in_height_range_of_player(found_block->block, player->pos)) continue;

         Rect_t block_rect = block_get_inclusive_rect(found_block->position.pixel, found_block->rotated_cut);

         if(pixel_in_rect(pos_a.pixel, block_rect) || pixel_in_rect(pos_b.pixel, block_rect)){
              return found_block->block;
         }
     }

     return nullptr;
}

bool player_against_solid_tile(Player_t* player, Direction_t direction, TileMap_t* tilemap){
     Position_t pos = get_player_adjacent_position(player, direction);

     return tilemap_is_solid(tilemap, pixel_to_coord(pos.pixel));
}

bool player_against_solid_interactive(Player_t* player, Direction_t direction, QuadTreeNode_t<Interactive_t>* interactive_qt, QuadTreeNode_t<Block_t>* block_qt){
     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     Coord_t coord = pixel_to_coord(pos_a.pixel);
     Interactive_t* interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)) return true;
          if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on && player->pos.z > PORTAL_MAX_HEIGHT) return true;
          if(interactive->type == INTERACTIVE_TYPE_PIT){
               auto pit_area_filled_in = calculate_pit_area_filled_in(coord, block_qt);

               if(!pit_area_filled_in.bottom_left_filled_in && pixel_in_rect(pos_a.pixel, pit_area_filled_in.bottom_left_corner)){
                    return true;
               }

               if(!pit_area_filled_in.bottom_right_filled_in && pixel_in_rect(pos_a.pixel, pit_area_filled_in.bottom_right_corner)){
                    return true;
               }

               if(!pit_area_filled_in.top_left_filled_in && pixel_in_rect(pos_a.pixel, pit_area_filled_in.top_left_corner)){
                    return true;
               }

               if(!pit_area_filled_in.top_right_filled_in && pixel_in_rect(pos_a.pixel, pit_area_filled_in.top_right_corner)){
                    return true;
               }
          }
     }

     coord = pixel_to_coord(pos_b.pixel);
     interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)) return true;
          if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on && player->pos.z > PORTAL_MAX_HEIGHT) return true;
          if(interactive->type == INTERACTIVE_TYPE_PIT){
               auto pit_area_filled_in = calculate_pit_area_filled_in(coord, block_qt);

               if(!pit_area_filled_in.bottom_left_filled_in && pixel_in_rect(pos_b.pixel, pit_area_filled_in.bottom_left_corner)){
                    return true;
               }

               if(!pit_area_filled_in.bottom_right_filled_in && pixel_in_rect(pos_b.pixel, pit_area_filled_in.bottom_right_corner)){
                    return true;
               }

               if(!pit_area_filled_in.top_left_filled_in && pixel_in_rect(pos_b.pixel, pit_area_filled_in.top_left_corner)){
                    return true;
               }

               if(!pit_area_filled_in.top_right_filled_in && pixel_in_rect(pos_b.pixel, pit_area_filled_in.top_right_corner)){
                    return true;
               }
          }
     }

     return false;
}

#define MAX_PLAYER_BLOCK_COLLISIONS 8

struct PlayerBlockCollision_t{
     Block_t*    block = nullptr;
     Direction_t dir = DIRECTION_COUNT;
     Position_t  pos;
     bool        through_portal = false;
     U8          portal_rotations = 0;
     F32         distance_to_player = 0;
     Vec_t       player_pos_delta;
     F32         player_pos_delta_mag;
};

struct PlayerBlockCollisions_t{
     PlayerBlockCollision_t collisions[MAX_PLAYER_BLOCK_COLLISIONS];
     S16 count = 0;

     bool add(PlayerBlockCollision_t collision){
          if(count >= MAX_PLAYER_BLOCK_COLLISIONS) return false;

          collisions[count] = collision;
          count++;
          return true;
     }
};

static int collision_distance_comparer(const void* a, const void* b){
    PlayerBlockCollision_t* real_a = *(PlayerBlockCollision_t**)(a);
    PlayerBlockCollision_t* real_b = *(PlayerBlockCollision_t**)(b);

    if(real_a->player_pos_delta_mag == real_b->player_pos_delta_mag){
          return real_a->pos.z > real_b->pos.z;
    }

    return real_a->player_pos_delta_mag < real_b->player_pos_delta_mag;
}

void sort_collisions_by_distance(PlayerBlockCollisions_t* block_collisions){
    qsort(block_collisions->collisions, block_collisions->count, sizeof(*block_collisions->collisions), collision_distance_comparer);
}

void stop_against_blocks_moving_with_block(World_t* world, Block_t* block, Direction_t direction, // Position_t adjusted_stop_pos,
                                           F32 new_pos_delta){
    Vec_t new_pos_delta_vec = vec_zero();
    if(direction_is_horizontal(direction)){
        reset_move(&block->horizontal_move);
        block->vel.x = 0;
        block->accel.x = 0;
        block->stopped_by_player_horizontal = false;
        new_pos_delta_vec.x = new_pos_delta;
    }else{
        reset_move(&block->vertical_move);
        block->vel.y = 0;
        block->accel.y = 0;
        block->stopped_by_player_vertical = false;
        new_pos_delta_vec.y = new_pos_delta;
    }

    auto against_result = block_against_other_blocks(block->pos + block->pos_delta, block->cut, direction,
                                                     world->block_qt, world->interactive_qt, &world->tilemap);
    for(S16 i = 0; i < against_result.count; i++){
        Direction_t against_direction = direction_rotate_clockwise(direction, against_result.objects[i].rotations_through_portal);
        Block_t* against_block = against_result.objects[i].block;

        Vec_t rotated_new_pos_delta_vec = vec_rotate_quadrants_clockwise(new_pos_delta_vec, against_result.objects[i].rotations_through_portal);
        F32 rotated_new_pos_delta = direction_is_horizontal(against_direction) ? rotated_new_pos_delta_vec.x : rotated_new_pos_delta_vec.y;
        stop_against_blocks_moving_with_block(world, against_block, against_direction, rotated_new_pos_delta);
    }

    if(new_pos_delta){
         if(direction_is_horizontal(direction)){
              block->pos_delta.x = new_pos_delta;
         }else{
              block->pos_delta.y = new_pos_delta;
         }
    }
}

MovePlayerThroughWorldResult_t move_player_through_world(Position_t player_pos, Vec_t player_vel, Vec_t player_pos_delta,
                                                         Direction_t player_face, S8 player_clone_instance, S16 player_index,
                                                         S16 player_pushing_block, Direction_t player_pushing_block_dir,
                                                         S8 player_pushing_block_rotation, World_t* world){
     MovePlayerThroughWorldResult_t result = {};

     result.pos_delta = player_pos_delta;
     result.pushing_block = player_pushing_block;
     result.pushing_block_dir = player_pushing_block_dir;
     result.pushing_block_rotation = player_pushing_block_rotation;

     // figure out tiles that are close by
     Position_t final_player_pos = player_pos + result.pos_delta;
     Coord_t player_coord = pos_to_coord(final_player_pos);

     // NOTE: this assumes the player can't move more than 1 coord in 1 frame
     Coord_t min = player_coord - Coord_t{1, 1};
     Coord_t max = player_coord + Coord_t{1, 1};

     min = coord_clamp_zero_to_dim(min, world->tilemap.width - (S16)(1), world->tilemap.height - (S16)(1));
     max = coord_clamp_zero_to_dim(max, world->tilemap.width - (S16)(1), world->tilemap.height - (S16)(1));

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     auto player_check_rect = rect_surrounding_adjacent_coords(player_coord);
     quad_tree_find_in(world->block_qt, player_check_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     sort_blocks_by_ascending_height(blocks, block_count);

     PlayerBlockCollisions_t block_collisions;

     // the reverse loop is so we process higher blocks before lower blocks
     for(S16 i = block_count - 1; i >= 0; i--){
          Block_t* block = blocks[i];

          // check if the block is in our player's height range
          if(!block_in_height_range_of_player(block, player_pos)) continue;

          PlayerBlockCollision_t collision;
          collision.player_pos_delta = result.pos_delta;

          Position_t block_pos = block->teleport ? (block->teleport_pos + block->teleport_pos_delta) : (block->pos + block->pos_delta);
          bool collided = false;
          position_collide_with_rect(player_pos, block_pos,
                                     block_get_width_in_pixels(block) * PIXEL_SIZE,
                                     block_get_height_in_pixels(block) * PIXEL_SIZE,
                                     &collision.player_pos_delta, &collided);
          if(collided){
               result.collided = true;

               collision.block = block;
               collision.pos = block_pos;
               collision.dir = relative_quadrant(player_pos.pixel, block_get_center(block_pos, block->cut).pixel);
               collision.player_pos_delta_mag = vec_magnitude(collision.player_pos_delta);

               block_collisions.add(collision);
          }
     }

     auto found_blocks = find_blocks_through_portals(player_coord, &world->tilemap, world->interactive_qt, world->block_qt);

     for(S16 i = 0; i < found_blocks.count; i++){
         auto* found_block = found_blocks.objects + i;

         if(!block_in_height_range_of_player(found_block->block, player_pos)) continue;

         PlayerBlockCollision_t collision;
         collision.player_pos_delta = result.pos_delta;

         bool collided = false;
         position_collide_with_rect(player_pos, found_block->position,
                                    block_get_width_in_pixels(found_block->rotated_cut) * PIXEL_SIZE,
                                    block_get_height_in_pixels(found_block->rotated_cut) * PIXEL_SIZE,
                                    &collision.player_pos_delta, &collided);

         if(collided){
              result.collided = true;

              collision.through_portal = true;
              collision.block = found_block->block;
              collision.pos = found_block->position;
              collision.dir = relative_quadrant(player_pos.pixel, block_get_center(found_block->position, found_block->rotated_cut).pixel);
              collision.portal_rotations = found_block->portal_rotations;
              collision.player_pos_delta_mag = vec_magnitude(collision.player_pos_delta);

              block_collisions.add(collision);
         }
     }

     sort_collisions_by_distance(&block_collisions);

     bool use_this_collision = false;
     for(S16 i = 0; i < block_collisions.count && !use_this_collision; i++){
          auto collision = block_collisions.collisions[i];

          use_this_collision = true;

          // this stops the block when it moves into the player
          Vec_t rotated_pos_delta = vec_rotate_quadrants_clockwise(collision.block->pos_delta, collision.portal_rotations);
          Vec_t rotated_accel = vec_rotate_quadrants_clockwise(collision.block->accel, collision.portal_rotations);
          MoveState_t relevant_move_state = block_get_move_in_direction(collision.block, collision.dir, collision.portal_rotations).state;

          auto* player = world->players.elements + player_index;

          // TODO: definitely heavily condense this
          bool player_slowing_down = false;

          switch(collision.dir){
          default:
               break;
          case DIRECTION_LEFT:
               if(rotated_pos_delta.x > 0.0f){
                    result.pos_delta.x = 0;

                    Direction_t check_dir = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);

                    bool would_squish = false;
                    Block_t* squished_block = player_against_block(player, check_dir, world->block_qt, world->interactive_qt, &world->tilemap);
                    would_squish = squished_block && squished_block->vel.x < collision.block->vel.x;

                    if(!would_squish){
                         would_squish = player_against_solid_tile(player, check_dir, &world->tilemap);
                    }

                    if(!would_squish){
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt, world->block_qt);
                    }

                    // only squish if the block we would be squished against is moving slower, do we stop the block we collided with
                    auto group_mass = get_block_mass_in_direction(world, collision.block, collision.dir);
                    auto momentum = (F32)group_mass * fabs(collision.block->vel.x);

                    // TODO: handle rotations
                    if(would_squish){
                         if(momentum >= PLAYER_SQUISH_MOMENTUM){
                              result.resetting = true;
                              add_global_tag(TAG_BLOCK_SQUISHES_PLAYER);
                         }else{
                              auto collided_pos_offset = collision.pos - collision.block->pos;
                              auto block_new_pos = collision.pos;
                              block_new_pos.pixel.x--;
                              block_new_pos.decimal.x = 0;
                              block_new_pos -= collided_pos_offset;
                              F32 new_pos_delta = pos_to_vec(block_new_pos - collision.block->pos).x;
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, new_pos_delta);
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         if(relevant_move_state == MOVE_STATE_STARTING && rotated_accel.x < 0){
                              // the player has started pushing the block left while it was coasting right so pass on taking any actions
                         }else{
                              F32 block_width = block_get_width_in_pixels(collision.block) * PIXEL_SIZE;
                              auto new_pos = collision.pos + Vec_t{block_width + PLAYER_RADIUS, 0};
                              result.pos_delta.x = pos_to_vec(new_pos - player_pos).x;

                              if(momentum < PLAYER_SQUISH_MOMENTUM){
                                   auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                                   slow_block_toward_gridlock(world, collision.block, slow_direction);
                                   player_slowing_down = true;
                              }
                         }
                    }else{
                         use_this_collision = false;
                    }

                    player->stopping_block_from = collision.dir;
               }else{
                    result.pos_delta = collision.player_pos_delta;
               }
               break;
          case DIRECTION_RIGHT:
               if(rotated_pos_delta.x < 0.0f){
                    result.pos_delta.x = 0;

                    Direction_t check_dir = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);

                    bool would_squish = false;
                    Block_t* squished_block = player_against_block(player, check_dir, world->block_qt, world->interactive_qt, &world->tilemap);
                    would_squish = squished_block && squished_block->vel.x > collision.block->vel.x;

                    if(!would_squish){
                         would_squish = player_against_solid_tile(player, check_dir, &world->tilemap);
                    }

                    if(!would_squish){
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt, world->block_qt);
                    }

                    auto group_mass = get_block_mass_in_direction(world, collision.block, collision.dir);
                    auto momentum = (F32)group_mass * fabs(collision.block->vel.x);

                    if(would_squish){
                         if(momentum >= PLAYER_SQUISH_MOMENTUM){
                              result.resetting = true;
                              add_global_tag(TAG_BLOCK_SQUISHES_PLAYER);
                         }else{
                              auto collided_pos_offset = collision.pos - collision.block->pos;
                              auto block_new_pos = collision.pos;
                              block_new_pos.pixel.x++;
                              block_new_pos.decimal.x = 0;
                              block_new_pos -= collided_pos_offset;
                              F32 new_pos_delta = pos_to_vec(block_new_pos - collision.block->pos).x;
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, new_pos_delta);
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         if(relevant_move_state == MOVE_STATE_STARTING && rotated_accel.x > 0){
                              // pass
                         }else{
                              auto new_pos = collision.pos - Vec_t{PLAYER_RADIUS, 0};
                              result.pos_delta.x = pos_to_vec(new_pos - player_pos).x;

                              if(momentum < PLAYER_SQUISH_MOMENTUM){
                                   auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                                   slow_block_toward_gridlock(world, collision.block, slow_direction);
                                   player_slowing_down = true;
                              }
                         }
                    }else{
                         use_this_collision = false;
                    }

                    player->stopping_block_from = collision.dir;
               }else{
                    result.pos_delta = collision.player_pos_delta;
               }
               break;
          case DIRECTION_UP:
               if(rotated_pos_delta.y < 0.0f){
                    result.pos_delta.y = 0;

                    Direction_t check_dir = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);

                    bool would_squish = false;
                    Block_t* squished_block = player_against_block(player, check_dir, world->block_qt, world->interactive_qt, &world->tilemap);
                    would_squish = squished_block && squished_block->vel.y > collision.block->vel.y;
                    if(!would_squish){
                         would_squish = player_against_solid_tile(player, check_dir, &world->tilemap);
                    }

                    if(!would_squish){
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt, world->block_qt);
                    }

                    auto group_mass = get_block_mass_in_direction(world, collision.block, collision.dir);
                    auto momentum = (F32)group_mass * fabs(collision.block->vel.y);

                    if(would_squish){
                         if(momentum >= PLAYER_SQUISH_MOMENTUM){
                              result.resetting = true;
                              add_global_tag(TAG_BLOCK_SQUISHES_PLAYER);
                         }else{
                              auto collided_pos_offset = collision.pos - collision.block->pos;
                              auto block_new_pos = collision.pos;
                              block_new_pos.pixel.y++;
                              block_new_pos.decimal.y = 0;
                              block_new_pos -= collided_pos_offset;
                              F32 new_pos_delta = pos_to_vec(block_new_pos - collision.block->pos).y;
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, new_pos_delta);
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         if(relevant_move_state == MOVE_STATE_STARTING && rotated_accel.y > 0){
                              // pass
                         }else{
                              auto new_pos = collision.pos - Vec_t{0, PLAYER_RADIUS};
                              result.pos_delta.y = pos_to_vec(new_pos - player_pos).y;

                              if(momentum < PLAYER_SQUISH_MOMENTUM){
                                   auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                                   slow_block_toward_gridlock(world, collision.block, slow_direction);
                                   player_slowing_down = true;
                              }
                         }
                    }else{
                         use_this_collision = false;
                    }

                    player->stopping_block_from = collision.dir;
               }else{
                    result.pos_delta = collision.player_pos_delta;
               }
               break;
          case DIRECTION_DOWN:
               if(rotated_pos_delta.y > 0.0f){
                    result.pos_delta.y = 0;

                    Direction_t check_dir = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);

                    bool would_squish = false;
                    Block_t* squished_block = player_against_block(player, check_dir, world->block_qt, world->interactive_qt, &world->tilemap);
                    would_squish = squished_block && squished_block->vel.y < collision.block->vel.y;

                    if(!would_squish){
                         would_squish = player_against_solid_tile(player, check_dir, &world->tilemap);
                    }

                    if(!would_squish){
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt, world->block_qt);
                    }

                    auto group_mass = get_block_mass_in_direction(world, collision.block, collision.dir);
                    auto momentum = (F32)group_mass * fabs(collision.block->vel.y);

                    if(would_squish){
                         if(momentum >= PLAYER_SQUISH_MOMENTUM){
                              result.resetting = true;
                              add_global_tag(TAG_BLOCK_SQUISHES_PLAYER);
                         }else{
                              auto collided_pos_offset = collision.pos - collision.block->pos;
                              auto block_new_pos = collision.pos;
                              block_new_pos.pixel.y--;
                              block_new_pos.decimal.y = 0;
                              block_new_pos -= collided_pos_offset;
                              F32 new_pos_delta = pos_to_vec(block_new_pos - collision.block->pos).y;
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, new_pos_delta);
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         if(relevant_move_state == MOVE_STATE_STARTING && rotated_accel.y < 0){
                              // pass
                         }else{
                              F32 block_height = block_get_height_in_pixels(collision.block) * PIXEL_SIZE;
                              auto new_pos = collision.pos + Vec_t{0, block_height + PLAYER_RADIUS};
                              result.pos_delta.y = pos_to_vec(new_pos - player_pos).y;

                              if(momentum < PLAYER_SQUISH_MOMENTUM){
                                   auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                                   slow_block_toward_gridlock(world, collision.block, slow_direction);
                                   player_slowing_down = true;
                              }
                         }
                    }else{
                         use_this_collision = false;
                    }

                    player->stopping_block_from = collision.dir;
               }else{
                    result.pos_delta = collision.player_pos_delta;
               }
               break;
          }

          auto rotated_player_face = direction_rotate_counter_clockwise(player_face, collision.portal_rotations);

          bool held_down = block_held_down_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held();
          bool on_ice = block_on_ice(collision.block->pos, collision.block->pos_delta, collision.block->cut,
                                     &world->tilemap, world->interactive_qt, world->block_qt);
          bool pushable = block_pushable(collision.block, rotated_player_face, world, 1.0f);

          if(use_this_collision && collision.dir == player_face && (player_vel.x != 0.0f || player_vel.y != 0.0f) && (!held_down || (on_ice && pushable))){
               // check that we collide with exactly one block and that if we are pushing through a portal, it is not too high up
               if(!collision.through_portal || (collision.through_portal && collision.block->pos.z < PORTAL_MAX_HEIGHT)){ // also check that the player is actually pushing against the block
                    result.pushing_block = get_block_index(world, collision.block);
                    result.pushing_block_dir = rotated_player_face;
                    result.pushing_block_rotation = collision.portal_rotations;
               }else{
                    // stop the player from pushing 2 blocks at once
                    result.pushing_block = -1;
                    result.pushing_block_dir = DIRECTION_COUNT;
                    result.pushing_block_rotation = 0;
               }
          }else if(held_down && (!on_ice || !pushable) && !player_slowing_down){
               use_this_collision = false;
          }
     }

     Direction_t collided_tile_dir = DIRECTION_COUNT;
     for(S16 y = min.y; y <= max.y; y++){
          for(S16 x = min.x; x <= max.x; x++){
               if(tile_is_solid(world->tilemap.tiles[y] + x)){
                    Coord_t coord {x, y};
                    bool collide_with_tile = false;
                    Rect_t coord_rect = rect_surrounding_coord(coord);
                    position_slide_against_rect(player_pos, coord_rect, PLAYER_RADIUS, &result.pos_delta, &collide_with_tile);
                    if(collide_with_tile){
                        result.collided = true;
                        collided_tile_dir = direction_between(player_coord, coord);
                    }
               }
          }
     }

     Direction_t collided_interactive_dir = DIRECTION_COUNT;
     for(S16 y = min.y; y <= max.y; y++){
          for(S16 x = min.x; x <= max.x; x++){
               Coord_t coord {x, y};

               Interactive_t* interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, coord, player_pos.z, true);
               if(!interactive){
                    PortalExit_t portal_exits = find_portal_exits(coord, &world->tilemap, world->interactive_qt);
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         Direction_t current_portal_dir = (Direction_t)(d);
                         auto portal_exit = portal_exits.directions + d;

                         for(S8 p = 0; p < portal_exit->count; p++){
                              auto portal_dst_coord = portal_exit->coords[p];
                              if(portal_dst_coord == coord) continue;

                              Coord_t portal_dst_output_coord = portal_dst_coord + direction_opposite(current_portal_dir);

                              interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, portal_dst_output_coord, player_pos.z);
                              break;
                         }

                         if(interactive) break;
                    }
               }

               if(interactive){
                    bool collided = false;
                    bool empty_pit_or_solid_interactive = true;
                    if(interactive->type == INTERACTIVE_TYPE_PIT){
                         auto pit_area_filled_in = calculate_pit_area_filled_in(coord, world->block_qt);

                         if(pit_area_filled_in.bottom_left_filled_in || pit_area_filled_in.bottom_right_filled_in ||
                            pit_area_filled_in.top_left_filled_in || pit_area_filled_in.top_right_filled_in){
                              empty_pit_or_solid_interactive = false;
                         }

                         if(!pit_area_filled_in.top_left_filled_in){
                             position_slide_against_rect(player_pos, pit_area_filled_in.top_left_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!pit_area_filled_in.top_right_filled_in){
                             position_slide_against_rect(player_pos, pit_area_filled_in.top_right_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!pit_area_filled_in.bottom_left_filled_in){
                             position_slide_against_rect(player_pos, pit_area_filled_in.bottom_left_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!pit_area_filled_in.bottom_right_filled_in){
                             position_slide_against_rect(player_pos, pit_area_filled_in.bottom_right_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }
                    }

                    if(empty_pit_or_solid_interactive){
                         Rect_t coord_rect = rect_surrounding_coord(coord);
                         position_slide_against_rect(player_pos, coord_rect, PLAYER_RADIUS, &result.pos_delta, &collided);
                         if(collided && !result.collided){
                              result.collided = true;
                              collided_interactive_dir = direction_between(player_coord, coord);
                         }
                    }
               }
          }
     }

     for(S16 i = 0; i < world->players.count; i++){
          if(i == player_index) continue;
          Player_t* other_player = world->players.elements + i;
          if(other_player->clone_instance > 0 && other_player->clone_instance == player_clone_instance) continue; // don't collide while cloning
          F64 distance = pixel_distance_between(player_pos.pixel, other_player->pos.pixel);
          if(distance > PLAYER_RADIUS_IN_SUB_PIXELS * 3.0f) continue;
          bool collided = false;
          Position_t other_player_bottom_left = other_player->pos - Vec_t{PLAYER_RADIUS, PLAYER_RADIUS};
          position_collide_with_rect(player_pos, other_player_bottom_left, 2.0f * PLAYER_RADIUS, 2.0f * PLAYER_RADIUS, &result.pos_delta, &collided);
     }

     return result;
}

TeleportPositionResult_t teleport_position_across_portal(Position_t position, Vec_t pos_delta, World_t* world, Coord_t premove_coord,
                                                         Coord_t postmove_coord, bool require_on){
     TeleportPositionResult_t result {};

     if(postmove_coord == premove_coord) return result;
     auto* interactive = quad_tree_interactive_find_at(world->interactive_qt, postmove_coord);
     if(!is_active_portal(interactive)) return result;
     if(interactive->portal.face != direction_opposite(direction_between(postmove_coord, premove_coord))) return result;

     Position_t offset_from_center = position - coord_to_pos_at_tile_center(postmove_coord);
     PortalExit_t portal_exit = find_portal_exits(postmove_coord, &world->tilemap, world->interactive_qt, require_on);

     for(S8 d = 0; d < DIRECTION_COUNT; d++){
          for(S8 p = 0; p < portal_exit.directions[d].count; p++){
               if(portal_exit.directions[d].coords[p] == postmove_coord) continue;

               Position_t final_offset = offset_from_center;

               Direction_t opposite = direction_opposite((Direction_t)(d));
               U8 rotations_between = direction_rotations_between(interactive->portal.face, opposite);
               final_offset = position_rotate_quadrants_counter_clockwise(final_offset, rotations_between);

               result.results[result.count].rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));
               result.results[result.count].delta = vec_rotate_quadrants_clockwise(pos_delta, result.results[result.count].rotations);
               result.results[result.count].pos = coord_to_pos_at_tile_center(portal_exit.directions[d].coords[p] + opposite) + final_offset;
               result.results[result.count].src_portal = postmove_coord;
               result.results[result.count].dst_portal = portal_exit.directions[d].coords[p];
               result.count++;
          }
     }

     return result;
}

static void illuminate_line(Coord_t start, Coord_t end, U8 value, World_t* world, Coord_t from_portal){
     Coord_t coords[LIGHT_MAX_LINE_LEN];
     S8 coord_count = 0;

     // determine line of points using a modified bresenham to be symmetrical
     {
          if(start.x == end.x){
               // build a simple vertical path
               for(S16 y = start.y; y <= end.y; ++y){
                    coords[coord_count] = Coord_t{start.x, y};
                    coord_count++;
               }
          }else{
               F64 error = 0.0;
               F64 dx = (F64)(end.x) - (F64)(start.x);
               F64 dy = (F64)(end.y) - (F64)(start.y);
               F64 derror = fabs(dy / dx);

               S16 step_x = (start.x < end.x) ? (S16)(1) : (S16)(-1);
               S16 step_y = (end.y - start.y >= 0) ? (S16)(1) : (S16)(-1);
               S16 end_step_x = end.x + step_x;
               S16 sy = start.y;

               for(S16 sx = start.x; sx != end_step_x; sx += step_x){
                    Coord_t coord {sx, sy};
                    coords[coord_count] = coord;
                    coord_count++;

                    error += derror;
                    while(error >= 0.5){
                         coord = {sx, sy};

                         // only add non-duplicate coords
                         if(coords[coord_count - 1] != coord){
                              coords[coord_count] = coord;
                              coord_count++;
                         }

                         sy += step_y;
                         error -= 1.0;
                    }
               }
          }
     }

     for(S8 i = 0; i < coord_count; ++i){
          Tile_t* tile = tilemap_get_tile(&world->tilemap, coords[i]);
          if(!tile) continue;

          S16 diff_x = (S16)(abs(coords[i].x - start.x));
          S16 diff_y = (S16)(abs(coords[i].y - start.y));
          U8 distance = static_cast<U8>(sqrt(static_cast<F32>(diff_x * diff_x + diff_y * diff_y)));

          U8 new_value = value - (distance * (U8)(LIGHT_DECAY));

          if(coords[i] != from_portal){
               Interactive_t* interactive = quad_tree_interactive_find_at(world->interactive_qt, coords[i]);
               if(is_active_portal(interactive)){
                    PortalExit_t portal_exits = find_portal_exits(coords[i], &world->tilemap, world->interactive_qt);
                    for (auto &direction : portal_exits.directions) {
                         for(S8 p = 0; p < direction.count; p++){
                              if(direction.coords[p] == coords[i]) continue;
                              illuminate(direction.coords[p], new_value, world, direction.coords[p]);
                         }
                    }
               }
          }

          Block_t* block = nullptr;
          if(coords[i] != start){
               Rect_t coord_rect = rect_surrounding_adjacent_coords(coords[i]);

               S16 block_count = 0;
               Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
               quad_tree_find_in(world->block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

               for(S16 b = 0; b < block_count; b++){
                    if(block_get_coord(blocks[b]) == coords[i]){
                         block = blocks[b];
                         break;
                    }
               }
          }

          if(coords[i] != start && tile_is_solid(tile)){
               break;
          }

          if(tile->light < new_value) tile->light = new_value;

          if(block) break;

          // TODO: probably handle doors too?
          if(coords[i] != start){
               Interactive_t* interactive = quad_tree_interactive_find_at(world->interactive_qt, coords[i]);
               if(interactive && interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks >= (POPUP_MAX_LIFT_TICKS / 2)){
                    break;
               }
          }
     }
}

void illuminate(Coord_t coord, U8 value, World_t* world, Coord_t from_portal){
     if(coord.x < 0 || coord.y < 0 || coord.x >= world->tilemap.width || coord.y >= world->tilemap.height) return;

     S16 radius = ((value - BASE_LIGHT) / LIGHT_DECAY) + 1;

     if(radius < 0) return;

     Coord_t delta {radius, radius};
     Coord_t min = coord - delta;
     Coord_t max = coord + delta;

     for(S16 j = min.y + 1; j < max.y; ++j) {
          // bottom of box
          illuminate_line(coord, Coord_t{min.x, j}, value, world, from_portal);

          // top of box
          illuminate_line(coord, Coord_t{max.x, j}, value, world, from_portal);
     }

     for(S16 i = min.x + 1; i < max.x; ++i) {
          // left of box
          illuminate_line(coord, Coord_t{i, min.y,}, value, world, from_portal);

          // right of box
          illuminate_line(coord, Coord_t{i, max.y,}, value, world, from_portal);
     }
}

static void impact_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported, bool spread_the_ice){
     Coord_t delta {radius, radius};
     Coord_t min = center - delta;
     Coord_t max = center + delta;

     for(S16 y = min.y; y <= max.y; ++y){
          for(S16 x = min.x; x <= max.x; ++x){
               Coord_t coord{x, y};
               Tile_t* tile = tilemap_get_tile(&world->tilemap, coord);
               if(tile){
                    if(!tile_is_solid(tile)){
                         Rect_t coord_rect = rect_surrounding_adjacent_coords(coord);
                         S16 block_count = 0;
                         Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                         quad_tree_find_in(world->block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                         bool spread_on_block = false;
                         for(S16 i = 0; i < block_count; i++){
                              Block_t* block = blocks[i];
                              if(block_get_coord(block) == coord && height > block->pos.z &&
                                 height < (block->pos.z + HEIGHT_INTERVAL + MELT_SPREAD_HEIGHT) &&
                                 !block_held_down_by_another_block(block, world->block_qt, world->interactive_qt, &world->tilemap).held()){
                                   if(spread_the_ice){
                                        if(block->element == ELEMENT_NONE) block->element = ELEMENT_ONLY_ICED;
                                        spread_on_block = true;
                                        add_global_tag(TAG_BLOCK_BLOCKS_ICE_FROM_BEING_SPREAD);
                                   }else{
                                        if(block->element == ELEMENT_ONLY_ICED) block->element = ELEMENT_NONE;
                                        spread_on_block = true;
                                        add_global_tag(TAG_BLOCK_BLOCKS_ICE_FROM_BEING_MELTED);
                                   }
                              }
                         }

                         Interactive_t* interactive = quad_tree_find_at(world->interactive_qt, coord.x, coord.y);

                         if(!spread_on_block){
                              if(interactive){
                                   switch(interactive->type){
                                   case INTERACTIVE_TYPE_POPUP:
                                        if(interactive->popup.lift.ticks == 1 && height <= MELT_SPREAD_HEIGHT){
                                             if(spread_the_ice){
                                                  add_global_tag(TAG_SPREAD_ICE);
                                                  tile->flags |= TILE_FLAG_ICED;
                                             }else{
                                                  add_global_tag(TAG_MELT_ICE);
                                                  tile->flags &= ~TILE_FLAG_ICED;
                                             }
                                             interactive->popup.iced = false;
                                        }else if(height < interactive->popup.lift.ticks + MELT_SPREAD_HEIGHT){
                                             interactive->popup.iced = spread_the_ice;
                                             if(spread_the_ice){
                                                  add_global_tag(TAG_ICED_POPUP);
                                             }else{
                                                  add_global_tag(TAG_MELTED_POPUP);
                                             }
                                        }
                                        break;
                                   case INTERACTIVE_TYPE_PRESSURE_PLATE:
                                   case INTERACTIVE_TYPE_ICE_DETECTOR:
                                   case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                                        if(height <= MELT_SPREAD_HEIGHT){
                                             if(spread_the_ice){
                                                  add_global_tag(TAG_SPREAD_ICE);
                                                  add_global_tag(TAG_ICED_PRESSURE_PLATE);
                                                  tile->flags |= TILE_FLAG_ICED;
                                             }else{
                                                  add_global_tag(TAG_MELT_ICE);
                                                  add_global_tag(TAG_MELTED_PRESSURE_PLATE);
                                                  tile->flags &= ~TILE_FLAG_ICED;
                                                  if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                                                       interactive->pressure_plate.iced_under = false;
                                                  }
                                             }
                                        }
                                        break;
                                   case INTERACTIVE_TYPE_PIT:
                                        if(height <= 0){
                                             interactive->pit.iced = spread_the_ice;
                                        }
                                        break;
                                   default:
                                        break;
                                   }
                              }else if(height <= MELT_SPREAD_HEIGHT){
                                   if(spread_the_ice){
                                        tile->flags |= TILE_FLAG_ICED;
                                   }else{
                                        add_global_tag(TAG_MELT_ICE);
                                        tile->flags &= ~TILE_FLAG_ICED;
                                   }
                              }
                         }

                         if(is_active_portal(interactive)){
                              if(!teleported){
                                   auto portal_exits = find_portal_exits(coord, &world->tilemap, world->interactive_qt);
                                   for(S8 d = 0; d < DIRECTION_COUNT; d++){
                                        for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                                             if(portal_exits.directions[d].coords[p] == coord) continue;
                                             S16 x_diff = coord.x - center.x;
                                             S16 y_diff = coord.y - center.y;
                                             U8 distance_from_center = (U8)(sqrt(x_diff * x_diff + y_diff * y_diff));
                                             Direction_t opposite = direction_opposite((Direction_t)(d));

                                             impact_ice(portal_exits.directions[d].coords[p] + opposite, height, radius - distance_from_center,
                                                        world, true, spread_the_ice);
                                        }
                                   }
                              }
                         }
                    }else if(!teleported){
                         Coord_t diff = coord - center;
                         if(diff.x != 0 && diff.y != 0){
                              Coord_t attempts[2] = {
                                   Coord_t{(S16)(coord.x - diff.x), coord.y},
                                   Coord_t{coord.x, (S16)(coord.y - diff.y)},
                              };

                              for(S16 a = 0; a < 2; a++){
                                   Coord_t attempt = attempts[a];
                                   Interactive_t* interactive = quad_tree_find_at(world->interactive_qt, attempt.x, attempt.y);
                                   if(is_active_portal(interactive)){
                                        auto portal_exits = find_portal_exits(attempt, &world->tilemap, world->interactive_qt);
                                        for(S8 d = 0; d < DIRECTION_COUNT; d++){
                                             for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                                                  if(portal_exits.directions[d].coords[p] == attempt) continue;

                                                  U8 portal_rotations = direction_rotations_between(interactive->portal.face, static_cast<Direction_t>(d));

                                                  Coord_t offset = coord_rotate_quadrants_clockwise(diff, portal_rotations);
                                                  Coord_t dest = portal_exits.directions[d].coords[p] + offset;

                                                  U8 distance_from_center = (U8)(sqrt(diff.x * diff.x + diff.y * diff.y));

                                                  impact_ice(dest, height, radius - distance_from_center,
                                                             world, true, spread_the_ice);
                                             }
                                        }
                                   }
                              }
                         }
                    }
               }
          }
     }
}

void spread_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported){
     impact_ice(center, height, radius, world, teleported, true);
}

void melt_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported){
     impact_ice(center, height, radius, world, teleported, false);
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

BlockPushMoveDirectionResult_t block_push(Block_t* block, MoveDirection_t move_direction, World_t* world, bool pushed_by_ice,
                                          F32 force, TransferMomentum_t* instant_momentum, PushFromEntangler_t* from_entangler,
                                          S16 block_contributing_momentum_to_total_blocks, bool side_effects){
     Direction_t first;
     Direction_t second;
     move_direction_to_directions(move_direction, &first, &second);

     BlockPushResult_t a = {};
     BlockPushResult_t b = {};

     if(first != DIRECTION_COUNT){
          a = block_push(block, first, world, pushed_by_ice, force, instant_momentum, from_entangler,
                         block_contributing_momentum_to_total_blocks, side_effects);
     }

     if(second != DIRECTION_COUNT){
          b = block_push(block, second, world, pushed_by_ice, force, instant_momentum, from_entangler,
                         block_contributing_momentum_to_total_blocks, side_effects);
     }

     BlockPushMoveDirectionResult_t result {a, b};

     return result;
}

BlockPushResult_t block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum,
                             PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks, bool side_effects){
     return block_push(block, block->pos, block->pos_delta, direction, world, pushed_by_ice, force, instant_momentum,
                       from_entangler, block_contributing_momentum_to_total_blocks, side_effects);
}

static float calc_half_distance_to_next_grid_center(S16 pixel, F32 decimal, S16 block_len, bool positive){
     // if the position is not grid aligned
     if(pixel % block_len != 0 || decimal != 0){

          // find the next grid center
          S16 next_grid_center_pixel = (pixel - (pixel % block_len));
          if(positive) next_grid_center_pixel += block_len;

          S16 pixel_diff = (positive) ? (next_grid_center_pixel - pixel) : (pixel - next_grid_center_pixel);
          F32 decimal_diff = (positive) ? (PIXEL_SIZE - decimal) : (decimal);

          return ((F32)(pixel_diff) * PIXEL_SIZE + decimal_diff) * 0.5f;
     }

     return (block_len / 2) * PIXEL_SIZE;
}

F32 get_block_static_friction(S16 mass){
     return (F32)(mass) * GRAVITY * ICE_STATIC_FRICTION_COEFFICIENT;
}

static bool collision_result_overcomes_friction(F32 original_vel, F32 final_vel, S16 mass){
     static const F32 dt = 0.0166666f;
     if(original_vel != 0) return true;
     F32 change_in_vel = fabs(original_vel - final_vel);
     F32 impulse = (F32)(mass) * change_in_vel;
     F32 applied_force = impulse / dt;
     F32 static_friction_force = get_block_static_friction(mass);
     return applied_force >= static_friction_force;
}

F32 get_block_expected_player_push_velocity(World_t* world, Block_t* block, F32 force){
     S16 mass = get_block_stack_mass(world, block);
     return get_block_normal_pushed_velocity(block->cut, mass, force);
}

static F32 get_block_velocity_ratio(World_t* world, Block_t* block, F32 vel, F32 force){
     return fabs(vel) / get_block_expected_player_push_velocity(world, block, force);
}

bool apply_push_horizontal(Block_t* block, Position_t pos, World_t* world, Direction_t direction, TransferMomentum_t* instant_momentum,
                           bool pushed_by_ice, F32 force, PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks,
                           BlockPushResult_t* result){
      DirectionMask_t accel_mask = vec_direction_mask(block->accel);
      assert(block_contributing_momentum_to_total_blocks != 0);

      auto original_move_state = block->horizontal_move.state;
      if(pushed_by_ice){
           auto pushee_momentum = get_block_momentum(world, block, direction);
           pushee_momentum.mass /= block_contributing_momentum_to_total_blocks;

           auto elastic_result = elastic_transfer_momentum(instant_momentum->mass, instant_momentum->vel, pushee_momentum.mass, pushee_momentum.vel);
           if(collision_result_overcomes_friction(block->vel.x, elastic_result.second_final_velocity, get_block_stack_mass(world, block))){
                add_global_tag(TAB_BLOCK_MOMENTUM_COLLISION);
                auto instant_vel = elastic_result.second_final_velocity;

                BlockElasticCollision_t elastic_collision {};
                elastic_collision.init(instant_momentum->mass, instant_momentum->vel, elastic_result.first_final_velocity,
                                       pushee_momentum.mass, pushee_momentum.vel, elastic_result.second_final_velocity,
                                       block - world->blocks.elements, direction);
                result->collisions.insert(&elastic_collision);

                if(direction == DIRECTION_LEFT){
                    if(instant_vel > 0) instant_vel = -instant_vel;
                }else if(direction == DIRECTION_RIGHT){
                    if(instant_vel < 0) instant_vel = -instant_vel;
                }

                // LOG("giving block %ld: %f horizontal impact momentum\n", block - world->blocks.elements, pushee_momentum.mass * instant_vel);

                block_add_horizontal_momentum(block, BLOCK_MOMENTUM_TYPE_IMPACT, pushee_momentum.mass * instant_vel);

                auto motion = motion_x_component(block);
                F32 x_pos = pos_to_vec(block->pos).x;
                block->horizontal_move.time_left = calc_coast_motion_time_left(block->cut, &motion, x_pos);
                if(direction == DIRECTION_LEFT) block->horizontal_move.time_left = -block->horizontal_move.time_left;
                block->stopped_by_player_horizontal = false;
           }else{
                return false;
           }
      }else if(block->horizontal_move.state == MOVE_STATE_STARTING && direction_in_mask(accel_mask, direction)){
           result->busy = true;
           return false;
      }else{
           if(from_entangler){
               block->accel.x = from_entangler->accel * force;
               block->coast_vel.x = from_entangler->coast_vel * force;
               block->horizontal_move.time_left = from_entangler->move_time_left;

               if(original_move_state == MOVE_STATE_IDLING){
                   block->stop_distance_pixel_x = (S16)(block_get_lowest_dimension(from_entangler->cut) * force) * 0.5f;
               }

               if(direction == DIRECTION_LEFT && block->accel.x > 0){
                   block->accel.x = -block->accel.x;
                   block->coast_vel.x = -block->coast_vel.x;
               }else if(direction == DIRECTION_RIGHT && block->accel.x < 0){
                   block->accel.x = -block->accel.x;
                   block->coast_vel.x = -block->coast_vel.x;
               }
           }else{
               F32 accel_time = BLOCK_ACCEL_TIME;
               S16 lower_dim = block_get_lowest_dimension(block->cut);

               if(lower_dim == HALF_TILE_SIZE_IN_PIXELS) accel_time *= SMALL_BLOCK_ACCEL_MULTIPLIER;

               Position_t block_pos = block_get_position(block);
               Vec_t block_vel = block_get_vel(block);

               F32 half_distance_to_next_grid_center = calc_half_distance_to_next_grid_center(block_pos.pixel.x,
                                                                                              block_pos.decimal.x,
                                                                                              lower_dim,
                                                                                              direction == DIRECTION_RIGHT);

               F32 velocity_ratio = get_block_velocity_ratio(world, block, block_vel.x, force);
               F32 ideal_accel = calc_accel_from_stop((lower_dim / 2) * PIXEL_SIZE, accel_time) * force;

               block->coast_vel.x = calc_velocity_motion(0, ideal_accel, accel_time);

               if(direction == DIRECTION_LEFT){
                   ideal_accel = -ideal_accel;
                   block->coast_vel.x = -block->coast_vel.x;
               }

               if(block->horizontal_move.state == MOVE_STATE_COASTING || block->horizontal_move.state == MOVE_STATE_STOPPING){
                    block->accel.x = ideal_accel;
                    accel_time = (block->coast_vel.x - block_vel.x) / block->accel.x;
               }else{
                    block->accel.x = calc_accel_from_stop(half_distance_to_next_grid_center, accel_time) * force;

                    if(direction == DIRECTION_LEFT){
                        block->accel.x = -block->accel.x;
                    }
               }

               block->horizontal_move.time_left = accel_time - (velocity_ratio * accel_time);
           }

           block->horizontal_move.state = MOVE_STATE_STARTING;
           block->horizontal_move.sign = (direction == DIRECTION_LEFT) ? MOVE_SIGN_NEGATIVE : MOVE_SIGN_POSITIVE;
      }

      block->started_on_pixel_x = pos.pixel.x;
      block->stopped_by_player_horizontal = false;
      return true;
}

bool apply_push_vertical(Block_t* block, Position_t pos, World_t* world, Direction_t direction, TransferMomentum_t* instant_momentum,
                         bool pushed_by_ice, F32 force, PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks,
                         BlockPushResult_t* result){
      DirectionMask_t accel_mask = vec_direction_mask(block->accel);
      assert(block_contributing_momentum_to_total_blocks != 0);

      auto original_move_state = block->vertical_move.state;
      if(pushed_by_ice){
           auto pushee_momentum = get_block_momentum(world, block, direction);
           pushee_momentum.mass /= block_contributing_momentum_to_total_blocks;

           auto elastic_result = elastic_transfer_momentum(instant_momentum->mass, instant_momentum->vel, pushee_momentum.mass, pushee_momentum.vel);
           if(collision_result_overcomes_friction(block->vel.y, elastic_result.second_final_velocity, get_block_stack_mass(world, block))){
                add_global_tag(TAB_BLOCK_MOMENTUM_COLLISION);
                auto instant_vel = elastic_result.second_final_velocity;

                BlockElasticCollision_t elastic_collision {};
                elastic_collision.init(instant_momentum->mass, instant_momentum->vel, elastic_result.first_final_velocity,
                                       pushee_momentum.mass, pushee_momentum.vel, elastic_result.second_final_velocity,
                                       block - world->blocks.elements, direction);
                result->collisions.insert(&elastic_collision);

                if(direction == DIRECTION_DOWN){
                    if(instant_vel > 0) instant_vel = -instant_vel;
                }else if(direction == DIRECTION_UP){
                    if(instant_vel < 0) instant_vel = -instant_vel;
                }

                // LOG("giving block %ld: %f vertical impact momentum\n", block - world->blocks.elements, pushee_momentum.mass * instant_vel);

                block_add_vertical_momentum(block, BLOCK_MOMENTUM_TYPE_IMPACT, pushee_momentum.mass * instant_vel);

                auto motion = motion_y_component(block);
                F32 y_pos = pos_to_vec(block->pos).y;
                block->vertical_move.time_left = calc_coast_motion_time_left(block->cut, &motion, y_pos);
                if(direction == DIRECTION_DOWN) block->vertical_move.time_left = -block->vertical_move.time_left;
                block->stopped_by_player_vertical = false;
           }else{
                return false;
           }
      }else if(block->vertical_move.state == MOVE_STATE_STARTING && direction_in_mask(accel_mask, direction)){
           result->busy = true;
           return false;
      }else{
           if(from_entangler){
               block->accel.y = from_entangler->accel * force;
               block->coast_vel.y = from_entangler->coast_vel * force;
               block->vertical_move.time_left = from_entangler->move_time_left;

               if(original_move_state == MOVE_STATE_IDLING){
                   block->stop_distance_pixel_y = (S16)(block_get_lowest_dimension(from_entangler->cut) * force) * 0.5f;
               }

               if(direction == DIRECTION_DOWN && block->accel.y > 0){
                   block->accel.y = -block->accel.y;
                   block->coast_vel.y = -block->coast_vel.y;
               }else if(direction == DIRECTION_UP && block->accel.y < 0){
                   block->accel.y = -block->accel.y;
                   block->coast_vel.y = -block->coast_vel.y;
               }
           }else{
               F32 accel_time = BLOCK_ACCEL_TIME;
               S16 lower_dim = block_get_lowest_dimension(block->cut);

               if(lower_dim == HALF_TILE_SIZE_IN_PIXELS) accel_time *= SMALL_BLOCK_ACCEL_MULTIPLIER;

               Position_t block_pos = block_get_position(block);
               Vec_t block_vel = block_get_vel(block);

               F32 half_distance_to_next_grid_center = calc_half_distance_to_next_grid_center(block_pos.pixel.y,
                                                                                              block_pos.decimal.y,
                                                                                              lower_dim,
                                                                                              direction == DIRECTION_UP);
               F32 velocity_ratio = get_block_velocity_ratio(world, block, block_vel.y, force);
               F32 ideal_accel = calc_accel_from_stop((lower_dim / 2) * PIXEL_SIZE, accel_time) * force;

               block->coast_vel.y = calc_velocity_motion(0, ideal_accel, accel_time);

               if(direction == DIRECTION_DOWN){
                   ideal_accel = -ideal_accel;
                   block->coast_vel.y = -block->coast_vel.y;
               }

               if(block->vertical_move.state == MOVE_STATE_COASTING || block->vertical_move.state == MOVE_STATE_STOPPING){
                    block->accel.y = ideal_accel;
                    accel_time = (block->coast_vel.y - block_vel.y) / block->accel.y;
               }else{
                    block->accel.y = calc_accel_from_stop(half_distance_to_next_grid_center, accel_time) * force;

                    if(direction == DIRECTION_DOWN){
                        block->accel.y = -block->accel.y;
                    }
               }

               block->vertical_move.time_left = accel_time - (velocity_ratio * accel_time);
           }

           block->vertical_move.state = MOVE_STATE_STARTING;
           block->vertical_move.sign = (direction == DIRECTION_DOWN) ? MOVE_SIGN_NEGATIVE : MOVE_SIGN_POSITIVE;
      }

      block->started_on_pixel_y = pos.pixel.y;
      block->stopped_by_player_vertical = false;
      return true;
}

void update_block_momentum_from_push(Block_t* block, Direction_t direction, PushFromEntangler_t* from_entangler, BlockPushResult_t* push_result, BlockPushResult_t* final_result){
     if(direction_is_horizontal(direction)){
         bool transferred_momentum_back = false;

         for(S16 c = 0; c < push_result->collisions.count; c++){
             auto& collision = push_result->collisions.objects[c];

             if(collision.transferred_momentum_back()){
                 transferred_momentum_back = true;

                 // TODO: how do we handle multiple collisions transferring momentum back?
                 if(from_entangler){
                      block_add_horizontal_momentum(block, BLOCK_MOMENTUM_TYPE_IMPACT, collision.pushee_velocity * collision.pushee_mass);
                      // block->horizontal_move.state = MOVE_STATE_COASTING;
                      // block->horizontal_move.sign = move_sign_from_vel(collision.pushee_velocity);
                      final_result->pushed = true;
                 }else{
                      final_result->collisions.insert(&collision);
                      final_result->pushed = false;
                 }
             }
         }

         if(!transferred_momentum_back) reset_move(&block->horizontal_move);
     }else{
         bool transferred_momentum_back = false;

         for(S16 c = 0; c < push_result->collisions.count; c++){
             auto& collision = push_result->collisions.objects[c];
             if(collision.transferred_momentum_back()){
                 transferred_momentum_back = true;

                 if(from_entangler){
                      block_add_vertical_momentum(block, BLOCK_MOMENTUM_TYPE_IMPACT, collision.pushee_velocity * collision.pushee_mass);
                      final_result->pushed = true;
                 }else{
                      final_result->collisions.insert(&collision);
                      final_result->pushed = false;
                 }
             }
         }

         if(!transferred_momentum_back) reset_move(&block->vertical_move);
     }
}

static bool adjacent_block_has_just_been_pushed(Block_t* block, Direction_t direction){
     bool horizontal = direction_is_horizontal(direction);
     MoveState_t move_state = horizontal ? block->horizontal_move.state : block->vertical_move.state;
     if(move_state == MOVE_STATE_STARTING &&
        direction_in_mask(vec_direction_mask(block->accel), direction)){
          return true;
     }

     return false;
}

bool check_entangled_against_block_able_to_move(Block_t* block, Direction_t direction, Block_t* against_block, Direction_t against_direction, bool both_on_ice, World_t* world){
     if(direction == DIRECTION_COUNT) return false;

     // if block is entangled with the block it collides with, check if the entangled block can move, this is kind of duplicate work
     bool only_against_stationary_entanglers = true;
     Block_t* entangled_against_block = against_block;
     while(entangled_against_block){
          Direction_t check_direction = against_direction;

          Position_t entangled_against_block_pos = block_get_position(entangled_against_block);
          Vec_t entangled_against_block_pos_delta = block_get_pos_delta(entangled_against_block);
          BlockCut_t entangled_against_block_cut = block_get_cut(entangled_against_block);

          auto next_against_block = block_against_another_block(entangled_against_block_pos + entangled_against_block_pos_delta,
                                                                entangled_against_block_cut,
                                                                check_direction, world->block_qt,
                                                                world->interactive_qt, &world->tilemap,
                                                                &check_direction);
          if(next_against_block == nullptr) break;
          if(!blocks_are_entangled(entangled_against_block, next_against_block, &world->blocks) &&
             !block_on_ice(next_against_block->pos, entangled_against_block->pos_delta, entangled_against_block->cut,
                           &world->tilemap, world->interactive_qt, world->block_qt) &&
             !adjacent_block_has_just_been_pushed(next_against_block, direction)){
               only_against_stationary_entanglers = false;
               break;
          }

          entangled_against_block = next_against_block;
     }

     if(!only_against_stationary_entanglers){
          return false;
     }

     if(against_block->rotation != block->rotation){
          if(!both_on_ice){
               return false; // only when the rotation is equal can we move with the block
          }
     }

     if(block_against_solid_tile(against_block, direction, &world->tilemap)){
          return false;
     }

     if(block_against_solid_interactive(against_block, direction, &world->tilemap, world->interactive_qt)){
          return false;
     }

     return true;
}

bool resolve_push_against_block(Block_t* block, MoveDirection_t move_direction, bool pushed_by_ice, bool pushed_block_on_ice,
                                F32 force, TransferMomentum_t* instant_momentum, PushFromEntangler_t* from_entangler,
                                BlockAgainstOther_t* against, S16 against_count, World_t* world,
                                S16 block_contributing_momentum_to_total_blocks, bool side_effects,
                                BlockPushResult_t* result, bool* transfers_force){
     Block_t* against_block = against->block;
     Direction_t first_direction;
     Direction_t second_direction;

     move_direction_to_directions(move_direction, &first_direction, &second_direction);

     Direction_t first_against_block_push_dir = direction_rotate_clockwise(first_direction, against->rotations_through_portal);
     Direction_t second_against_block_push_dir = direction_rotate_clockwise(second_direction, against->rotations_through_portal);

     TransferMomentum_t first_rotated_instant_momentum = {};
     TransferMomentum_t second_rotated_instant_momentum = {};

     if(instant_momentum){
          second_rotated_instant_momentum = *instant_momentum;
          first_rotated_instant_momentum = *instant_momentum;
          first_rotated_instant_momentum.vel = rotate_vec_counter_clockwise_to_see_if_negates(first_rotated_instant_momentum.vel,
                                                                                              direction_is_horizontal(first_against_block_push_dir),
                                                                                              against->rotations_through_portal);

          second_rotated_instant_momentum.vel = rotate_vec_counter_clockwise_to_see_if_negates(first_rotated_instant_momentum.vel,
                                                                                              direction_is_horizontal(second_against_block_push_dir),
                                                                                              against->rotations_through_portal);
     }

     MoveDirection_t final_move_direction = move_direction_from_directions(first_against_block_push_dir, second_against_block_push_dir);

     // TODO: should this be block_on_frictionless() ?
     bool on_ice = block_on_ice(against_block->pos, against_block->pos_delta, against_block->cut,
                                &world->tilemap, world->interactive_qt, world->block_qt);
     bool both_on_ice = (on_ice && pushed_block_on_ice);

     bool are_entangled = blocks_are_entangled(block, against_block, &world->blocks);

     if(against_block == block){
          if(pushed_by_ice && block_on_ice(against_block->pos, against_block->pos_delta, against_block->cut,
                                           &world->tilemap, world->interactive_qt, world->block_qt)){
               // pass
          }else{
               return false;
          }
     }else if(on_ice){
          // if the block originally pushed is not on ice, we don't push this one either
          if(!pushed_block_on_ice && !are_entangled){
               return false;
          }

          if(pushed_by_ice && instant_momentum){
               // check if we are able to move or if we transfer our force to the block
               F32 block_against_dir_vel = 0;

               Vec_t against_block_vel = get_block_momentum_vel(world, against_block);

               switch(first_against_block_push_dir){
               default:
                    break;
               case DIRECTION_LEFT:
                    *transfers_force = (against_block_vel.x > first_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_UP:
                    *transfers_force = (against_block_vel.y < first_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               case DIRECTION_RIGHT:
                    *transfers_force = (against_block_vel.x < first_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_DOWN:
                    *transfers_force = (against_block_vel.y > first_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               }

               switch(second_against_block_push_dir){
               default:
                    break;
               case DIRECTION_LEFT:
                    *transfers_force = (against_block_vel.x > second_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_UP:
                    *transfers_force = (against_block_vel.y < second_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               case DIRECTION_RIGHT:
                    *transfers_force = (against_block_vel.x < second_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_DOWN:
                    *transfers_force = (against_block_vel.y > second_rotated_instant_momentum.vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               }

               if(*transfers_force){
                    auto split_instant_momentum = first_rotated_instant_momentum;
                    split_instant_momentum.mass /= against_count;

                    // check if the momentum transfer overcomes static friction
                    auto block_mass = get_block_stack_mass(world, block) / against_count;
                    auto elastic_result = elastic_transfer_momentum(split_instant_momentum.mass, split_instant_momentum.vel, block_mass, block_against_dir_vel);

                    if(!collision_result_overcomes_friction(block_against_dir_vel, elastic_result.second_final_velocity, block_mass)){
                         return false;
                    }

                    if(result){
                         // TODO: we need to handle multiple direction momentums that we calculated
                         auto push_result = block_push(against_block, final_move_direction, world, pushed_by_ice, force,
                                                       &split_instant_momentum, nullptr, block_contributing_momentum_to_total_blocks,
                                                       side_effects);
                         result->againsts_pushed.merge(&push_result.horizontal_result.againsts_pushed);
                         result->againsts_pushed.merge(&push_result.vertical_result.againsts_pushed);

                         if(push_result.horizontal_result.pushed){
                              BlockPushedAgainst_t pushed_against {};
                              pushed_against.block = against_block;
                              pushed_against.direction = first_against_block_push_dir;
                              result->againsts_pushed.insert(&pushed_against);
                         }

                         if(push_result.vertical_result.pushed){
                              BlockPushedAgainst_t pushed_against {};
                              pushed_against.block = against_block;
                              pushed_against.direction = second_against_block_push_dir;
                              result->againsts_pushed.insert(&pushed_against);
                         }

                         update_block_momentum_from_push(block, first_direction, from_entangler, &push_result.horizontal_result, result);

                         if(second_direction != DIRECTION_COUNT){
                              update_block_momentum_from_push(block, second_direction, from_entangler, &push_result.vertical_result, result);
                         }
                    }
               }
          }else if(pushed_block_on_ice){
               if(are_entangled){
                    // if they are opposite entangled (potentially through a portal) then they just push into each other
                    S8 total_collided_rotations = direction_rotations_between(first_direction, first_against_block_push_dir) + against_block->rotation;
                    total_collided_rotations %= DIRECTION_COUNT;

                    auto rotations_between = direction_rotations_between((Direction_t)(total_collided_rotations), (Direction_t)(block->rotation));
                    if(rotations_between == 2){
                         return false;
                    }
               }

               if(result){
                    if(block_starting_to_move_in_direction(against_block, first_against_block_push_dir) ||
                       block_starting_to_move_in_direction(against_block, second_against_block_push_dir)){
                         // TODO: I don't think this is exactly the right logic, we may want to still do a push in one of the directions even if the other is
                         // pass
                    }else{
                         auto save_block = *against_block;
                         auto push_result = block_push(against_block, final_move_direction, world, pushed_by_ice, force,
                                                       instant_momentum, nullptr, block_contributing_momentum_to_total_blocks,
                                                       side_effects);
                         if(!side_effects) *against_block = save_block;
                         result->againsts_pushed.merge(&push_result.horizontal_result.againsts_pushed);
                         result->againsts_pushed.merge(&push_result.vertical_result.againsts_pushed);

                         bool first_successful = push_result.horizontal_result.pushed;

                         if(push_result.horizontal_result.pushed){
                              BlockPushedAgainst_t pushed_against {};
                              pushed_against.block = against_block;
                              pushed_against.direction = first_against_block_push_dir;
                              result->againsts_pushed.insert(&pushed_against);
                         }

                         bool second_successful = push_result.vertical_result.pushed;

                         if(push_result.vertical_result.pushed){
                              BlockPushedAgainst_t pushed_against {};
                              pushed_against.block = against_block;
                              pushed_against.direction = second_against_block_push_dir;
                              result->againsts_pushed.insert(&pushed_against);
                         }

                         if(!first_successful && !second_successful){
                              return false;
                         }
                    }
               }
          }
     }

     if(blocks_are_entangled(against_block, block, &world->blocks)){
          bool check_first = check_entangled_against_block_able_to_move(block, first_direction, against_block, first_against_block_push_dir, both_on_ice, world);
          bool check_second = check_entangled_against_block_able_to_move(block, second_direction, against_block, second_against_block_push_dir, both_on_ice, world);

          if(!check_first && !check_second){
               return false;
          }
     }else if(!on_ice){
          bool first_successful = false;
          bool second_successful = false;

          F32 total_block_mass = get_block_mass_in_direction(world, block, first_direction, false);

          if(total_block_mass <= PLAYER_MAX_PUSH_MASS_ON_FRICTION){
              add_global_tag(TAG_PLAYER_PUSHES_MORE_THAN_ONE_MASS);
              auto save_block = *against_block;
              auto push_result = block_push(against_block, first_against_block_push_dir, world, pushed_by_ice, force, instant_momentum, nullptr,
                                            block_contributing_momentum_to_total_blocks, side_effects);
              if(!side_effects) *against_block = save_block;
              result->againsts_pushed.merge(&push_result.againsts_pushed);

              first_successful = push_result.pushed;

              if(push_result.pushed && result){
                   BlockPushedAgainst_t pushed_against {};
                   pushed_against.block = against_block;
                   pushed_against.direction = first_against_block_push_dir;
                   result->againsts_pushed.insert(&pushed_against);
              }
          }

          if(second_direction != DIRECTION_COUNT){
               total_block_mass = get_block_mass_in_direction(world, block, second_direction, false);

               if(total_block_mass <= PLAYER_MAX_PUSH_MASS_ON_FRICTION){
                   add_global_tag(TAG_PLAYER_PUSHES_MORE_THAN_ONE_MASS);
                   auto save_block = *against_block;
                   auto push_result = block_push(against_block, second_against_block_push_dir, world, pushed_by_ice,
                                                 force, instant_momentum, nullptr,
                                                 block_contributing_momentum_to_total_blocks, side_effects);
                   if(!side_effects) *against_block = save_block;
                   result->againsts_pushed.merge(&push_result.againsts_pushed);

                   second_successful = push_result.pushed;

                   if(push_result.pushed && result){
                        BlockPushedAgainst_t pushed_against {};
                        pushed_against.block = against_block;
                        pushed_against.direction = first_against_block_push_dir;
                        result->againsts_pushed.insert(&pushed_against);
                   }
               }
          }

          bool adjacent_block_already_moving = false;

          {
               if(adjacent_block_has_just_been_pushed(against_block, first_against_block_push_dir)){
                    adjacent_block_already_moving = true;
               }

               if(adjacent_block_has_just_been_pushed(against_block, second_against_block_push_dir)){
                    adjacent_block_already_moving = true;
               }
          }

          if(!first_successful && !second_successful && !adjacent_block_already_moving){
               return false;
          }
     }

     return true;
}

bool is_block_against_solid_centroid(Block_t* block, Direction_t direction, F32 force, World_t* world){
     Block_t* entangled_block = rotated_entangled_blocks_against_centroid(block, direction, world->block_qt, &world->blocks, world->interactive_qt, &world->tilemap);
     if(entangled_block){
          S16 block_mass = block_get_mass(block);
          S16 entangled_block_mass = block_get_mass(entangled_block);
          F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
          if((mass_ratio * force) >= 1.0f) return true;
     }

     return false;
}

bool block_would_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                      bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum,
                      PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks,
                      bool side_effects, BlockPushResult_t* result)
{
     auto against_result = block_against_other_blocks(pos + pos_delta, block->cut, direction, world->block_qt, world->interactive_qt,
                                                      &world->tilemap);
     bool pushed_block_on_frictionless = block_on_frictionless(pos, pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt);

     bool transfers_force = false;
     {
          // TODO: we have returns in this loop, how do we handle them?
          MoveDirection_t move_direction = move_direction_from_directions(direction, DIRECTION_COUNT);

          for(S16 i = 0; i < against_result.count; i++){
               if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_frictionless, force,
                                              instant_momentum, from_entangler, against_result.objects + i, against_result.count,
                                              world, block_contributing_momentum_to_total_blocks, side_effects,
                                              result, &transfers_force)){
                    return false;
               }
          }
     }

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          if(block->vertical_move.state != MOVE_STATE_IDLING && block->accel.y != 0.0f){
               Direction_t vertical_direction = block->accel.y > 0.0f ? DIRECTION_UP : DIRECTION_DOWN;
               DirectionMask_t directions = direction_mask_add(direction_to_direction_mask(direction), vertical_direction);
               auto against = block_diagonally_against_block(pos + pos_delta, block->cut, directions, &world->tilemap, world->interactive_qt, world->block_qt);
               if(against.block != NULL){
                    MoveDirection_t move_direction = move_direction_from_directions(direction, vertical_direction);
                    if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_frictionless, force, instant_momentum,
                                                   from_entangler, &against, 1, world, block_contributing_momentum_to_total_blocks,
                                                   side_effects, result, &transfers_force)){
                         return false;
                    }
               }
          }
          break;
     case DIRECTION_DOWN:
     case DIRECTION_UP:
          if(block->horizontal_move.state != MOVE_STATE_IDLING && block->accel.x != 0.0f){
               Direction_t horizontal_direction = block->accel.x > 0.0f ? DIRECTION_RIGHT : DIRECTION_LEFT;
               DirectionMask_t directions = direction_mask_add(direction_to_direction_mask(direction), horizontal_direction);
               auto against = block_diagonally_against_block(pos + pos_delta, block->cut, directions, &world->tilemap, world->interactive_qt, world->block_qt);
               if(against.block != NULL){
                    MoveDirection_t move_direction = move_direction_from_directions(horizontal_direction, direction);
                    if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_frictionless, force, instant_momentum,
                                                   from_entangler, &against, 1, world, block_contributing_momentum_to_total_blocks,
                                                   side_effects, result, &transfers_force)){
                         return false;
                    }
               }
          }
          break;
     }

     if(transfers_force){
          return false;
     }

     if(!pushed_by_ice){
          auto against_block = rotated_entangled_blocks_against_centroid(block, direction, world->block_qt, &world->blocks,
                                                                         world->interactive_qt, &world->tilemap);
          if(against_block){
               // given the current force, and masses, can this push move the entangled block anyways?
               if(is_block_against_solid_centroid(block, direction, force, world)) return false;
          }
     }

     if(block_against_solid_tile(pos, pos_delta, block->cut, direction, &world->tilemap)){
          return false;
     }

     if(block_against_solid_interactive(block, direction, &world->tilemap, world->interactive_qt)){
          return false;
     }

     // check if we are diagonally against a solid and already moving in an orthogonal direction
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          if(block->vertical_move.state != MOVE_STATE_IDLING && block->accel.y != 0.0f){
               Direction_t vertical_direction = block->accel.y > 0.0f ? DIRECTION_UP : DIRECTION_DOWN;
               if(block_diagonally_against_solid(pos, pos_delta, block->cut, direction, vertical_direction, &world->tilemap, world->interactive_qt)){
                    return false;
               }
          }
          break;
     case DIRECTION_DOWN:
     case DIRECTION_UP:
          if(block->horizontal_move.state != MOVE_STATE_IDLING && block->accel.x != 0.0f){
               Direction_t horizontal_direction = block->accel.x > 0.0f ? DIRECTION_RIGHT : DIRECTION_LEFT;
               if(block_diagonally_against_solid(pos, pos_delta, block->cut, horizontal_direction, direction, &world->tilemap, world->interactive_qt)){
                    return false;
               }
          }
          break;
     }

     auto* player = block_against_player(block, direction, &world->players);
     if(player){
          bool player_should_stop_block = true;

          if(pushed_by_ice){
               // if the block was pushed by ice, then check how much momentum was behind it
               auto block_momentum = instant_momentum ? instant_momentum->vel * instant_momentum->mass : 0;
               if(block_momentum >= PLAYER_SQUISH_MOMENTUM) player_should_stop_block = false;

               if(direction_in_vec(block->vel, direction)){
                    player_should_stop_block = false;
               }
          }

          if(player_should_stop_block){
               player->stopping_block_from = direction_opposite(direction);
               player->stopping_block_from_time = PLAYER_STOP_IDLE_BLOCK_TIMER;
               return false;
          }
     }

     return true;
}

void block_do_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                   bool pushed_by_ice, BlockPushResult_t* result, F32 force, TransferMomentum_t* instant_momentum,
                   PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks){
     bool pushed_block_on_frictionless = block_on_frictionless(pos, pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt);

     auto* player = block_against_player(block, direction, &world->players);
     if(player){
          bool player_should_stop_block = true;

          if(pushed_by_ice){
               // if the block was pushed by ice, then check how much momentum was behind it
               auto block_momentum = instant_momentum ? instant_momentum->vel * instant_momentum->mass : 0;
               if(block_momentum >= PLAYER_SQUISH_MOMENTUM) player_should_stop_block = false;

               if(direction_in_vec(block->vel, direction)){
                    player_should_stop_block = false;
               }
          }

          if(player_should_stop_block){
               player->stopping_block_from = direction_opposite(direction);
               player->stopping_block_from_time = PLAYER_STOP_IDLE_BLOCK_TIMER;
               return;
          }
     }

     // if are sliding on ice and are pushed in the opposite direction then stop
     if(pushed_block_on_frictionless && !instant_momentum && from_entangler){
          switch(direction){
          default:
               break;
          case DIRECTION_LEFT:
          case DIRECTION_RIGHT:
          {
               // TODO: compress this code with similar code elsewhere
               Direction_t block_move_dir = block_axis_move(block, true);

               if(direction_opposite(block_move_dir) == direction){
                    S16 block_mass = get_block_stack_mass(world, block);
                    F32 normal_block_velocity = get_block_normal_pushed_velocity(block->cut, block_mass);
                    if(block->vel.x < 0) normal_block_velocity = -normal_block_velocity;
                    F32 final_vel = block->vel.x - normal_block_velocity;

                    if(fabs(final_vel) <= FLT_EPSILON){
                        slow_block_toward_gridlock(world, block, direction_opposite(direction));
                        return;
                    }else{
                        // TODO: This calculation is probably wrong in terms of forces, but gets us the gameplay impact we want, come back to this
                        block->accel.x = (final_vel - block->vel.x) / BLOCK_ACCEL_TIME;
                        block->target_vel.x = final_vel;
                        block->horizontal_move.time_left = BLOCK_ACCEL_TIME;
                        block->horizontal_move.state = MOVE_STATE_STARTING;
                        block->started_on_pixel_x = 0;
                        block->stop_distance_pixel_x = 0;
                        return;
                    }
               }
               break;
          }
               break;
          case DIRECTION_UP:
          case DIRECTION_DOWN:
          {
               Direction_t block_move_dir = block_axis_move(block, false);

               // TODO: handle instant momentum

               if(direction_opposite(block_move_dir) == direction){
                    S16 block_mass = get_block_stack_mass(world, block);
                    F32 normal_block_velocity = get_block_normal_pushed_velocity(block->cut, block_mass);
                    if(block->vel.y < 0) normal_block_velocity = -normal_block_velocity;
                    F32 final_vel = block->vel.y - normal_block_velocity;

                    if(fabs(final_vel) <= FLT_EPSILON){
                        slow_block_toward_gridlock(world, block, direction_opposite(direction));
                        return;
                    }else{
                        block->accel.y = (final_vel - block->vel.y) / BLOCK_ACCEL_TIME;
                        block->target_vel.y = final_vel;
                        block->vertical_move.time_left = BLOCK_ACCEL_TIME;
                        block->vertical_move.state = MOVE_STATE_STARTING;
                        block->started_on_pixel_y = 0;
                        block->stop_distance_pixel_y = 0;
                        return;
                    }
               }
               break;
          }
          }
     }

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          if(!apply_push_horizontal(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler,
                                    block_contributing_momentum_to_total_blocks, result)){
               return;
          }
          break;
     case DIRECTION_DOWN:
     case DIRECTION_UP:
          if(!apply_push_vertical(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler,
                                  block_contributing_momentum_to_total_blocks, result)){
               return;
          }
          break;
     }

     result->pushed = true;
}

BlockPushResult_t block_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world,
                             bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum,
                             PushFromEntangler_t* from_entangler, S16 block_contributing_momentum_to_total_blocks,
                             bool side_effects){
     // LOG("block_push() %d -> %s with force %f colliding with blocks: %d, by ice: %d side effects: %d\n",
     //     get_block_index(world, block), direction_to_string(direction), force,
     //     block_contributing_momentum_to_total_blocks, pushed_by_ice, side_effects);
     // if(instant_momentum){
     //     LOG(" instant momentum %d, %f\n", instant_momentum->mass, instant_momentum->vel);
     // }
     // if(from_entangler){
     //      LOG(" from entangler: accel: %f, move_time_left: %f, coast_vel: %f, cut: %d\n", from_entangler->accel,
     //          from_entangler->move_time_left, from_entangler->coast_vel, from_entangler->cut);
     // }

     BlockPushResult_t result {};
     if(!block_would_push(block, pos, pos_delta, direction, world, pushed_by_ice, force, instant_momentum,
                          from_entangler, block_contributing_momentum_to_total_blocks, side_effects,
                          &result)){
          return result;
     }

     block_do_push(block, pos, pos_delta, direction, world, pushed_by_ice, &result, force, instant_momentum,
                   from_entangler, block_contributing_momentum_to_total_blocks);
     return result;
}

bool block_pushable(Block_t* block, Direction_t direction, World_t* world, F32 force){
     Direction_t collided_block_push_dir = DIRECTION_COUNT;
     Block_t* collided_block = block_against_another_block(block->pos + block->pos_delta, block->cut, direction, world->block_qt,
                                                           world->interactive_qt, &world->tilemap, &collided_block_push_dir);
     if(collided_block){
          if(collided_block == block){
               // pass, this happens in a corner portal!
          }else if(blocks_are_entangled(collided_block, block, &world->blocks)){
               return block_pushable(collided_block, collided_block_push_dir, world, force);
          }else{
               return false;
          }
     }

     if(is_block_against_solid_centroid(block, direction, force, world)) return false;
     if(block_against_solid_tile(block, direction, &world->tilemap)) return false;
     if(block_against_solid_interactive(block, direction, &world->tilemap, world->interactive_qt)) return false;

     return true;
}

bool reset_players(ObjectArray_t<Player_t>* players){
     destroy(players);
     bool success = init(players, 1);
     if(success){
          *players->elements = Player_t{};
          players->elements[0].has_bow = true;
     }
     return success;
}

void describe_block(World_t* world, Block_t* block){
     auto mass = get_block_stack_mass(world, block);
     auto horizontal_momentum = fabs(mass * block->vel.x);
     auto vertical_momentum = fabs(mass * block->vel.y);
     LOG("block %ld: pixel %d, %d, %d, -> (%d, %d) decimal: %.10f, %.10f, rot: %d, element: %s, entangle: %d, clone id: %d\n",
         block - world->blocks.elements, block->pos.pixel.x, block->pos.pixel.y, block->pos.z,
         block->pos.pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS, block->pos.pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS,
         block->pos.decimal.x, block->pos.decimal.y,
         block->rotation, element_to_string(block->element),
         block->entangle_index, block->clone_id);
     LOG("     cut : %s\n", block_cut_to_string(block->cut));
     LOG("  accel  : %.10f, %.10f\n", block->accel.x, block->accel.y);
     LOG("    vel  : %.10f, %.10f prev_vel: %f, %f coast: %f, %f\n", block->vel.x, block->vel.y, block->prev_vel.x, block->prev_vel.y, block->coast_vel.x, block->coast_vel.y);
     LOG(" pos dt  : %f, %f\n", block->pos_delta.x, block->pos_delta.y);
     LOG(" start px: %d, %d\n", block->started_on_pixel_x, block->started_on_pixel_y);
     LOG(" stop px : %d, %d\n", block->stop_on_pixel_x, block->stop_on_pixel_y);
     LOG(" hmove   : %s %s %f\n", move_state_to_string(block->horizontal_move.state), move_sign_to_string(block->horizontal_move.sign), block->horizontal_move.distance);
     LOG(" vmove   : %s %s %f\n", move_state_to_string(block->vertical_move.state), move_sign_to_string(block->vertical_move.sign), block->vertical_move.distance);
     LOG(" hcoast  : %s vcoast: %s\n", block_coast_to_string(block->coast_horizontal), block_coast_to_string(block->coast_vertical));
     LOG(" con_tp  : %d %s\n", block->connected_teleport.block_index, direction_to_string(block->connected_teleport.direction));
     LOG(" flags   : held_up: %d, carried_by_block: %d stopped_by_player_horizontal: %d, stopped_by_player_vertical: %d\n",
         block->held_up, block->carried_pos_delta.block_index, block->stopped_by_player_horizontal,
         block->stopped_by_player_vertical);
     LOG(" stack mass: %d, horizontal momentum: %f, vertical momentum: %f\n", mass, horizontal_momentum, vertical_momentum);
     LOG("\n");
}

void describe_player(World_t* world, Player_t* player){
     S16 index = player - world->players.elements;
     LOG("Player %d: pixel: %d, %d, %d, decimal %f, %f, face: %s push_block: %d, push_block_dir: %s, push_block_rot: %d push_time: %f\n", index,
         player->pos.pixel.x, player->pos.pixel.y, player->pos.z, player->pos.decimal.x, player->pos.decimal.y,
         direction_to_string(player->face), player->pushing_block, direction_to_string(player->pushing_block_dir), player->pushing_block_rotation,
         player->push_time);
     LOG("  vel: %f, %f accel: %f, %f pos_dt: %f, %f\n", player->vel.x, player->vel.y, player->accel.x, player->accel.y, player->pos_delta.x , player->pos_delta.y);
     LOG("  rot: %d, move_rot: L %d, U %d, R %d, D %d\n", player->rotation,
         player->move_rotation[DIRECTION_LEFT], player->move_rotation[DIRECTION_UP], player->move_rotation[DIRECTION_RIGHT], player->move_rotation[DIRECTION_DOWN]);
     LOG("  held_up: %d carried by: pos: %f, %f neg: %f, %f\n", player->held_up, player->carried_pos_delta.positive.x, player->carried_pos_delta.positive.y,
         player->carried_pos_delta.negative.x, player->carried_pos_delta.negative.y);
     LOG("\n");
}

void describe_coord(Coord_t coord, World_t* world){
     auto coord_rect = rect_surrounding_coord(coord);
     LOG("\ndescribe_coord(%d, %d) rect: %d, %d, <-> %d, %d\n", coord.x, coord.y, coord_rect.left, coord_rect.bottom,
         coord_rect.right, coord_rect.top);
     auto* tile = tilemap_get_tile(&world->tilemap, coord);
     if(tile){
          LOG("Tile: id: %u, light: %u rot: %u\n", tile->id, tile->light, tile->rotation);
          if(tile->flags){
               LOG(" flags:\n");
               if(tile->flags & TILE_FLAG_ICED) printf("  ICED\n");
               if(tile->flags & TILE_FLAG_SOLID) printf("  SOLID\n");
               if(tile->flags & TILE_FLAG_RESET_IMMUNE) printf("  RESET_IMMUNE\n");
               if(tile->flags & TILE_FLAG_WIRE_STATE) printf("  WIRE_STATE\n");
               if(tile->flags & TILE_FLAG_WIRE_LEFT) printf("  WIRE_LEFT\n");
               if(tile->flags & TILE_FLAG_WIRE_UP) printf("  WIRE_UP\n");
               if(tile->flags & TILE_FLAG_WIRE_RIGHT) printf("  WIRE_RIGHT\n");
               if(tile->flags & TILE_FLAG_WIRE_DOWN) printf("  WIRE_DOWN\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT) printf("  CLUSTER_LEFT\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID) printf("  CLUSTER_MID\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT) printf("  CLUSTER_RIGHT\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON) printf("  CLUSTER_LEFT_ON\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID_ON) printf("  CLUSTER_MID_ON\n");
               if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON) printf("  CLUSTER_RIGHT_ON\n");
          }
     }

     auto* interactive = quad_tree_find_at(world->interactive_qt, coord.x, coord.y);
     if(interactive){
          const char* type_string = "INTERACTIVE_TYPE_UKNOWN";
          const int info_string_len = 128;
          char info_string[info_string_len];
          memset(info_string, 0, info_string_len);

          switch(interactive->type){
          default:
               break;
          case INTERACTIVE_TYPE_NONE:
               type_string = "NONE";
               break;
          case INTERACTIVE_TYPE_PRESSURE_PLATE:
               type_string = "PRESSURE_PLATE";
               snprintf(info_string, info_string_len, "down: %d, iced_undo: %d",
                        interactive->pressure_plate.down, interactive->pressure_plate.iced_under);
               break;
          case INTERACTIVE_TYPE_LIGHT_DETECTOR:
               type_string = "LIGHT_DETECTOR";
               snprintf(info_string, info_string_len, "on: %d", interactive->detector.on);
               break;
          case INTERACTIVE_TYPE_ICE_DETECTOR:
               type_string = "ICE_DETECTOR";
               snprintf(info_string, info_string_len, "on: %d", interactive->detector.on);
               break;
          case INTERACTIVE_TYPE_POPUP:
               type_string = "POPUP";
               snprintf(info_string, info_string_len, "lift: ticks: %u, timer: %f, up: %d, iced: %d",
                        interactive->popup.lift.ticks, interactive->popup.lift.timer, interactive->popup.lift.up, interactive->popup.iced);
               break;
          case INTERACTIVE_TYPE_LEVER:
               type_string = "LEVER";
               break;
          case INTERACTIVE_TYPE_DOOR:
               type_string = "DOOR";
               snprintf(info_string, info_string_len, "face: %s, lift: ticks %u, up: %d",
                        direction_to_string(interactive->door.face), interactive->door.lift.ticks,
                        interactive->door.lift.up);
               break;
          case INTERACTIVE_TYPE_PORTAL:
               type_string = "PORTAL";
               snprintf(info_string, info_string_len, "face: %s, on: %d has_block_inside: %d wants_to_turn_off: %d", direction_to_string(interactive->portal.face),
                        interactive->portal.on, interactive->portal.has_block_inside, interactive->portal.wants_to_turn_off);
               break;
          case INTERACTIVE_TYPE_BOMB:
               type_string = "BOMB";
               break;
          case INTERACTIVE_TYPE_BOW:
               type_string = "BOW";
               break;
          case INTERACTIVE_TYPE_STAIRS:
               type_string = "STAIRS";
               break;
          case INTERACTIVE_TYPE_PROMPT:
               type_string = "PROMPT";
               break;
          case INTERACTIVE_TYPE_PIT:
               type_string = "PIT";
               snprintf(info_string, info_string_len, "iced: %d", interactive->pit.iced);
               break;
          }

          LOG("type: %s %s\n", type_string, info_string);
     }

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(world->block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     for(S16 i = 0; i < block_count; i++){
          auto* block = blocks[i];
          describe_block(world, block);
     }

     for(S16 i = 0; i < world->players.count; i++){
          auto* player = world->players.elements + i;
          auto player_coord = pos_to_coord(player->pos);

          if(player_coord == coord){
               describe_player(world, player);
          }
     }
}

S16 get_block_index(World_t* world, Block_t* block){
    S16 index = block - world->blocks.elements;
    assert(index >= 0 && index <= world->blocks.count);
    return index;
}

bool setup_default_room(World_t* world){
     init(&world->tilemap, ROOM_TILE_SIZE, ROOM_TILE_SIZE);

     for(S16 i = 0; i < world->tilemap.width; i++){
          Tile_t* bottom_wall_top = world->tilemap.tiles[0] + i;
          Tile_t* bottom_wall_bottom = world->tilemap.tiles[1] + i;

          bottom_wall_top->id = 0;
          bottom_wall_bottom->id = 1;
          bottom_wall_top->rotation = 2;
          bottom_wall_bottom->rotation = 2;
          bottom_wall_top->flags |= TILE_FLAG_SOLID;
          bottom_wall_bottom->flags |= TILE_FLAG_SOLID;

          Tile_t* top_wall_top = world->tilemap.tiles[world->tilemap.height - 1] + i;
          Tile_t* top_wall_bottom = world->tilemap.tiles[world->tilemap.height - 2] + i;

          top_wall_top->id = 0;
          top_wall_bottom->id = 1;
          top_wall_top->rotation = 0;
          top_wall_bottom->rotation = 0;
          top_wall_top->flags |= TILE_FLAG_SOLID;
          top_wall_bottom->flags |= TILE_FLAG_SOLID;
     }

     for(S16 i = 0; i < world->tilemap.height; i++){
          Tile_t* bottom_wall_top = world->tilemap.tiles[i] + 0;
          Tile_t* bottom_wall_bottom = world->tilemap.tiles[i] + 1;

          bottom_wall_top->id = 0;
          bottom_wall_bottom->id = 1;
          bottom_wall_top->rotation = 3;
          bottom_wall_bottom->rotation = 3;
          bottom_wall_top->flags |= TILE_FLAG_SOLID;
          bottom_wall_bottom->flags |= TILE_FLAG_SOLID;

          Tile_t* top_wall_top = world->tilemap.tiles[i] + (world->tilemap.width - 1);
          Tile_t* top_wall_bottom = world->tilemap.tiles[i] + (world->tilemap.width - 2);

          top_wall_top->id = 0;
          top_wall_bottom->id = 1;
          top_wall_top->rotation = 1;
          top_wall_bottom->rotation = 1;
          top_wall_top->flags |= TILE_FLAG_SOLID;
          top_wall_bottom->flags |= TILE_FLAG_SOLID;
     }

     world->tilemap.tiles[0][0].id = 3;
     world->tilemap.tiles[0][0].rotation = 0;

     world->tilemap.tiles[0][1].id = 0;
     world->tilemap.tiles[0][1].rotation = 2;

     world->tilemap.tiles[1][0].id = 0;
     world->tilemap.tiles[1][0].rotation = 3;

     world->tilemap.tiles[1][1].id = 2;
     world->tilemap.tiles[1][1].rotation = 0;

     world->tilemap.tiles[0][15].id = 0;
     world->tilemap.tiles[0][15].rotation = 2;

     world->tilemap.tiles[0][16].id = 3;
     world->tilemap.tiles[0][16].rotation = 3;

     world->tilemap.tiles[1][15].id = 2;
     world->tilemap.tiles[1][15].rotation = 3;

     world->tilemap.tiles[1][16].id = 0;
     world->tilemap.tiles[1][16].rotation = 1;

     world->tilemap.tiles[15][0].id = 0;
     world->tilemap.tiles[15][0].rotation = 3;

     world->tilemap.tiles[15][1].id = 2;
     world->tilemap.tiles[15][1].rotation = 1;

     world->tilemap.tiles[16][0].id = 3;
     world->tilemap.tiles[16][0].rotation = 1;

     world->tilemap.tiles[16][1].id = 0;
     world->tilemap.tiles[16][1].rotation = 0;

     world->tilemap.tiles[15][15].id = 2;
     world->tilemap.tiles[15][15].rotation = 2;

     world->tilemap.tiles[15][16].id = 0;
     world->tilemap.tiles[15][16].rotation = 1;

     world->tilemap.tiles[16][15].id = 0;
     world->tilemap.tiles[16][15].rotation = 0;

     world->tilemap.tiles[16][16].id = 3;
     world->tilemap.tiles[16][16].rotation = 2;

     if(!init(&world->interactives, 1)){
          return false;
     }

     world->interactives.elements[0].coord.x = -1;
     world->interactives.elements[0].coord.y = -1;

     if(!init(&world->blocks, 1)){
          return false;
     }

     world->blocks.elements[0].pos = coord_to_pos(Coord_t{-1, -1});

     return true;
}

void reset_tilemap_light(World_t* world){
     // TODO: this will need to be optimized when the map is much bigger
     for(S16 j = 0; j < world->tilemap.height; j++){
          for(S16 i = 0; i < world->tilemap.width; i++){
               world->tilemap.tiles[j][i].light = BASE_LIGHT;
          }
     }
}

bool block_in_height_range_of_player(Block_t* block, Position_t player_pos){
     if(block->pos.z <= player_pos.z - HEIGHT_INTERVAL ||
        block->pos.z >= player_pos.z + (HEIGHT_INTERVAL * 2)) return false;
     return true;
}

F32 momentum_term(F32 mass, F32 vel){
     return 0.5 * mass * vel * vel;
}

F32 momentum_term(TransferMomentum_t* transfer_momentum){
     return momentum_term((F32)(transfer_momentum->mass), transfer_momentum->vel);
}

static S16 get_player_mass_on_block(World_t* world, Block_t* block){
     S16 mass = 0;
     auto block_rect = block_get_inclusive_rect(block);

     for(S16 p = 0; p < world->players.count; p++){
          auto player = world->players.elements + p;
          auto player_pos = player->teleport ? player->teleport_pos + player->teleport_pos_delta : player->pos + player->pos_delta;
          if(player_pos.z == (block->pos.z + HEIGHT_INTERVAL) && pixel_in_rect(player_pos.pixel, block_rect)){
               mass += PLAYER_MASS;
          }
     }

     return mass;
}

S16 get_block_stack_mass(World_t* world, Block_t* block){
     S16 mass = block_get_mass(block);
     mass += get_player_mass_on_block(world, block);

     if(block->element != ELEMENT_ICE && block->element != ELEMENT_ONLY_ICED){
          auto result = block_held_down_by_another_block(block->pos.pixel, block->pos.z, block->cut, world->block_qt, world->interactive_qt, &world->tilemap, 0, false);
          for(S16 i = 0; i < result.count; i++){
               // check earlier blocks we've processed to see if they are currently entangled and cloning of one of them
               bool cloning = false;
               for(S16 j = 0; j < i; j++){
                    if(blocks_are_entangled(result.blocks_held[i].block, result.blocks_held[j].block, &world->blocks) &&
                       result.blocks_held[i].block->clone_start.x != 0){
                         cloning = true;
                         break;
                    }
               }

               if(!cloning){
                    mass += get_block_stack_mass(world, result.blocks_held[i].block);
                    mass += get_player_mass_on_block(world, result.blocks_held[i].block);
               }
          }
     }

     return mass;
}

TransferMomentum_t get_block_momentum(World_t* world, Block_t* block, Direction_t direction, bool prev){
     TransferMomentum_t block_momentum;

     block_momentum.vel = 0;
     block_momentum.mass = get_block_stack_mass(world, block);

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          if(prev){
               block_momentum.vel = block->prev_vel.x;
          }else{
               block_momentum.vel = block->vel.x;
          }
          break;
     case DIRECTION_UP:
     case DIRECTION_DOWN:
          if(prev){
               block_momentum.vel = block->prev_vel.y;
          }else{
               block_momentum.vel = block->vel.y;
          }
          break;
     }

     return block_momentum;
}

ElasticCollisionResult_t elastic_transfer_momentum(F32 mass_1, F32 vel_i_1, F32 mass_2, F32 vel_i_2){
     ElasticCollisionResult_t result;

     // 2 equasions we can use together to find answer for final velocities
     // m1 v1i + m2 v2i = m1 v1f + m2 v2f
     // v1i + v1f = v2i + v2f
     // v1f = v2i + v2f - v1i

     // v1i - v2i = v2f - v1f

     // substitute v1f in terms of v2f
     // m1 v1i + m2 v2i = m1 (v2i + v2f - v1i) + m2 v2f

     // distribute to isolate v2f
     // m1 v1i + m2 v2i = m1 v2i + m1 v2f - m1 v1i + m2 v2f
     // m1 v1i + m2 v2i - m1 v2i + m1 v1i = m1 v2f + m2 v2f
     // m1 v1i + m2 v2i - m1 v2i + m1 v1i = v2f (m1 + m2)
     // (m1 v1i + m2 v2i - m1 v2i + m1 v1i) / (m1 + m2) = v2f

     F32 initial_momentums = mass_1 * vel_i_1 + mass_2 * vel_i_2;
     F32 brought_over_to_left = -(mass_1 * vel_i_2) + mass_1 * vel_i_1;
     F32 divided_by = mass_1 + mass_2;

     result.second_final_velocity = (initial_momentums + brought_over_to_left) / divided_by;
     result.first_final_velocity = vel_i_2 + result.second_final_velocity - vel_i_1;

     // LOG("m1: %f, v1: %f, m2: %f, v2: %f, v1f: %f, v2f: %f\n",
     //     mass_1, vel_i_1, mass_2, vel_i_2, result.first_final_velocity, result.second_final_velocity);

     return result;
}

ElasticCollisionResult_t elastic_transfer_momentum_to_block(TransferMomentum_t* first_transfer_momentum, World_t* world, Block_t* block, Direction_t direction,
                                                            S16 block_contributing_momentum_to_total_blocks){
     auto second_block_momentum = get_block_momentum(world, block, direction);
     second_block_momentum.mass /= block_contributing_momentum_to_total_blocks;
     auto result = elastic_transfer_momentum(first_transfer_momentum->mass, first_transfer_momentum->vel, second_block_momentum.mass, second_block_momentum.vel);

     // LOG("  elastic transfer from block with %d %f to block %d with %d %f results: %f, %f in the %s\n", first_transfer_momentum->mass, first_transfer_momentum->vel,
     //     get_block_index(world, block), second_block_momentum.mass, second_block_momentum.vel, result.first_final_velocity, result.second_final_velocity,
     //     direction_is_horizontal(direction) ? "x" : "y");

     return result;
}

static void get_touching_blocks_in_direction(World_t* world, Block_t* block, Direction_t direction, BlockList_t* block_list,
                                             bool require_on_ice = true){
     auto result = block_against_other_blocks(block->pos + block->pos_delta, block->cut, direction, world->block_qt,
                                              world->interactive_qt, &world->tilemap);
     for(S16 i = 0; i < result.count; i++){
          Direction_t result_direction = direction;
          result_direction = direction_rotate_clockwise(result_direction, result.objects[i].rotations_through_portal);
          auto result_block = result.objects[i].block;

          if((require_on_ice && block_on_ice(result_block->pos, result_block->pos_delta, result_block->cut,
                                             &world->tilemap, world->interactive_qt, world->block_qt)) ||
              !require_on_ice){
               get_block_stack(world, result_block, block_list, result.objects[i].rotations_through_portal);
               get_touching_blocks_in_direction(world, result_block, result_direction, block_list, require_on_ice);
          }
     }
}

void get_block_stack(World_t* world, Block_t* block, BlockList_t* block_list, S8 rotations_through_portal){
     block_list->add(block, rotations_through_portal);

     if(block->element != ELEMENT_ICE && block->element != ELEMENT_ONLY_ICED){
          auto result = block_held_down_by_another_block(block, world->block_qt, world->interactive_qt, &world->tilemap);
          for(S16 i = 0; i < result.count; i++){
               get_block_stack(world, result.blocks_held[i].block, block_list, rotations_through_portal);
          }
     }
}

S16 get_block_mass_in_direction(World_t* world, Block_t* block, Direction_t direction, bool require_on_ice){
     BlockList_t block_list;
     get_block_stack(world, block, &block_list, DIRECTION_COUNT);

     if((require_on_ice && block_on_ice(block->pos, block->pos_delta, block->cut,
                                       &world->tilemap, world->interactive_qt, world->block_qt)) ||
        !require_on_ice){
          get_touching_blocks_in_direction(world, block, direction, &block_list, require_on_ice);

          // accumulate all blocks mass but reduce duplication of mass for entangled blocks
          // TODO: n^2 * m, if we sort the blocks we can speed this up using a binary search bringing it to n log n * m
          for(S16 i = 0; i < block_list.count; i++){
               auto* block_entry = block_list.entries + i;

               S16 entangle_index = block_entry->block->entangle_index;
               S16 prev_entangle_index = -1;
               while(entangle_index != i && prev_entangle_index != entangle_index && entangle_index >= 0){
                    prev_entangle_index = entangle_index;
                    for(S16 j = i + 1; j < block_list.count; j++){
                         auto* block_entry_itr = block_list.entries + j;
                         if(entangle_index == get_block_index(world, block_entry_itr->block)){
                              S8 final_rotation = block_entry_itr->block->rotation - block_entry_itr->rotations_through_portal;
                              S8 rotation_between = direction_rotations_between((Direction_t)(block_entry->block->rotation), (Direction_t)(final_rotation)) % DIRECTION_COUNT;

                              if(rotation_between == 0){
                                   block_entry_itr->counted = false;
                                   entangle_index = block_entry_itr->block->entangle_index;
                              }
                         }
                    }
               }
          }
     }

     // count the rest of the blocks if allowed
     S16 mass = 0;
     for(S16 i = 0; i < block_list.count; i++){
          auto* block_entry = block_list.entries + i;
          if(block_entry->counted){
               mass += block_get_mass(block_entry->block);

               auto against_player = block_against_player(block_entry->block, direction, &world->players);
               if(against_player){
                    mass += PLAYER_MASS;
               }
          }
     }

     return mass;
}

AllowedToPushResult_t allowed_to_push(World_t* world, Block_t* block, Direction_t direction, F32 force, TransferMomentum_t* instant_momentum){
     AllowedToPushResult_t result;
     result.push = true;

     S16 block_width = block_get_width_in_pixels(block);
     S16 block_height = block_get_height_in_pixels(block);

     F32 total_block_mass = get_block_mass_in_direction(world, block, direction);
     result.mass_ratio = (F32)(block_width * block_height) / (F32)(total_block_mass);

     if(block_on_ice(block->pos, Vec_t{}, block->cut, &world->tilemap, world->interactive_qt, world->block_qt)){
          // player applies a force to accelerate the block by BLOCK_ACCEL
          if(instant_momentum){
               auto elastic_result = elastic_transfer_momentum_to_block(instant_momentum, world, block, direction);
               result.push = collision_result_overcomes_friction(block->vel.y, elastic_result.second_final_velocity, total_block_mass);
          }else{
               F32 accel_time = BLOCK_ACCEL_TIME;

               if(block_width == HALF_TILE_SIZE_IN_PIXELS || block_height == HALF_TILE_SIZE_IN_PIXELS) accel_time *= SMALL_BLOCK_ACCEL_MULTIPLIER;

               // TODO: why do we use a .y here?
               F32 block_acceleration = (result.mass_ratio * BLOCK_ACCEL((block_center_pixel_offset(block->cut).y * PIXEL_SIZE), accel_time));
               F32 applied_force = (F32)(total_block_mass) * block_acceleration / BLOCK_ACCEL_TIME;
               F32 static_friction = 0;

               if(direction_is_horizontal(direction) && block->vel.x == 0){
                    static_friction = get_block_static_friction(total_block_mass);
               }else if(!direction_is_horizontal(direction) && block->vel.y == 0){
                    static_friction = get_block_static_friction(total_block_mass);
               }

               if(applied_force < static_friction) result.push = false;
          }
     }else{
          if(force < 1.0f) result.push = false;
     }

     return result;
}

PushFromEntangler_t build_push_from_entangler(Block_t* block, Direction_t push_dir, F32 force){
     PushFromEntangler_t from_entangler;

     if(direction_is_horizontal(push_dir)){
         from_entangler.accel = block->accel.x / force;
         from_entangler.move_time_left = block->horizontal_move.time_left;
         from_entangler.coast_vel = block->coast_vel.x / force;
     }else{
         from_entangler.accel = block->accel.y / force;
         from_entangler.move_time_left = block->vertical_move.time_left;
         from_entangler.coast_vel = block->coast_vel.y / force;
     }
     from_entangler.cut = block->cut;
     return from_entangler;
}

Vec_t get_block_momentum_vel(World_t* world, Block_t* block){
     Vec_t block_vel = block->vel;

     if(block->horizontal_momentum == BLOCK_MOMENTUM_SUM){
          auto mass = get_block_stack_mass(world, block);
          block_vel.x = (block->impact_momentum.x + block->kickback_momentum.x) / mass;
     }else if(block->horizontal_momentum == BLOCK_MOMENTUM_STOP){
          block_vel.x = 0;
     }
     if(block->vertical_momentum == BLOCK_MOMENTUM_SUM){
          auto mass = get_block_stack_mass(world, block);
          block_vel.y = (block->impact_momentum.y + block->kickback_momentum.y) / mass;
     }else if(block->vertical_momentum == BLOCK_MOMENTUM_STOP){
          block_vel.y = 0;
     }

     return block_vel;
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

void world_expand_editor_camera(World_t* world){
     S16 quarter_width = (world->editor_camera_bounds.right - world->editor_camera_bounds.left) / 4;
     S16 quarter_height = (world->editor_camera_bounds.top - world->editor_camera_bounds.bottom) / 4;

     world->editor_camera_bounds.left -= quarter_width;
     world->editor_camera_bounds.right += quarter_width;
     world->editor_camera_bounds.bottom -= quarter_height;
     world->editor_camera_bounds.top += quarter_height;
}

void world_shrink_editor_camera(World_t* world){
     S16 eighth_width = (world->editor_camera_bounds.right - world->editor_camera_bounds.left) / 8;
     S16 eighth_height = (world->editor_camera_bounds.top - world->editor_camera_bounds.bottom) / 8;

     world->editor_camera_bounds.left += eighth_width;
     world->editor_camera_bounds.right -= eighth_width;
     world->editor_camera_bounds.bottom += eighth_height;
     world->editor_camera_bounds.top -= eighth_height;
}

void world_move_editor_camera(World_t* world, Direction_t direction){
     S16 quarter_width = (world->editor_camera_bounds.right - world->editor_camera_bounds.left) / 4;
     S16 quarter_height = (world->editor_camera_bounds.top - world->editor_camera_bounds.bottom) / 4;

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          world->editor_camera_bounds.left -= quarter_width;
          world->editor_camera_bounds.right -= quarter_width;
          break;
     case DIRECTION_RIGHT:
          world->editor_camera_bounds.left += quarter_width;
          world->editor_camera_bounds.right += quarter_width;
          break;
     case DIRECTION_DOWN:
          world->editor_camera_bounds.bottom -= quarter_height;
          world->editor_camera_bounds.top -= quarter_height;
          break;
     case DIRECTION_UP:
          world->editor_camera_bounds.bottom += quarter_height;
          world->editor_camera_bounds.top += quarter_height;
          break;
     }
}

void world_recalculate_camera_on_world_bounds(World_t* world){
     world->recalc_room_camera = true;
     world->editor_camera_bounds = Rect_t{0, 0, world->tilemap.width, world->tilemap.height};
}
