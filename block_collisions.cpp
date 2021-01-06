#include "block_collisions.h"
#include "conversion.h"
#include "defines.h"
#include "utils.h"
#include "tags.h"

#include <stdlib.h>
#include <math.h>
#include <float.h>

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

static F32 get_collision_dt(CheckBlockCollisionResult_t* collision){
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

static int sort_collision_by_time_comparer(const void* a, const void* b){
     CheckBlockCollisionResult_t* collision_a = (CheckBlockCollisionResult_t*)a;
     CheckBlockCollisionResult_t* collision_b = (CheckBlockCollisionResult_t*)b;

     return get_collision_dt(collision_a) < get_collision_dt(collision_b);
}

bool CheckBlockCollisions_t::init(S32 block_count){
    collisions = (CheckBlockCollisionResult_t*)malloc(block_count * block_count * sizeof(*collisions));
    if(collisions == NULL) return false;
    allocated = block_count;
    return true;
}

bool CheckBlockCollisions_t::add_collision(CheckBlockCollisionResult_t* collision){
    if(count >= allocated) return false;
    collisions[count] = *collision;
    count++;
    return true;
}

void CheckBlockCollisions_t::reset(){
    count = 0;
}

void CheckBlockCollisions_t::clear(){
    free(collisions);
    collisions = NULL;
    count = 0;
    allocated = 0;
}

void CheckBlockCollisions_t::sort_by_time(){
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
                                   {(S8)(DIRECTION_MASK_RIGHT | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
                                   {(S8)(DIRECTION_MASK_RIGHT | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {(S8)(DIRECTION_MASK_LEFT  | DIRECTION_MASK_UP), DIRECTION_LEFT, DIRECTION_DOWN, DIRECTION_UP, DIRECTION_RIGHT},
                                   {(S8)(DIRECTION_MASK_LEFT  | DIRECTION_MASK_DOWN), DIRECTION_LEFT, DIRECTION_UP, DIRECTION_DOWN, DIRECTION_RIGHT},
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

                                        stop_block_colliding_in_dir(block, move_dir_to_stop);
                                        stop_block_colliding_in_dir(entangled_block, stop_entangled_dir);

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
