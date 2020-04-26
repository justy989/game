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
     if(!d) return result;
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

     LOG("load map %s\n", filepath);
     result.success = load_map(filepath, player_start, &world->tilemap, &world->blocks, &world->interactives);
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

static void toggle_electricity(TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree, Coord_t coord,
                               Direction_t direction, bool from_wire, bool activated_by_door){
     Coord_t adjacent_coord = coord + direction;
     Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
     if(!tile) return;

     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, adjacent_coord);
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
               if(!activated_by_door) toggle_electricity(tilemap, interactive_quad_tree,
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
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_LEFT, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT && direction != DIRECTION_LEFT){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_RIGHT, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN && direction != DIRECTION_UP){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_DOWN, true, false);
               }

               if(interactive->wire_cross.mask & DIRECTION_MASK_UP && direction != DIRECTION_DOWN){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_UP, true, false);
               }
          }else{
               if(tile->flags & TILE_FLAG_WIRE_LEFT && direction != DIRECTION_RIGHT){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_LEFT, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_RIGHT && direction != DIRECTION_LEFT){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_RIGHT, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_DOWN && direction != DIRECTION_UP){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_DOWN, true, false);
               }

               if(tile->flags & TILE_FLAG_WIRE_UP && direction != DIRECTION_DOWN){
                    toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, DIRECTION_UP, true, false);
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
               toggle_electricity(tilemap, interactive_quad_tree, adjacent_coord, cluster_direction, true, false);
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
     if(!block_on_ice(block->pos, block->pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt) &&
        !block_on_air(block, &world->tilemap, world->interactive_qt, world->block_qt)) return;

     Move_t* move = direction_is_horizontal(direction) ? &block->horizontal_move : &block->vertical_move;

     if(move->state == MOVE_STATE_STOPPING || move->state == MOVE_STATE_IDLING) return;

     move->state = MOVE_STATE_STOPPING;
     move->distance = 0;

     auto pos = pos_to_vec(block->pos);
     auto block_mass = get_block_stack_mass(world, block);
     F32 block_vel = 0;

     add_global_tag(TAB_PLAYER_STOPS_COASTING_BLOCK);

     if(direction_is_horizontal(direction)){
          block->stopped_by_player_horizontal = true;
          auto motion = motion_x_component(block);
          block->accel.x = begin_stopping_grid_aligned_motion(block->cut, &motion, pos.x, block_mass); // adjust by one tick since we missed this update
          block_vel = block->vel.x;
     }else{
          block->stopped_by_player_vertical = true;
          auto motion = motion_y_component(block);
          block->accel.y = begin_stopping_grid_aligned_motion(block->cut, &motion, pos.y, block_mass); // adjust by one tick since we missed this update
          block_vel = block->vel.y;
     }

     auto against_result = block_against_other_blocks(block->pos + block->pos_delta, block->cut, direction_opposite(direction),
                                                      world->block_qt, world->interactive_qt, &world->tilemap);
     for(S16 i = 0; i < against_result.count; i++){
         Direction_t against_direction = direction_rotate_clockwise(direction, against_result.againsts[i].rotations_through_portal);
         Block_t* against_block = against_result.againsts[i].block;

         // we don't want to impact a block that is colliding with the block as we are stopping it, but rather a
         // gaggle of blocks sliding together that the player is stopping
         if(direction_is_horizontal(against_direction)){
             if(block_vel != against_block->vel.x) continue;
         }else{
             if(block_vel != against_block->vel.y) continue;
         }

         slow_block_toward_gridlock(world, against_block, against_direction);
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
         auto* found_block = found_blocks.blocks + i;

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

bool player_against_solid_interactive(Player_t* player, Direction_t direction, QuadTreeNode_t<Interactive_t>* interactive_qt){
     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     Coord_t coord = pixel_to_coord(pos_a.pixel);
     Interactive_t* interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)) return true;
          if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on && player->pos.z > PORTAL_MAX_HEIGHT) return true;
     }

     coord = pixel_to_coord(pos_b.pixel);
     interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive){
          if(interactive_is_solid(interactive)) return true;
          if(interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on && player->pos.z > PORTAL_MAX_HEIGHT) return true;
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

void stop_against_blocks_moving_with_block(World_t* world, Block_t* block, Direction_t direction, Position_t adjusted_stop_pos){
    if(direction_is_horizontal(direction)){
        reset_move(&block->horizontal_move);
        block->vel.x = 0;
        block->accel.x = 0;
        block->stopped_by_player_horizontal = false;
    }else{
        reset_move(&block->vertical_move);
        block->vel.y = 0;
        block->accel.y = 0;
        block->stopped_by_player_vertical = false;
    }

    Position_t block_center = block_get_center(adjusted_stop_pos, block->cut);
    auto against_result = block_against_other_blocks(block->pos + block->pos_delta, block->cut, direction,
                                                     world->block_qt, world->interactive_qt, &world->tilemap);
    for(S16 i = 0; i < against_result.count; i++){
        Direction_t against_direction = direction_rotate_clockwise(direction, against_result.againsts[i].rotations_through_portal);
        Block_t* against_block = against_result.againsts[i].block;
        Position_t against_center = block_get_center(against_block);
        Position_t new_against_center = against_center;

        switch(against_direction)
        {
        default:
            break;
        case DIRECTION_LEFT:
            new_against_center.pixel.x = block_center.pixel.x - block_get_width_in_pixels(against_block->cut);
            new_against_center.decimal.x = block_center.decimal.x;
            against_block->pos_delta.x = pos_to_vec(new_against_center - against_center).x;
            break;
        case DIRECTION_RIGHT:
            new_against_center.pixel.x = block_center.pixel.x + block_get_width_in_pixels(against_block->cut);
            new_against_center.decimal.x = block_center.decimal.x;
            against_block->pos_delta.x = pos_to_vec(new_against_center - against_center).x;
            break;
        case DIRECTION_DOWN:
            new_against_center.pixel.y = block_center.pixel.y - block_get_height_in_pixels(against_block->cut);
            new_against_center.decimal.y = block_center.decimal.y;
            against_block->pos_delta.y = pos_to_vec(new_against_center - against_center).y;
            break;
        case DIRECTION_UP:
            new_against_center.pixel.y = block_center.pixel.y + block_get_height_in_pixels(against_block->cut);
            new_against_center.decimal.y = block_center.decimal.y;
            against_block->pos_delta.y = pos_to_vec(new_against_center - against_center).y;
            break;
        }

        stop_against_blocks_moving_with_block(world, against_block, against_direction, against_block->pos + against_block->pos_delta);
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
         auto* found_block = found_blocks.blocks + i;

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
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt);
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
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, block_new_pos);
                              collision.block->pos_delta.x = new_pos_delta;
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         F32 block_width = block_get_width_in_pixels(collision.block) * PIXEL_SIZE;
                         auto new_pos = collision.pos + Vec_t{block_width + PLAYER_RADIUS, 0};
                         result.pos_delta.x = pos_to_vec(new_pos - player_pos).x;

                         if(momentum < PLAYER_SQUISH_MOMENTUM){
                              auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                              slow_block_toward_gridlock(world, collision.block, slow_direction);
                              player_slowing_down = true;
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
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt);
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
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, block_new_pos);
                              collision.block->pos_delta.x = new_pos_delta;
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         auto new_pos = collision.pos - Vec_t{PLAYER_RADIUS, 0};
                         result.pos_delta.x = pos_to_vec(new_pos - player_pos).x;

                         if(momentum < PLAYER_SQUISH_MOMENTUM){
                              auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                              slow_block_toward_gridlock(world, collision.block, slow_direction);
                              player_slowing_down = true;
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
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt);
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
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, block_new_pos);
                              collision.block->pos_delta.y = new_pos_delta;

                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         auto new_pos = collision.pos - Vec_t{0, PLAYER_RADIUS};
                         result.pos_delta.y = pos_to_vec(new_pos - player_pos).y;

                         if(momentum < PLAYER_SQUISH_MOMENTUM){
                              auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                              slow_block_toward_gridlock(world, collision.block, slow_direction);
                              player_slowing_down = true;
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
                         would_squish = player_against_solid_interactive(player, check_dir, world->interactive_qt);
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
                              stop_against_blocks_moving_with_block(world, collision.block, collision.dir, block_new_pos);
                              collision.block->pos_delta.y = new_pos_delta;
                         }
                    }else if(!(collision.block->pos.z > player->pos.z && block_held_up_by_another_block(collision.block, world->block_qt, world->interactive_qt, &world->tilemap).held())){
                         F32 block_height = block_get_height_in_pixels(collision.block) * PIXEL_SIZE;
                         auto new_pos = collision.pos + Vec_t{0, block_height + PLAYER_RADIUS};
                         result.pos_delta.y = pos_to_vec(new_pos - player_pos).y;

                         if(momentum < PLAYER_SQUISH_MOMENTUM){
                              auto slow_direction = direction_rotate_counter_clockwise(direction_opposite(collision.dir), collision.portal_rotations);
                              slow_block_toward_gridlock(world, collision.block, slow_direction);
                              player_slowing_down = true;
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
               if(world->tilemap.tiles[y][x].id){
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
               if(interactive){
                    bool collided = false;
                    bool empty_pit = true;
                    if(interactive->type == INTERACTIVE_TYPE_PIT){
                         Rect_t pit_rect = rect_surrounding_coord(coord);
                         quad_tree_find_in(world->block_qt, pit_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                         bool cover_top_left = false;
                         bool cover_top_right = false;
                         bool cover_bottom_left = false;
                         bool cover_bottom_right = false;

                         Rect_t top_left_corner {pit_rect.left, (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS), (S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS - 1), pit_rect.top};
                         Rect_t top_right_corner {(S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS), (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS), pit_rect.right, pit_rect.top};
                         Rect_t bottom_left_corner {pit_rect.left, pit_rect.bottom, (S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS - 1), (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS - 1)};
                         Rect_t bottom_right_corner {(S16)(pit_rect.left + HALF_TILE_SIZE_IN_PIXELS), pit_rect.bottom, pit_rect.right, (S16)(pit_rect.bottom + HALF_TILE_SIZE_IN_PIXELS - 1)};

                         for(S16 b = 0; b < block_count; b++){
                              if(blocks[b]->pos.z > -HEIGHT_INTERVAL) continue;

                              auto block_rect = block_get_inclusive_rect(blocks[b]);

                              if(!rect_completely_in_rect(block_rect, pit_rect)) continue;

                              if(rect_in_rect(top_left_corner, block_rect)){
                                  cover_top_left = true;
                                  empty_pit = false;
                              }
                              if(rect_in_rect(top_right_corner, block_rect)){
                                  cover_top_right = true;
                                  empty_pit = false;
                              }
                              if(rect_in_rect(bottom_left_corner, block_rect)){
                                  cover_bottom_left = true;
                                  empty_pit = false;
                              }
                              if(rect_in_rect(bottom_right_corner, block_rect)){
                                  cover_bottom_right = true;
                                  empty_pit = false;
                              }
                         }

                         if(!cover_top_left){
                             position_slide_against_rect(player_pos, top_left_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!cover_top_right){
                             position_slide_against_rect(player_pos, top_right_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!cover_bottom_left){
                             position_slide_against_rect(player_pos, bottom_left_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }

                         if(!cover_bottom_right){
                             position_slide_against_rect(player_pos, bottom_right_corner, PLAYER_RADIUS, &result.pos_delta, &collided);
                             if(collided && !result.collided){
                                  result.collided = true;
                                  collided_interactive_dir = direction_between(player_coord, coord);
                             }
                         }
                    }

                    if(empty_pit){
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
               S16 px = coords[i].x * TILE_SIZE_IN_PIXELS;
               S16 py = coords[i].y * TILE_SIZE_IN_PIXELS;
               Rect_t coord_rect {px, py, (S16)(px + TILE_SIZE_IN_PIXELS), (S16)(py + TILE_SIZE_IN_PIXELS)};

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
               if(tile && !tile_is_solid(tile)){
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

                    if(is_active_portal(interactive) && !teleported){
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
          }
     }
}

void spread_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported){
     impact_ice(center, height, radius, world, teleported, true);
}

void melt_ice(Coord_t center, S8 height, S16 radius, World_t* world, bool teleported){
     impact_ice(center, height, radius, world, teleported, false);
}

BlockPushMoveDirectionResult_t block_push(Block_t* block, MoveDirection_t move_direction, World_t* world, bool pushed_by_ice,
                                          F32 force, TransferMomentum_t* instant_momentum, PushFromEntangler_t* from_entangler){
     Direction_t first;
     Direction_t second;
     move_direction_to_directions(move_direction, &first, &second);

     BlockPushResult_t a = {};
     BlockPushResult_t b = {};

     if(first != DIRECTION_COUNT){
          a = block_push(block, first, world, pushed_by_ice, force, instant_momentum, from_entangler);
     }

     if(second != DIRECTION_COUNT){
          b = block_push(block, second, world, pushed_by_ice, force, instant_momentum, from_entangler);
     }

     BlockPushMoveDirectionResult_t result {a, b};

     return result;
}

BlockPushResult_t block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum,
                             PushFromEntangler_t* from_entangler){
     return block_push(block, block->pos, block->pos_delta, direction, world, pushed_by_ice, force, instant_momentum, from_entangler);
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

void apply_push_horizontal(Block_t* block, Position_t pos, World_t* world, Direction_t direction, TransferMomentum_t* instant_momentum,
                           bool pushed_by_ice, F32 force, PushFromEntangler_t* from_entangler, BlockPushResult_t* result){
      auto original_move_state = block->horizontal_move.state;
      if(pushed_by_ice){
           auto elastic_result = elastic_transfer_momentum_to_block(instant_momentum, world, block, direction);
           if(collision_result_overcomes_friction(block->vel.x, elastic_result.second_final_velocity, get_block_stack_mass(world, block))){
                add_global_tag(TAB_BLOCK_MOMENTUM_COLLISION);
                auto instant_vel = elastic_result.second_final_velocity;
                auto pushee_momentum = get_block_momentum(world, block, direction);
                result->add_collision(instant_momentum->mass, elastic_result.first_final_velocity, pushee_momentum.mass, pushee_momentum.vel, elastic_result.second_final_velocity);
                if(direction == DIRECTION_LEFT){
                    if(instant_vel > 0) instant_vel = -instant_vel;
                }else if(direction == DIRECTION_RIGHT){
                    if(instant_vel < 0) instant_vel = -instant_vel;
                }
                block->vel.x = instant_vel;
                block->horizontal_move.sign = move_sign_from_vel(block->vel.x);
                block->horizontal_move.state = MOVE_STATE_COASTING;

                auto motion = motion_x_component(block);
                F32 x_pos = pos_to_vec(block->pos).x;
                block->horizontal_move.time_left = calc_coast_motion_time_left(block->cut, &motion, x_pos);
                if(direction == DIRECTION_LEFT) block->horizontal_move.time_left = -block->horizontal_move.time_left;
                block->stopped_by_player_horizontal = false;
           }else{
                return;
           }
      }else if(block->horizontal_move.state == MOVE_STATE_IDLING || block->horizontal_move.state == MOVE_STATE_COASTING){
           block->horizontal_move.state = MOVE_STATE_STARTING;
           block->horizontal_move.sign = (direction == DIRECTION_LEFT) ? MOVE_SIGN_NEGATIVE : MOVE_SIGN_POSITIVE;

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

               F32 half_distance_to_next_grid_center = calc_half_distance_to_next_grid_center(block->pos.pixel.x,
                                                                                              block->pos.decimal.x,
                                                                                              lower_dim,
                                                                                              direction == DIRECTION_RIGHT);
               F32 velocity_ratio = get_block_velocity_ratio(world, block, block->vel.x, force);

               block->accel.x = calc_accel_from_stop(half_distance_to_next_grid_center, accel_time) * force;

               F32 ideal_accel = calc_accel_from_stop((lower_dim / 2) * PIXEL_SIZE, accel_time) * force;

               if(direction == DIRECTION_LEFT){
                   ideal_accel = -ideal_accel;
                   block->accel.x = -block->accel.x;
               }

               block->coast_vel.x = calc_velocity_motion(0, ideal_accel, accel_time);
               block->horizontal_move.time_left = accel_time - (velocity_ratio * accel_time);
           }
      }else{
           result->busy = true;
           return;
      }

      block->started_on_pixel_x = pos.pixel.x;
}

void apply_push_vertical(Block_t* block, Position_t pos, World_t* world, Direction_t direction, TransferMomentum_t* instant_momentum,
                         bool pushed_by_ice, F32 force, PushFromEntangler_t* from_entangler, BlockPushResult_t* result){
      auto original_move_state = block->vertical_move.state;
      if(pushed_by_ice){
           auto elastic_result = elastic_transfer_momentum_to_block(instant_momentum, world, block, direction);
           if(collision_result_overcomes_friction(block->vel.y, elastic_result.second_final_velocity, get_block_stack_mass(world, block))){
                add_global_tag(TAB_BLOCK_MOMENTUM_COLLISION);
                auto instant_vel = elastic_result.second_final_velocity;
                auto pushee_momentum = get_block_momentum(world, block, direction);
                result->add_collision(instant_momentum->mass, elastic_result.first_final_velocity, pushee_momentum.mass, pushee_momentum.vel, elastic_result.second_final_velocity);
                if(direction == DIRECTION_DOWN){
                    if(instant_vel > 0) instant_vel = -instant_vel;
                }else if(direction == DIRECTION_UP){
                    if(instant_vel < 0) instant_vel = -instant_vel;
                }
                block->vel.y = instant_vel;
                block->vertical_move.sign = move_sign_from_vel(block->vel.y);
                block->vertical_move.state = MOVE_STATE_COASTING;

                auto motion = motion_y_component(block);
                F32 y_pos = pos_to_vec(block->pos).y;
                block->vertical_move.time_left = calc_coast_motion_time_left(block->cut, &motion, y_pos);
                if(direction == DIRECTION_DOWN) block->vertical_move.time_left = -block->vertical_move.time_left;
                block->stopped_by_player_vertical = false;
           }else{
                return;
           }
      }else if(block->vertical_move.state == MOVE_STATE_IDLING || block->vertical_move.state == MOVE_STATE_COASTING){
           block->vertical_move.state = MOVE_STATE_STARTING;
           block->vertical_move.sign = (direction == DIRECTION_DOWN) ? MOVE_SIGN_NEGATIVE : MOVE_SIGN_POSITIVE;

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

               F32 half_distance_to_next_grid_center = calc_half_distance_to_next_grid_center(block->pos.pixel.y,
                                                                                              block->pos.decimal.y,
                                                                                              lower_dim,
                                                                                              direction == DIRECTION_UP);
               F32 velocity_ratio = get_block_velocity_ratio(world, block, block->vel.y, force);

               block->accel.y = calc_accel_from_stop(half_distance_to_next_grid_center, accel_time) * force;

               F32 ideal_accel = calc_accel_from_stop((lower_dim / 2) * PIXEL_SIZE, accel_time) * force;
               if(direction == DIRECTION_DOWN){
                   ideal_accel = -ideal_accel;
                   block->accel.y = -block->accel.y;
               }
               block->coast_vel.y = calc_velocity_motion(0, ideal_accel, accel_time);
               block->vertical_move.time_left = accel_time - (velocity_ratio * accel_time);
           }
      }else{
           result->busy = true;
           return;
      }

      block->started_on_pixel_y = pos.pixel.y;
}

void update_block_momentum_from_push(Block_t* block, Direction_t direction, PushFromEntangler_t* from_entangler, BlockPushResult_t* push_result, BlockPushResult_t* final_result){
     if(direction_is_horizontal(direction)){
         bool transferred_momentum_back = false;

         for(S16 c = 0; c < push_result->collision_count; c++){
             auto& collision = push_result->collisions[c];

             if(collision.transferred_momentum_back()){
                 transferred_momentum_back = true;

                 // TODO: how do we handle multiple collisions transferring momentum back?
                 if(from_entangler){
                      block->vel.x = collision.pushee_velocity;
                      block->horizontal_move.state = MOVE_STATE_COASTING;
                      block->horizontal_move.sign = move_sign_from_vel(block->vel.x);
                      final_result->pushed = true;
                 }else{
                      final_result->add_collision(collision.pusher_mass, collision.pusher_velocity, collision.pushee_mass, collision.pushee_initial_velocity, collision.pushee_velocity);
                      // reset_move(&block->horizontal_move);
                      final_result->pushed = false;
                 }
             }
         }

         if(!transferred_momentum_back) reset_move(&block->horizontal_move);
     }else{
         bool transferred_momentum_back = false;

         for(S16 c = 0; c < push_result->collision_count; c++){
             auto& collision = push_result->collisions[c];
             if(collision.transferred_momentum_back()){
                 transferred_momentum_back = true;

                 if(from_entangler){
                      block->vel.y = collision.pushee_velocity;
                      block->vertical_move.state = MOVE_STATE_COASTING;
                      block->vertical_move.sign = move_sign_from_vel(block->vel.x);
                      final_result->pushed = true;
                 }else{
                      final_result->add_collision(collision.pusher_mass, collision.pusher_velocity, collision.pushee_mass, collision.pushee_initial_velocity, collision.pushee_velocity);
                      // reset_move(&block->vertical_move);
                      final_result->pushed = false;
                 }
             }
         }

         if(!transferred_momentum_back) reset_move(&block->vertical_move);
     }
}

bool check_entangled_against_block_able_to_move(Block_t* block, Direction_t direction, Block_t* against_block, Direction_t against_direction, bool both_on_ice, World_t* world){
     if(direction == DIRECTION_COUNT) return false;

     // if block is entangled with the block it collides with, check if the entangled block can move, this is kind of duplicate work
     bool only_against_entanglers = true;
     Block_t* entangled_against_block = against_block;
     while(entangled_against_block){
          Direction_t check_direction = against_direction;

          auto next_against_block = block_against_another_block(entangled_against_block->pos + entangled_against_block->pos_delta,
                                                                entangled_against_block->cut,
                                                                check_direction, world->block_qt,
                                                                world->interactive_qt, &world->tilemap,
                                                                &check_direction);
          if(next_against_block == nullptr) break;
          if(!blocks_are_entangled(entangled_against_block, next_against_block, &world->blocks) &&
             !block_on_ice(next_against_block->pos, entangled_against_block->pos_delta, entangled_against_block->cut,
                           &world->tilemap, world->interactive_qt, world->block_qt)){
               only_against_entanglers = false;
               break;
          }

          entangled_against_block = next_against_block;
     }

     if(!only_against_entanglers){
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
                                BlockAgainstOther_t* against, S16 against_count, World_t* world, BlockPushResult_t* result,
                                bool* transfers_force){
     Block_t* against_block = against->block;
     Direction_t first_direction;
     Direction_t second_direction;

     move_direction_to_directions(move_direction, &first_direction, &second_direction);

     Direction_t first_against_block_push_dir = direction_rotate_clockwise(first_direction, against->rotations_through_portal);
     Direction_t second_against_block_push_dir = direction_rotate_clockwise(second_direction, against->rotations_through_portal);

     MoveDirection_t final_move_direction = move_direction_from_directions(first_against_block_push_dir, second_against_block_push_dir);

     bool on_ice = false;
     bool both_on_ice = false;

     if(against_block == block){
          if(pushed_by_ice && block_on_ice(against_block->pos, against_block->pos_delta, against_block->cut,
                                           &world->tilemap, world->interactive_qt, world->block_qt)){
               // pass
          }else{
               return false;
          }
     }else if((on_ice = block_on_ice(against_block->pos, against_block->pos_delta, against_block->cut,
                                     &world->tilemap, world->interactive_qt, world->block_qt))){
          if(pushed_block_on_ice) both_on_ice = true;

          if(pushed_by_ice && instant_momentum){
               // check if we are able to move or if we transfer our force to the block
               F32 block_against_dir_vel = 0;

               switch(first_against_block_push_dir){
               default:
                    break;
               case DIRECTION_LEFT:
                    *transfers_force = (against_block->vel.x > instant_momentum->vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_UP:
                    *transfers_force = (against_block->vel.y < instant_momentum->vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               case DIRECTION_RIGHT:
                    *transfers_force = (against_block->vel.x < instant_momentum->vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_DOWN:
                    *transfers_force = (against_block->vel.y > instant_momentum->vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               }

               switch(second_against_block_push_dir){
               default:
                    break;
               case DIRECTION_LEFT:
                    *transfers_force = (against_block->vel.x > instant_momentum->vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_UP:
                    *transfers_force = (against_block->vel.y < instant_momentum->vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               case DIRECTION_RIGHT:
                    *transfers_force = (against_block->vel.x < instant_momentum->vel);
                    block_against_dir_vel = block->vel.x;
                    break;
               case DIRECTION_DOWN:
                    *transfers_force = (against_block->vel.y > instant_momentum->vel);
                    block_against_dir_vel = block->vel.y;
                    break;
               }

               if(*transfers_force){
                    auto split_instant_momentum = *instant_momentum;
                    split_instant_momentum.mass /= against_count;

                    // check if the momentum transfer overcomes static friction
                    auto block_mass = get_block_stack_mass(world, block) / against_count;
                    auto elastic_result = elastic_transfer_momentum(split_instant_momentum.mass, split_instant_momentum.vel, block_mass, block_against_dir_vel);

                    if(!collision_result_overcomes_friction(block_against_dir_vel, elastic_result.second_final_velocity, block_mass)){
                         return false;
                    }

                    auto push_result = block_push(against_block, final_move_direction, world, pushed_by_ice, force, &split_instant_momentum);

                    update_block_momentum_from_push(block, first_direction, from_entangler, &push_result.horizontal_result, result);

                    if(second_direction != DIRECTION_COUNT){
                         update_block_momentum_from_push(block, second_direction, from_entangler, &push_result.vertical_result, result);
                    }
               }
          }else if(pushed_block_on_ice){
               bool are_entangled = blocks_are_entangled(block, against_block, &world->blocks);

               if(are_entangled){
                    // if they are opposite entangled (potentially through a portal) then they just push into each other
                    S8 total_collided_rotations = direction_rotations_between(first_direction, first_against_block_push_dir) + against_block->rotation;
                    total_collided_rotations %= DIRECTION_COUNT;

                    auto rotations_between = direction_rotations_between((Direction_t)(total_collided_rotations), (Direction_t)(block->rotation));
                    if(rotations_between == 2){
                         return false;
                    }
               }

               auto push_result = block_push(against_block, final_move_direction, world, pushed_by_ice, force, instant_momentum);

               bool first_successful = push_result.horizontal_result.pushed;

               if(push_result.horizontal_result.pushed){
                    BlockPushedAgainst_t pushed_against {};
                    pushed_against.block = against_block;
                    pushed_against.direction = first_against_block_push_dir;
                    result->add_pushed_against(&pushed_against);
               }

               bool second_successful = push_result.vertical_result.pushed;

               if(push_result.vertical_result.pushed){
                    BlockPushedAgainst_t pushed_against {};
                    pushed_against.block = against_block;
                    pushed_against.direction = second_against_block_push_dir;
                    result->add_pushed_against(&pushed_against);
               }

               if(!first_successful && !second_successful){
                    return false;
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
              auto push_result = block_push(against_block, first_against_block_push_dir, world, pushed_by_ice, force, instant_momentum);

              first_successful = push_result.pushed;
          }

          if(second_direction != DIRECTION_COUNT){
               total_block_mass = get_block_mass_in_direction(world, block, second_direction, false);

               if(total_block_mass <= PLAYER_MAX_PUSH_MASS_ON_FRICTION){
                   add_global_tag(TAG_PLAYER_PUSHES_MORE_THAN_ONE_MASS);
                   auto push_result = block_push(against_block, second_against_block_push_dir, world, pushed_by_ice, force, instant_momentum);

                   second_successful = push_result.pushed;
               }
          }

          if(!first_successful && !second_successful){
               return false;
          }
     }

     return true;
}

BlockPushResult_t block_push(Block_t* block, Position_t pos, Vec_t pos_delta, Direction_t direction, World_t* world, bool pushed_by_ice, F32 force, TransferMomentum_t* instant_momentum,
                             PushFromEntangler_t* from_entangler){
     // LOG("block_push() %d -> %s with force %f\n", get_block_index(world, block), direction_to_string(direction), force);
     // if(instant_momentum){
     //     LOG(" instant momentum %d, %f\n", instant_momentum->mass, instant_momentum->vel);
     // }

     BlockPushResult_t result {};
     auto against_result = block_against_other_blocks(pos + pos_delta, block->cut, direction, world->block_qt, world->interactive_qt,
                                                      &world->tilemap);
     // bool both_on_ice = false;
     bool pushed_block_on_ice = block_on_ice(pos, pos_delta, block->cut, &world->tilemap, world->interactive_qt, world->block_qt);
     // bool transfers_force = false;

     F32 block_push_vel = 0;
     F32 save_block_push_vel = 0;

     if(direction_is_horizontal(direction)){
          block_push_vel = block->vel.x;
          save_block_push_vel = block->vel.x;
     }else{
          block_push_vel = block->vel.y;
          save_block_push_vel = block->vel.y;
     }

     bool transfers_force = false;

     {
          // TODO: we have returns in this loop, how do we handle them?
          MoveDirection_t move_direction = move_direction_from_directions(direction, DIRECTION_COUNT);

          for(S16 i = 0; i < against_result.count; i++){
               if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_ice, force,
                                              instant_momentum, from_entangler, against_result.againsts + i, against_result.count,
                                              world, &result, &transfers_force)){
                    return result;
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
                    if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_ice, force, instant_momentum,
                                                   from_entangler, &against, 1, world, &result, &transfers_force)){
                         return result;
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
                    if(!resolve_push_against_block(block, move_direction, pushed_by_ice, pushed_block_on_ice, force, instant_momentum,
                                                   from_entangler, &against, 1, world, &result, &transfers_force)){
                         return result;
                    }
               }
          }
          break;
     }

     if(transfers_force) return result;

     if(!pushed_by_ice){
          auto against_block = rotated_entangled_blocks_against_centroid(block, direction, world->block_qt, &world->blocks,
                                                                         world->interactive_qt, &world->tilemap);
          if(against_block){
               // TODO: compress this logic with the logic in block_pushable()
               // given the current force, and masses, can this push move the entangled block anyways?
               S16 block_mass = block_get_mass(block);
               S16 entangled_block_mass = block_get_mass(against_block);
               F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
               if((mass_ratio * force) >= 1.0f) return result;
          }
     }

     if(block_against_solid_tile(pos, pos_delta, block->cut, direction, &world->tilemap)){
          return result;
     }
     if(block_against_solid_interactive(block, direction, &world->tilemap, world->interactive_qt)){
          return result;
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
                    return result;
               }
          }
          break;
     case DIRECTION_DOWN:
     case DIRECTION_UP:
          if(block->horizontal_move.state != MOVE_STATE_IDLING && block->accel.x != 0.0f){
               Direction_t horizontal_direction = block->accel.x > 0.0f ? DIRECTION_RIGHT : DIRECTION_LEFT;
               if(block_diagonally_against_solid(pos, pos_delta, block->cut, horizontal_direction, direction, &world->tilemap, world->interactive_qt)){
                    return result;
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
               return result;
          }
     }

     // if are sliding on ice and are pushed in the opposite direction then stop
     if(pushed_block_on_ice && !instant_momentum){
          switch(direction){
          default:
               break;
          case DIRECTION_LEFT:
          case DIRECTION_RIGHT:
          {
               Vec_t horizontal_vel = {block->vel.x, 0};
               auto moving_dir = vec_direction(horizontal_vel);

               if(direction_opposite(moving_dir) == direction){
                    S16 block_mass = get_block_stack_mass(world, block);
                    F32 normal_block_velocity = get_block_normal_pushed_velocity(block->cut, block_mass);
                    if(block->vel.x < 0) normal_block_velocity = -normal_block_velocity;
                    F32 final_vel = block->vel.x - normal_block_velocity;

                    if(fabs(final_vel) <= FLT_EPSILON){
                        slow_block_toward_gridlock(world, block, direction_opposite(direction));
                        return result;
                    }else{
                        // TODO: This calculation is probably wrong in terms of forces, but gets us the gameplay impact we want, come back to this
                        block->accel.x = (final_vel - block->vel.x) / BLOCK_ACCEL_TIME;
                        block->target_vel.x = final_vel;
                        block->horizontal_move.time_left = BLOCK_ACCEL_TIME;
                        block->horizontal_move.state = MOVE_STATE_STARTING;
                        block->started_on_pixel_x = 0;
                        block->stop_distance_pixel_x = 0;
                        return result;
                    }
               }
               break;
          }
               break;
          case DIRECTION_UP:
          case DIRECTION_DOWN:
          {
               Vec_t vertical_vel = {0, block->vel.y};
               auto moving_dir = vec_direction(vertical_vel);

               // TODO: handle instant momentum

               if(direction_opposite(moving_dir) == direction){
                    S16 block_mass = get_block_stack_mass(world, block);
                    F32 normal_block_velocity = get_block_normal_pushed_velocity(block->cut, block_mass);
                    if(block->vel.y < 0) normal_block_velocity = -normal_block_velocity;
                    F32 final_vel = block->vel.y - normal_block_velocity;

                    if(fabs(final_vel) <= FLT_EPSILON){
                        slow_block_toward_gridlock(world, block, direction_opposite(direction));
                        return result;
                    }else{
                        block->accel.y = (final_vel - block->vel.y) / BLOCK_ACCEL_TIME;
                        block->target_vel.y = final_vel;
                        block->vertical_move.time_left = BLOCK_ACCEL_TIME;
                        block->vertical_move.state = MOVE_STATE_STARTING;
                        block->started_on_pixel_y = 0;
                        block->stop_distance_pixel_y = 0;
                        return result;
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
          apply_push_horizontal(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler, &result);
          break;
     case DIRECTION_RIGHT:
          apply_push_horizontal(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler, &result);
          break;
     case DIRECTION_DOWN:
          apply_push_vertical(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler, &result);
          break;
     case DIRECTION_UP:
          apply_push_vertical(block, pos, world, direction, instant_momentum, pushed_by_ice, force, from_entangler, &result);
          break;
     }

     result.pushed = true;
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

     if(block->entangle_index >= 0){
          Block_t* entangled_block = rotated_entangled_blocks_against_centroid(block, direction, world->block_qt, &world->blocks, world->interactive_qt, &world->tilemap);
          if(entangled_block){
               S16 block_mass = block_get_mass(block);
               S16 entangled_block_mass = block_get_mass(entangled_block);
               F32 mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);
               if((mass_ratio * force) >= 1.0f) return false;
          }
     }

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
     LOG("    vel  : %.10f, %.10f prev_vel: %f, %f\n", block->vel.x, block->vel.y, block->prev_vel.x, block->prev_vel.y);
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
          LOG("Tile: id: %u, light: %u\n", tile->id, tile->light);
          if(tile->flags){
               LOG(" flags:\n");
               if(tile->flags & TILE_FLAG_ICED) printf("  ICED\n");
               if(tile->flags & TILE_FLAG_CHECKPOINT) printf("  CHECKPOINT\n");
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

#define LOG_MISMATCH(name, fmt_spec, chk, act)\
     {                                                                                                                     \
          char mismatch_fmt_string[128];                                                                                   \
          snprintf(mismatch_fmt_string, 128, "mismatched '%s' value. demo '%s', actual '%s'\n", name, fmt_spec, fmt_spec); \
          LOG(mismatch_fmt_string, chk, act);                                                                              \
          test_passed = false;                                                                                             \
     }

bool test_map_end_state(World_t* world, Demo_t* demo){
     bool test_passed = true;

     TileMap_t check_tilemap = {};
     ObjectArray_t<Block_t> check_block_array = {};
     ObjectArray_t<Interactive_t> check_interactives = {};
     Coord_t check_player_start;

#define NAME_LEN 64
     char name[NAME_LEN];

     if(!load_map_from_file(demo->file, &check_player_start, &check_tilemap, &check_block_array, &check_interactives, demo->filepath)){
          LOG("failed to load map state from end of file\n");
          return false;
     }

     if(check_tilemap.width != world->tilemap.width){
          LOG_MISMATCH("tilemap width", "%d", check_tilemap.width, world->tilemap.width);
     }else if(check_tilemap.height != world->tilemap.height){
          LOG_MISMATCH("tilemap height", "%d", check_tilemap.height, world->tilemap.height);
     }else{
          for(S16 j = 0; j < check_tilemap.height; j++){
               for(S16 i = 0; i < check_tilemap.width; i++){
                    if(check_tilemap.tiles[j][i].flags != world->tilemap.tiles[j][i].flags){
                         snprintf(name, NAME_LEN, "tile %d, %d flags", i, j);
                         LOG_MISMATCH(name, "%d", check_tilemap.tiles[j][i].flags, world->tilemap.tiles[j][i].flags);
                    }
               }
          }
     }

     S16 check_player_pixel_count = 0;
     Pixel_t* check_player_pixels = nullptr;

     switch(demo->version){
     default:
          break;
     case 1:
          check_player_pixel_count = 1;
          check_player_pixels = (Pixel_t*)(malloc(sizeof(*check_player_pixels)));
          break;
     case 2:
          fread(&check_player_pixel_count, sizeof(check_player_pixel_count), 1, demo->file);
          check_player_pixels = (Pixel_t*)(malloc(sizeof(*check_player_pixels) * check_player_pixel_count));
          break;
     }

     for(S16 i = 0; i < check_player_pixel_count; i++){
          fread(check_player_pixels + i, sizeof(check_player_pixels[i]), 1, demo->file);
     }

     for(S16 i = 0; i < world->players.count; i++){
          Player_t* player = world->players.elements + i;

          if(check_player_pixels[i].x != player->pos.pixel.x){
               LOG_MISMATCH("player pixel x", "%d", check_player_pixels[i].x, player->pos.pixel.x);
          }

          if(check_player_pixels[i].y != player->pos.pixel.y){
               LOG_MISMATCH("player pixel y", "%d", check_player_pixels[i].y, player->pos.pixel.y);
          }
     }

     free(check_player_pixels);

     if(check_block_array.count != world->blocks.count){
          LOG_MISMATCH("block count", "%d", check_block_array.count, world->blocks.count);
     }else{
          for(S16 i = 0; i < check_block_array.count; i++){
               // TODO: consider checking other things
               Block_t* check_block = check_block_array.elements + i;
               Block_t* block = world->blocks.elements + i;
               if(check_block->pos.pixel.x != block->pos.pixel.x){
                    snprintf(name, NAME_LEN, "block %d pos x", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.pixel.x, block->pos.pixel.x);
               }

               if(check_block->pos.pixel.y != block->pos.pixel.y){
                    snprintf(name, NAME_LEN, "block %d pos y", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.pixel.y, block->pos.pixel.y);
               }

               if(check_block->pos.z != block->pos.z){
                    snprintf(name, NAME_LEN, "block %d pos z", i);
                    LOG_MISMATCH(name, "%d", check_block->pos.z, block->pos.z);
               }

               if(check_block->element != block->element){
                    snprintf(name, NAME_LEN, "block %d element", i);
                    LOG_MISMATCH(name, "%d", check_block->element, block->element);
               }

               if(check_block->entangle_index != block->entangle_index){
                    snprintf(name, NAME_LEN, "block %d entangle_index", i);
                    LOG_MISMATCH(name, "%d", check_block->entangle_index, block->entangle_index);
               }
          }
     }

     if(check_interactives.count != world->interactives.count){
          LOG_MISMATCH("interactive count", "%d", check_interactives.count, world->interactives.count);
     }else{
          for(S16 i = 0; i < check_interactives.count; i++){
               // TODO: consider checking other things
               Interactive_t* check_interactive = check_interactives.elements + i;
               Interactive_t* interactive = world->interactives.elements + i;

               if(check_interactive->type != interactive->type){
                    LOG_MISMATCH("interactive type", "%d", check_interactive->type, interactive->type);
               }else{
                    switch(check_interactive->type){
                    default:
                         break;
                    case INTERACTIVE_TYPE_PRESSURE_PLATE:
                         if(check_interactive->pressure_plate.down != interactive->pressure_plate.down){
                              snprintf(name, NAME_LEN, "interactive at %d, %d pressure plate down",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->pressure_plate.down,
                                           interactive->pressure_plate.down);
                         }
                         break;
                    case INTERACTIVE_TYPE_ICE_DETECTOR:
                    case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                         if(check_interactive->detector.on != interactive->detector.on){
                              snprintf(name, NAME_LEN, "interactive at %d, %d detector on",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->detector.on,
                                           interactive->detector.on);
                         }
                         break;
                    case INTERACTIVE_TYPE_POPUP:
                         if(check_interactive->popup.iced != interactive->popup.iced){
                              snprintf(name, NAME_LEN, "interactive at %d, %d popup iced",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->popup.iced,
                                           interactive->popup.iced);
                         }
                         if(check_interactive->popup.lift.up != interactive->popup.lift.up){
                              snprintf(name, NAME_LEN, "interactive at %d, %d popup lift up",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->popup.lift.up,
                                           interactive->popup.lift.up);
                         }
                         break;
                    case INTERACTIVE_TYPE_DOOR:
                         if(check_interactive->door.lift.up != interactive->door.lift.up){
                              snprintf(name, NAME_LEN, "interactive at %d, %d door lift up",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->door.lift.up,
                                           interactive->door.lift.up);
                         }
                         break;
                    case INTERACTIVE_TYPE_PORTAL:
                         if(check_interactive->portal.on != interactive->portal.on){
                              snprintf(name, NAME_LEN, "interactive at %d, %d portal on",
                                       interactive->coord.x, interactive->coord.y);
                              LOG_MISMATCH(name, "%d", check_interactive->portal.on,
                                           interactive->portal.on);
                         }
                         break;
                    }
               }
          }
     }

     return test_passed;
}

S16 get_block_index(World_t* world, Block_t* block){
    S16 index = block - world->blocks.elements;
    assert(index >= 0 && index <= world->blocks.count);
    return index;
}

bool setup_default_room(World_t* world){
     init(&world->tilemap, ROOM_TILE_SIZE, ROOM_TILE_SIZE);

     for(S16 i = 0; i < world->tilemap.width; i++){
          world->tilemap.tiles[0][i].id = 33;
          world->tilemap.tiles[1][i].id = 17;
          world->tilemap.tiles[world->tilemap.height - 1][i].id = 16;
          world->tilemap.tiles[world->tilemap.height - 2][i].id = 32;
     }

     for(S16 i = 0; i < world->tilemap.height; i++){
          world->tilemap.tiles[i][0].id = 18;
          world->tilemap.tiles[i][1].id = 19;
          world->tilemap.tiles[i][world->tilemap.width - 2].id = 34;
          world->tilemap.tiles[i][world->tilemap.height - 1].id = 35;
     }

     world->tilemap.tiles[0][0].id = 36;
     world->tilemap.tiles[0][1].id = 37;
     world->tilemap.tiles[1][0].id = 20;
     world->tilemap.tiles[1][1].id = 21;

     world->tilemap.tiles[16][0].id = 22;
     world->tilemap.tiles[16][1].id = 23;
     world->tilemap.tiles[15][0].id = 38;
     world->tilemap.tiles[15][1].id = 39;

     world->tilemap.tiles[15][15].id = 40;
     world->tilemap.tiles[15][16].id = 41;
     world->tilemap.tiles[16][15].id = 24;
     world->tilemap.tiles[16][16].id = 25;

     world->tilemap.tiles[0][15].id = 42;
     world->tilemap.tiles[0][16].id = 43;
     world->tilemap.tiles[1][15].id = 26;
     world->tilemap.tiles[1][16].id = 27;

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

TransferMomentum_t get_block_momentum(World_t* world, Block_t* block, Direction_t direction){
     TransferMomentum_t block_momentum;

     block_momentum.vel = 0;
     block_momentum.mass = get_block_stack_mass(world, block);

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
     case DIRECTION_RIGHT:
          block_momentum.vel = block->vel.x;
          break;
     case DIRECTION_UP:
     case DIRECTION_DOWN:
          block_momentum.vel = block->vel.y;
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

ElasticCollisionResult_t elastic_transfer_momentum_to_block(TransferMomentum_t* first_transfer_momentum, World_t* world, Block_t* block, Direction_t direction){
     auto second_block_momentum = get_block_momentum(world, block, direction);
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
          result_direction = direction_rotate_clockwise(result_direction, result.againsts[i].rotations_through_portal);
          auto result_block = result.againsts[i].block;

          if((require_on_ice && block_on_ice(result_block->pos, result_block->pos_delta, result_block->cut,
                                             &world->tilemap, world->interactive_qt, world->block_qt)) ||
              !require_on_ice){
               get_block_stack(world, result_block, block_list, result.againsts[i].rotations_through_portal);
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
