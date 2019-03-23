#include "world.h"
#include "defines.h"
#include "conversion.h"
#include "map_format.h"
#include "portal_exit.h"
#include "utils.h"
#include "collision.h"
#include "block_utils.h"

// linux
#include <dirent.h>
#include <string.h>
#include <math.h>

bool load_map_number(S32 map_number, Coord_t* player_start, World_t* world){
     // search through directory to find file starting with 3 digit map number
     DIR* d = opendir("content");
     if(!d) return false;
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

     if(!filepath[0]) return false;

     LOG("load map %s\n", filepath);
     return load_map(filepath, player_start, &world->tilemap, &world->blocks, &world->interactives);
}

void reset_map(Coord_t player_start, World_t* world, Undo_t* undo){
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
               if(from_wire) interactive->portal.on = !interactive->portal.on;
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

void slow_block_toward_gridlock(World_t* world, Block_t* block, bool horizontal, bool negative){
     if(!block_on_ice(block->pos, block->pos_delta, &world->tilemap, world->interactive_qt)) return;

     Move_t* move = horizontal ? &block->horizontal_move : &block->vertical_move;

     if(move->state == MOVE_STATE_STOPPING) return;

     move->state = MOVE_STATE_STOPPING;
     move->distance = 0;

     auto stop_coord = pos_to_coord(block->pos);
     if(negative){
          if(horizontal){
               stop_coord.x++;
          }else{
               stop_coord.y++;
          }
     }

     auto stop_position = coord_to_pos(stop_coord);
     Vec_t distance_to_stop = pos_to_vec(stop_position - block->pos);

     if(horizontal){
          // if we are too close to the tile boundary, have the block move to the next grid square, this is a design choice
          if(fabs(distance_to_stop.x) <= PLAYER_RADIUS){
               if(negative){
                    stop_coord.x++;
               }else{
                    stop_coord.x--;
               }
               stop_position = coord_to_pos(stop_coord);
               distance_to_stop = pos_to_vec(stop_position - block->pos);
          }
     }else{
          if(fabs(distance_to_stop.y) <= PLAYER_RADIUS){
               if(negative){
                    stop_coord.y++;
               }else{
                    stop_coord.y--;
               }
               stop_position = coord_to_pos(stop_coord);
               distance_to_stop = pos_to_vec(stop_position - block->pos);
          }
     }

     // xf = x0 + vt + 1/2at^2
     // d = xf - x0
     // d = vt + 1/2at^2
     // (d - vt) = 1/2at^2
     // (d - vt) / 1/2t^2 = a

     if(horizontal){
          F32 time = distance_to_stop.x / (0.5f * block->vel.x);
          block->stopped_by_player_horizontal = true;
          block->accel_magnitudes.x = fabs(block->vel.x / (time - 0.01666667)); // adjust by one tick since we missed this update
     }else{
          F32 time = distance_to_stop.y / (0.5f * block->vel.y);
          block->stopped_by_player_vertical = true;
          block->accel_magnitudes.y = fabs(block->vel.y / (time - 0.01666667)); // adjust by one tick since we missed this update
     }
}

Block_t* player_against_block(Player_t* player, Direction_t direction, QuadTreeNode_t<Block_t>* block_qt){
     auto check_rect = rect_surrounding_adjacent_coords(pos_to_coord(player->pos));

     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(block_qt, check_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

     for(S16 i = 0; i < block_count; i++){
          Block_t* block = blocks[i];
          Rect_t block_rect = block_get_rect(block);

          if(pixel_in_rect(pos_a.pixel, block_rect) || pixel_in_rect(pos_b.pixel, block_rect)){
               return block;
          }
     }

     return nullptr;
}

bool player_against_solid_tile(Player_t* player, Direction_t direction, TileMap_t* tilemap){
     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     if(tilemap_is_solid(tilemap, pixel_to_coord(pos_a.pixel))) return true;
     if(tilemap_is_solid(tilemap, pixel_to_coord(pos_b.pixel))) return true;

     return false;
}

bool player_against_solid_interactive(Player_t* player, Direction_t direction, QuadTreeNode_t<Interactive_t>* interactive_qt){
     Position_t pos_a;
     Position_t pos_b;

     get_player_adjacent_positions(player, direction, &pos_a, &pos_b);

     Coord_t coord = pixel_to_coord(pos_a.pixel);
     Interactive_t* interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive && interactive_is_solid(interactive)) return true;

     coord = pixel_to_coord(pos_b.pixel);
     interactive = quad_tree_find_at(interactive_qt, coord.x, coord.y);
     if(interactive && interactive_is_solid(interactive)) return true;

     return false;
}

MovePlayerThroughWorldResult_t move_player_through_world(Position_t player_pos, Vec_t player_vel, Vec_t player_pos_delta,
                                                         Direction_t player_face, S8 player_clone_instance, S16 player_index,
                                                         S16 player_pushing_block, Direction_t player_pushing_block_dir,
                                                         S8 player_pushing_block_rotation,
                                                         Coord_t* skip_coord, World_t* world){
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
     // S8 player_top = player.pos.z + 2 * HEIGHT_INTERVAL;
     min = coord_clamp_zero_to_dim(min, world->tilemap.width - (S16)(1), world->tilemap.height - (S16)(1));
     max = coord_clamp_zero_to_dim(max, world->tilemap.width - (S16)(1), world->tilemap.height - (S16)(1));

     // TODO: convert to use quad tree
     Vec_t collided_block_delta {};

     // where the player has collided with blocks
     DirectionMask_t collided_blocks_mask_dir = DIRECTION_MASK_NONE;
     Block_t* collided_blocks[DIRECTION_COUNT] = {};

     for(S16 i = 0; i < world->blocks.count; i++){
          Vec_t result_pos_delta = result.pos_delta;
          Vec_t pos_delta_save = result.pos_delta;

          bool collided_with_block = false;
          Block_t* block = world->blocks.elements + i;

          // check if the block is in our player's height range
          if(block->pos.z <= player_pos.z - HEIGHT_INTERVAL ||
             block->pos.z >= player_pos.z + (HEIGHT_INTERVAL * 2)) continue;

          Position_t block_pos = block->pos + block->pos_delta;
          position_collide_with_rect(player_pos, block_pos, TILE_SIZE, &result_pos_delta, &collided_with_block);
          if(collided_with_block) result.collided = true;

          U8 collision_portal_rotations = 0;
          Coord_t block_coord = block_get_coord(world->blocks.elements + i);

          if(!collided_with_block){
               // check if the block is in a portal and try to collide with it
               Vec_t coord_offset = pos_to_vec(world->blocks.elements[i].pos + world->blocks.elements[i].pos_delta +
                                               pixel_to_pos(HALF_TILE_SIZE_PIXEL) -
                                               coord_to_pos_at_tile_center(block_coord));
               for(S8 r = 0; r < DIRECTION_COUNT && !collided_with_block; r++){
                    Coord_t check_coord = block_coord + (Direction_t)(r);
                    Interactive_t* interactive = quad_tree_interactive_find_at(world->interactive_qt, check_coord);

                    if(is_active_portal(interactive)){
                         PortalExit_t portal_exits = find_portal_exits(check_coord, &world->tilemap, world->interactive_qt);

                         for(S8 d = 0; d < DIRECTION_COUNT && !collided_with_block; d++){
                              Vec_t final_coord_offset = rotate_vec_between_dirs_clockwise(interactive->portal.face, (Direction_t)(d), coord_offset);

                              for(S8 p = 0; p < portal_exits.directions[d].count; p++){
                                   if(portal_exits.directions[d].coords[p] == check_coord) continue;

                                   Position_t portal_pos = coord_to_pos_at_tile_center(portal_exits.directions[d].coords[p]) + final_coord_offset -
                                                           pixel_to_pos(HALF_TILE_SIZE_PIXEL);
                                   position_collide_with_rect(player_pos, portal_pos, TILE_SIZE, &result_pos_delta, &collided_with_block);

                                   if(collided_with_block){
                                        result.collided = true;
                                        block_pos = portal_pos;
                                        collision_portal_rotations = portal_rotations_between(interactive->portal.face, (Direction_t)(d));
                                        break;
                                   }
                              }
                         }
                    }
               }
          }

          if(collided_with_block){
               Vec_t pos_delta_diff = result_pos_delta - pos_delta_save;
               collided_block_delta = vec_rotate_quadrants_clockwise(pos_delta_diff, (U8)(4) - collision_portal_rotations);
               auto collided_block_dir = relative_quadrant(player_pos.pixel, block_pos.pixel + HALF_TILE_SIZE_PIXEL);
               auto collided_block = world->blocks.elements + i;

               {
                    // this stops the block when it moves into the player
                    Vec_t rotated_vel = vec_rotate_quadrants_clockwise(collided_block->vel, collision_portal_rotations);
                    bool even_rotations = (collision_portal_rotations % 2) == 0;

                    auto* player = world->players.elements + player_index;

                    // TODO: definitely heavily condense this

                    bool entangled_rotations_even = true;
                    Block_t* collided_entangled_block = nullptr;
                    if(collided_block->entangle_index >= 0){
                         collided_entangled_block = world->blocks.elements + collided_block->entangle_index;
                         entangled_rotations_even = ((collided_entangled_block->rotation + collided_block->rotation) % 2) == 0;
                    }

                    switch(collided_block_dir){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         if(rotated_vel.x > 0.0f){
                              result.pos_delta.x = 0;

                              bool would_squish = false;
                              Block_t* squished_block = player_against_block(player, direction_opposite(collided_block_dir), world->block_qt);
                              would_squish = squished_block && squished_block->vel.x < collided_block->vel.x;

                              if(!would_squish){
                                   would_squish = player_against_solid_tile(player, direction_opposite(collided_block_dir), &world->tilemap);
                              }
                              if(!would_squish){
                                   would_squish = player_against_solid_interactive(player, direction_opposite(collided_block_dir), world->interactive_qt);
                              }

                              // only squish if the block we would be squished against is moving slower, do we stop the block we collided with

                              // TODO: handle rotations
                              if(would_squish){
                                   reset_move(&collided_block->horizontal_move);

                                   // TODO: enforce the same behavior on entangled blocks
                                   collided_block->pos_delta.x = 0;
                                   collided_block->vel.x = 0;
                                   collided_block->accel.x = 0;
                                   collided_block->pos.pixel.x = (player->pos.pixel.x - 5) - BLOCK_SOLID_SIZE_IN_PIXELS;
                                   collided_block->pos.decimal.x = 0;
                              }else{
                                   auto new_pos = collided_block->pos + Vec_t{TILE_SIZE + PLAYER_RADIUS, 0};
                                   player->pos.pixel.x = new_pos.pixel.x;
                                   player->pos.decimal.x = new_pos.decimal.x;

                                   player->stopping_block_from = collided_block_dir;

                                   if(even_rotations){
                                        slow_block_toward_gridlock(world, collided_block, true, true);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, true);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, true);
                                             }
                                        }
                                   }else{
                                        slow_block_toward_gridlock(world, collided_block, false, true);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, true);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, true);
                                             }
                                        }
                                   }
                              }
                         }else{
                              result.pos_delta = result_pos_delta;
                         }
                         break;
                    case DIRECTION_RIGHT:
                         if(rotated_vel.x < 0.0f){
                              result.pos_delta.x = 0;

                              bool would_squish = false;
                              Block_t* squished_block = player_against_block(player, direction_opposite(collided_block_dir), world->block_qt);
                              would_squish = squished_block && squished_block->vel.x > collided_block->vel.x;

                              if(!would_squish){
                                   would_squish = player_against_solid_tile(player, direction_opposite(collided_block_dir), &world->tilemap);
                              }
                              if(!would_squish){
                                   would_squish = player_against_solid_interactive(player, direction_opposite(collided_block_dir), world->interactive_qt);
                              }

                              if(would_squish){
                                   reset_move(&collided_block->horizontal_move);
                                   collided_block->pos_delta.x = 0;
                                   collided_block->vel.x = 0;
                                   collided_block->accel.x = 0;
                                   collided_block->pos.decimal.x = 0;
                                   collided_block->pos.pixel.x = (player->pos.pixel.x + 5);
                              }else{
                                   auto new_pos = collided_block->pos - Vec_t{PLAYER_RADIUS, 0};
                                   player->pos.pixel.x = new_pos.pixel.x;
                                   player->pos.decimal.x = new_pos.decimal.x;

                                   player->stopping_block_from = collided_block_dir;

                                   if(even_rotations){
                                        slow_block_toward_gridlock(world, collided_block, true, false);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, false);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, false);
                                             }
                                        }
                                   }else{
                                        slow_block_toward_gridlock(world, collided_block, false, false);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, false);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, false);
                                             }
                                        }
                                   }
                              }
                         }else{
                              result.pos_delta = result_pos_delta;
                         }
                         break;
                    case DIRECTION_UP:
                         if(rotated_vel.y < 0.0f){
                              result.pos_delta.y = 0;

                              bool would_squish = false;
                              Block_t* squished_block = player_against_block(player, direction_opposite(collided_block_dir), world->block_qt);
                              would_squish = squished_block && squished_block->vel.y > collided_block->vel.y;
                              if(!would_squish){
                                   would_squish = player_against_solid_tile(player, direction_opposite(collided_block_dir), &world->tilemap);
                              }
                              if(!would_squish){
                                   would_squish = player_against_solid_interactive(player, direction_opposite(collided_block_dir), world->interactive_qt);
                              }

                              if(would_squish){
                                   reset_move(&collided_block->vertical_move);
                                   collided_block->pos_delta.y = 0;
                                   collided_block->vel.y = 0;
                                   collided_block->accel.y = 0;
                                   collided_block->pos.pixel.y = player->pos.pixel.y + 5;
                                   collided_block->pos.decimal.y = 0;
                              }else{
                                   auto new_pos = collided_block->pos - Vec_t{0, PLAYER_RADIUS};
                                   player->pos.pixel.y = new_pos.pixel.y;
                                   player->pos.decimal.y = new_pos.decimal.y;

                                   player->stopping_block_from = collided_block_dir;

                                   if(even_rotations){
                                        slow_block_toward_gridlock(world, collided_block, false, false);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, false);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, false);
                                             }
                                        }
                                   }else{
                                        slow_block_toward_gridlock(world, collided_block, true, false);
                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, false);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, false);
                                             }
                                        }
                                   }
                              }
                         }else{
                              result.pos_delta = result_pos_delta;
                         }
                         break;
                    case DIRECTION_DOWN:
                         if(rotated_vel.y > 0.0f){
                              result.pos_delta.y = 0;

                              bool would_squish = false;
                              Block_t* squished_block = player_against_block(player, direction_opposite(collided_block_dir), world->block_qt);
                              would_squish = squished_block && squished_block->vel.y < collided_block->vel.y;
                              if(!would_squish){
                                   would_squish = player_against_solid_tile(player, direction_opposite(collided_block_dir), &world->tilemap);
                              }
                              if(!would_squish){
                                   would_squish = player_against_solid_interactive(player, direction_opposite(collided_block_dir), world->interactive_qt);
                              }

                              if(would_squish){
                                   reset_move(&collided_block->vertical_move);
                                   collided_block->pos_delta.y = 0;
                                   collided_block->vel.y = 0;
                                   collided_block->accel.y = 0;
                                   collided_block->pos.decimal.y = 0;
                                   collided_block->pos.pixel.y = (player->pos.pixel.y - 5) - BLOCK_SOLID_SIZE_IN_PIXELS;
                              }else{
                                   auto new_pos = collided_block->pos + Vec_t{0, TILE_SIZE + PLAYER_RADIUS};
                                   player->pos.pixel.y = new_pos.pixel.y;
                                   player->pos.decimal.y = new_pos.decimal.y;

                                   player->stopping_block_from = collided_block_dir;

                                   if(even_rotations){
                                        slow_block_toward_gridlock(world, collided_block, false, true);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, true);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, true);
                                             }
                                        }
                                   }else{
                                        collided_block->stopped_by_player_horizontal = true;
                                        slow_block_toward_gridlock(world, collided_block, true, true);

                                        if(collided_entangled_block){
                                             if(entangled_rotations_even){
                                                  slow_block_toward_gridlock(world, collided_entangled_block, true, true);
                                             }else{
                                                  slow_block_toward_gridlock(world, collided_entangled_block, false, true);
                                             }
                                        }
                                   }
                              }
                         }else{
                              result.pos_delta = result_pos_delta;
                         }
                         break;
                    }
               }

               auto rotated_player_face = direction_rotate_counter_clockwise(player_face, collision_portal_rotations);

               if(collided_block_dir == player_face && (player_vel.x != 0.0f || player_vel.y != 0.0f)){
                    if(result.pushing_block < 0){ // also check that the player is actually pushing against the block
                         result.pushing_block = i;
                         result.pushing_block_dir = rotated_player_face;
                         result.pushing_block_rotation = collision_portal_rotations;
                    }else{
                         // stop the player from pushing 2 blocks at once
                         result.pushing_block = -1;
                         result.pushing_block_dir = DIRECTION_COUNT;
                         result.pushing_block_rotation = 0;
                    }
               }

               collided_blocks_mask_dir = direction_mask_add(collided_blocks_mask_dir, collided_block_dir);
               collided_blocks[collided_block_dir] = collided_block;

          }
     }

     Direction_t collided_tile_dir = DIRECTION_COUNT;
     for(S16 y = min.y; y <= max.y; y++){
          for(S16 x = min.x; x <= max.x; x++){
               if(world->tilemap.tiles[y][x].id){
                    Coord_t coord {x, y};
                    bool skip = false;
                    for(S16 d = 0; d < DIRECTION_COUNT; d++){
                         if(skip_coord[d] == coord){
                              skip = true;
                              break;
                         }
                    }
                    if(skip) continue;
                    bool collide_with_tile = false;
                    position_slide_against_rect(player_pos, coord, PLAYER_RADIUS, &result.pos_delta, &collide_with_tile);
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

               Interactive_t* interactive = quad_tree_interactive_solid_at(world->interactive_qt, &world->tilemap, coord);
               if(interactive){
                    // skip popups at the same level or lower than us
                    if(interactive->type == INTERACTIVE_TYPE_POPUP &&
                       interactive->popup.lift.ticks - 1 <= player_pos.z) continue;

                    bool collided = false;
                    position_slide_against_rect(player_pos, coord, PLAYER_RADIUS, &result.pos_delta, &collided);
                    if(collided && !result.collided){
                         result.collided = true;
                         collided_interactive_dir = direction_between(player_coord, coord);
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
          position_collide_with_rect(player_pos, other_player_bottom_left, 2.0f * PLAYER_RADIUS, &result.pos_delta, &collided);
     }

     return result;
}

TeleportPositionResult_t teleport_position_across_portal(Position_t position, Vec_t pos_delta, World_t* world, Coord_t premove_coord,
                                                         Coord_t postmove_coord){
     TeleportPositionResult_t result {};

     if(postmove_coord == premove_coord) return result;
     auto* interactive = quad_tree_interactive_find_at(world->interactive_qt, postmove_coord);
     if(!is_active_portal(interactive)) return result;
     if(interactive->portal.face != direction_opposite(direction_between(postmove_coord, premove_coord))) return result;

     Position_t offset_from_center = position - coord_to_pos_at_tile_center(postmove_coord);
     PortalExit_t portal_exit = find_portal_exits(postmove_coord, &world->tilemap, world->interactive_qt);

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
          Interactive_t* interactive = quad_tree_interactive_find_at(world->interactive_qt, coords[i]);
          if(interactive && interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks >= (POPUP_MAX_LIFT_TICKS / 2)){
               break;
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

static void impact_ice(Coord_t center, S16 radius, World_t* world, bool teleported, bool spread_the_ice){
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

                    Block_t* block = nullptr;
                    for(S16 i = 0; i < block_count; i++){
                         if(block_get_coord(blocks[i]) == coord && blocks[i]->pos.z == 0){
                              block = blocks[i];
                              break;
                         }
                    }

                    Interactive_t* interactive = quad_tree_find_at(world->interactive_qt, coord.x, coord.y);

                    if(block){
                         if(spread_the_ice){
                              if(block->element == ELEMENT_NONE) block->element = ELEMENT_ONLY_ICED;
                         }else{
                              if(block->element == ELEMENT_ONLY_ICED) block->element = ELEMENT_NONE;
                         }
                    }else{
                         if(interactive){
                              switch(interactive->type){
                              case INTERACTIVE_TYPE_POPUP:
                                   if(interactive->popup.lift.ticks == 1){
                                        if(spread_the_ice){
                                             interactive->popup.iced = false;
                                             tile->flags |= TILE_FLAG_ICED;
                                        }else{
                                             tile->flags &= ~TILE_FLAG_ICED;
                                        }
                                   }else{
                                        interactive->popup.iced = spread_the_ice;
                                   }
                                   break;
                              case INTERACTIVE_TYPE_PRESSURE_PLATE:
                              case INTERACTIVE_TYPE_ICE_DETECTOR:
                              case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                                   if(spread_the_ice){
                                        tile->flags |= TILE_FLAG_ICED;
                                   }else{
                                        tile->flags &= ~TILE_FLAG_ICED;
                                        if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                                             interactive->pressure_plate.iced_under = false;
                                        }
                                   }
                                   break;
                              default:
                                   break;
                              }
                         }else{
                              if(spread_the_ice){
                                   tile->flags |= TILE_FLAG_ICED;
                              }else{
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

                                   impact_ice(portal_exits.directions[d].coords[p] + opposite, radius - distance_from_center,
                                              world, true, spread_the_ice);
                              }
                         }
                    }
               }
          }
     }
}

void spread_ice(Coord_t center, S16 radius, World_t* world, bool teleported){
     impact_ice(center, radius, world, teleported, true);
}

void melt_ice(Coord_t center, S16 radius, World_t* world, bool teleported){
     impact_ice(center, radius, world, teleported, false);
}

bool block_push(Block_t* block, MoveDirection_t move_direction, World_t* world, bool pushed_by_ice, F32 instant_vel){
     Direction_t first;
     Direction_t second;
     move_direction_to_directions(move_direction, &first, &second);

     bool a = block_push(block, first, world, pushed_by_ice, instant_vel);
     bool b = block_push(block, second, world, pushed_by_ice, instant_vel);

     return a || b;
}

bool block_push(Block_t* block, Direction_t direction, World_t* world, bool pushed_by_ice, F32 instant_vel){
     Direction_t collided_block_push_dir = DIRECTION_COUNT;
     Block_t* collided_block = block_against_another_block(block->pos, direction, world->block_qt, world->interactive_qt,
                                                           &world->tilemap, &collided_block_push_dir);
     if(collided_block){
          if(collided_block == block){
               // pass, this happens in a corner portal!
          }else if(pushed_by_ice && block_on_ice(collided_block->pos, collided_block->pos_delta, &world->tilemap, world->interactive_qt)){
               if(block_push(collided_block, collided_block_push_dir, world, pushed_by_ice, instant_vel)){
                    push_entangled_block(collided_block, world, collided_block_push_dir, pushed_by_ice, instant_vel);
               }
               return false;
          }else if(block->entangle_index == (collided_block - world->blocks.elements)){
               // if block is entangled with the block it collides with, check if the entangled block can move, this is kind of duplicate work
               Block_t* entangled_collided_block = block_against_another_block(collided_block->pos, direction, world->block_qt,
                                                                               world->interactive_qt, &world->tilemap,
                                                                               &collided_block_push_dir);
               if(entangled_collided_block) return false;
               if(collided_block->rotation != block->rotation) return false; // only when the rotation is equal can we move with the block
               if(block_against_solid_tile(collided_block, direction, &world->tilemap, world->interactive_qt)) return false;
               if(block_against_solid_interactive(collided_block, direction, &world->tilemap, world->interactive_qt)) return false;
          }else{
               return false;
          }
     }

     if(block_against_solid_tile(block, direction, &world->tilemap, world->interactive_qt)) return false;
     if(block_against_solid_interactive(block, direction, &world->tilemap, world->interactive_qt)) return false;
     auto* player = block_against_player(block, direction, &world->players);
     if(player){
          player->stopping_block_from = direction_opposite(direction);
          player->stopping_block_from_time = PLAYER_STOP_IDLE_BLOCK_TIMER;
          return false;
     }

     // if are sliding on ice and are pushed in the opposite direction then stop
     if(block_on_ice(block->pos, block->pos_delta, &world->tilemap, world->interactive_qt)){
          switch(direction){
          default:
               break;
          case DIRECTION_LEFT:
          case DIRECTION_RIGHT:
          {
               Vec_t horizontal_vel = {block->vel.x, 0};
               auto moving_dir = vec_direction(horizontal_vel);

               if(direction_opposite(moving_dir) == direction){
                    block_stop_horizontally(block);
                    return true;
               }
               break;
          }
               break;
          case DIRECTION_UP:
          case DIRECTION_DOWN:
          {
               Vec_t vertical_vel = {0, block->vel.y};
               auto moving_dir = vec_direction(vertical_vel);

               if(direction_opposite(moving_dir) == direction){
                    block_stop_vertically(block);
                    return true;
               }
               break;
          }
          }
     }

     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          if(block->horizontal_move.state == MOVE_STATE_IDLING){
               block->horizontal_move.sign = MOVE_SIGN_NEGATIVE;
               if(pushed_by_ice){
                    block->horizontal_move.state = MOVE_STATE_COASTING;
                    // the velocity may be going the wrong way, but we can fix it here
                    if(instant_vel > 0) instant_vel = -instant_vel;
                    block->vel.x = instant_vel;
               }else{
                    block->horizontal_move.state = MOVE_STATE_STARTING;
                    block->accel_magnitudes.x = BLOCK_ACCEL;
               }
               block->horizontal_move.distance = 0;
               block->started_on_pixel_x = block->pos.pixel.x;
          }
          break;
     case DIRECTION_RIGHT:
          if(block->horizontal_move.state == MOVE_STATE_IDLING){
               block->horizontal_move.sign = MOVE_SIGN_POSITIVE;
               if(pushed_by_ice){
                    block->horizontal_move.state = MOVE_STATE_COASTING;
                    if(instant_vel < 0) instant_vel = -instant_vel;
                    block->vel.x = instant_vel;
               }else{
                    block->horizontal_move.state = MOVE_STATE_STARTING;
                    block->accel_magnitudes.x = BLOCK_ACCEL;
               }
               block->horizontal_move.distance = 0;
               block->started_on_pixel_x = block->pos.pixel.x;
          }
          break;
     case DIRECTION_DOWN:
          if(block->vertical_move.state == MOVE_STATE_IDLING){
               block->vertical_move.sign = MOVE_SIGN_NEGATIVE;
               if(pushed_by_ice){
                    block->vertical_move.state = MOVE_STATE_COASTING;
                    if(instant_vel > 0) instant_vel = -instant_vel;
                    block->vel.y = instant_vel;
               }else{
                    block->vertical_move.state = MOVE_STATE_STARTING;
                    block->accel_magnitudes.y = BLOCK_ACCEL;
               }
               block->vertical_move.distance = 0;
               block->started_on_pixel_y = block->pos.pixel.y;
          }
          break;
     case DIRECTION_UP:
          if(block->vertical_move.state == MOVE_STATE_IDLING){
               block->vertical_move.sign = MOVE_SIGN_POSITIVE;
               if(pushed_by_ice){
                    block->vertical_move.state = MOVE_STATE_COASTING;
                    if(instant_vel < 0) instant_vel = -instant_vel;
                    block->vel.y = instant_vel;
               }else{
                    block->vertical_move.state = MOVE_STATE_STARTING;
                    block->accel_magnitudes.y = BLOCK_ACCEL;
               }
               block->vertical_move.distance = 0;
               block->started_on_pixel_y = block->pos.pixel.y;
          }
          break;
     }

     return true;
}

bool reset_players(ObjectArray_t<Player_t>* players){
     destroy(players);
     bool success = init(players, 1);
     if(success) *players->elements = Player_t{};
     return success;
}

void describe_block(World_t* world, Block_t* block){
     LOG("block %ld: pixel %d, %d, decimal: %f, %f, rot: %d, element: %s, entangle: %d, clone id: %d\n",
         block - world->blocks.elements, block->pos.pixel.x, block->pos.pixel.y,
         block->pos.decimal.x, block->pos.decimal.y,
         block->rotation, element_to_string(block->element),
         block->entangle_index, block->clone_id);
     LOG(" hmove: %s %s %f\n", move_state_to_string(block->horizontal_move.state), move_sign_to_string(block->horizontal_move.sign), block->horizontal_move.distance);
     LOG(" vmove: %s %s %f\n", move_state_to_string(block->vertical_move.state), move_sign_to_string(block->vertical_move.sign), block->vertical_move.distance);
     LOG("\n");
}

void describe_coord(Coord_t coord, World_t* world){
     LOG("\ndescribe_coord(%d, %d)\n", coord.x, coord.y);
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
          const int info_string_len = 32;
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
               snprintf(info_string, info_string_len, "lift: ticks: %u, up: %d, iced: %d", interactive->popup.lift.ticks, interactive->popup.lift.up, interactive->popup.iced);
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
               snprintf(info_string, info_string_len, "face: %s, on: %d", direction_to_string(interactive->portal.face),
                        interactive->portal.on);
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
          }

          LOG("type: %s %s\n", type_string, info_string);
     }

     Rect_t coord_rect;

     coord_rect.left = coord.x * TILE_SIZE_IN_PIXELS;
     coord_rect.bottom = coord.y * TILE_SIZE_IN_PIXELS;
     coord_rect.right = coord_rect.left + TILE_SIZE_IN_PIXELS;
     coord_rect.top = coord_rect.bottom + TILE_SIZE_IN_PIXELS;

     S16 block_count = 0;
     Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
     quad_tree_find_in(world->block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
     for(S16 i = 0; i < block_count; i++){
          auto* block = blocks[i];
          LOG("block %ld: pixel %d, %d, decimal: %f, %f, rot: %d, element: %s, entangle: %d, clone id: %d\n",
              block - world->blocks.elements, block->pos.pixel.x, block->pos.pixel.y,
              block->pos.decimal.x, block->pos.decimal.y,
              block->rotation, element_to_string(block->element),
              block->entangle_index, block->clone_id);
          LOG(" hmove: %s %s %f\n", move_state_to_string(block->horizontal_move.state), move_sign_to_string(block->horizontal_move.sign), block->horizontal_move.distance);
          LOG(" vmove: %s %s %f\n", move_state_to_string(block->vertical_move.state), move_sign_to_string(block->vertical_move.sign), block->vertical_move.distance);
          LOG("\n");
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
