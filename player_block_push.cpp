#include "player_block_push.h"
#include "block_utils.h"

bool player_block_push_add_ordered_physical_reqs(PlayerBlockPush_t* player_block_push, ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                                 World_t* world, ObjectArray_t<PlayerBlockPush_t*>* ordered_player_block_pushes,
                                                 ObjectArray_t<RestoreBlock_t>* restore_blocks){
     auto* block = world->blocks.elements + player_block_push->block_index;
     auto block_pos = block_get_position(block);
     auto block_pos_delta = block_get_pos_delta(block);
     auto block_cut = block_get_cut(block);
     Direction_t push_direction = DIRECTION_COUNT;

     auto* against_block = block_against_another_block(block_pos + block_pos_delta, block_cut, player_block_push->direction, world->block_qt,
                                                 world->interactive_qt, &world->tilemap, &push_direction);
     if(against_block){
          U32 against_block_index = against_block - world->blocks.elements;
          for(S16 i = 0; i < player_block_pushes->count; i++){
               auto* itr = player_block_pushes->elements + i;
               if(itr->block_index == against_block_index &&
                  itr->direction == player_block_push->direction){
                    if(!player_block_push_add_ordered_physical_reqs(itr, player_block_pushes, world, ordered_player_block_pushes, restore_blocks)){
                         return false;
                    }
               }
          }
     }

     RestoreBlock_t restore_block {};
     restore_block.block = *block;
     restore_block.index = player_block_push->block_index;

     bool would_push = false;
     if(player_block_push->is_entangled()){
          would_push = block_would_push(block, block_pos, block_pos_delta, player_block_push->direction, world, false,
                                        player_block_push->allowed_to_push.mass_ratio, nullptr,
                                        &player_block_push->push_from_entangler, 1, false);
          if(would_push){
               BlockPushResult_t result {};
               block_do_push(block, block_pos, block_pos_delta, player_block_push->direction, world, false,
                             &result, player_block_push->allowed_to_push.mass_ratio, nullptr,
                             &player_block_push->push_from_entangler);
          }
     }else{
          would_push = block_would_push(block, block_pos, block_pos_delta, player_block_push->direction, world, false,
                                        player_block_push->allowed_to_push.mass_ratio, nullptr, nullptr, 1, false);
          if(would_push){
               BlockPushResult_t result {};
               block_do_push(block, block_pos, block_pos_delta, player_block_push->direction, world, false,
                             &result, player_block_push->allowed_to_push.mass_ratio, nullptr, nullptr);
          }
     }

     if(would_push || against_block == nullptr){
          if(resize(ordered_player_block_pushes, ordered_player_block_pushes->count + 1)){
               auto* ordered_player_block_push = ordered_player_block_pushes->elements + (ordered_player_block_pushes->count - 1);
               *ordered_player_block_push = player_block_push;
          }else{
               LOG("ran out of memory trying to allocate %d ordered block pushes\n", ordered_player_block_pushes->count + 1);
          }

          if(resize(restore_blocks, restore_blocks->count + 1)){
               auto* new_restore_block = restore_blocks->elements + (restore_blocks->count - 1);
               *new_restore_block = restore_block;
          }else{
               LOG("ran out of memory trying to allocate %d restore blocks\n", restore_blocks->count + 1);
          }
     }
     return would_push;
}

bool player_block_push_add_ordered_entangled_reqs(PlayerBlockPush_t* player_block_push,
                                                  ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                                  World_t* world, ObjectArray_t<PlayerBlockPush_t*>* ordered_player_block_pushes,
                                                  ObjectArray_t<RestoreBlock_t>* restore_blocks){
     if(player_block_push->is_entangled()){
          auto* entangled_player_block_push = player_block_pushes->elements + player_block_push->entangled_push_index;
          if(!player_block_push_add_ordered_physical_reqs(entangled_player_block_push, player_block_pushes, world,
                                                          ordered_player_block_pushes, restore_blocks)){
               return false;
          }
     }

     return true;
}
void add_pushes_for_against_results(BlockPushResult_t* push_result, ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                    S16 player_index, AllowedToPushResult_t* allowed_to_push_result, World_t* world){
     for(S16 a = 0; a < push_result->againsts_pushed.count; a++){
          if(!resize(player_block_pushes, player_block_pushes->count + 1)){
               LOG("%d: Ran out of memory trying to do player %d block pushes...\n", __LINE__, player_block_pushes->count + 1);
          }else{
               S16 against_entangled_push_index = (player_block_pushes->count - 1);
               auto* player_block_push = player_block_pushes->elements + against_entangled_push_index;
               player_block_push->performed = false;
               player_block_push->entangled_push_index = -1;
               player_block_push->player_index = player_index;
               player_block_push->block_index = push_result->againsts_pushed.objects[a].block - world->blocks.elements;
               player_block_push->direction = push_result->againsts_pushed.objects[a].direction;
               player_block_push->allowed_to_push = *allowed_to_push_result;

               add_entangled_player_block_pushes(player_block_pushes, push_result->againsts_pushed.objects[a].block,
                                                 push_result->againsts_pushed.objects[a].direction, allowed_to_push_result,
                                                 player_index, against_entangled_push_index, world);
          }
     }
}

void add_entangled_player_block_pushes(ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                       Block_t* block_to_push, Direction_t push_direction,
                                       AllowedToPushResult_t* allowed_to_push_result, S16 player_index,
                                       S16 entangled_push_index, World_t* world){
     Block_t save_block = *block_to_push;
     auto push_result = block_push(block_to_push, push_direction, world, false, allowed_to_push_result->mass_ratio, nullptr, nullptr, 1, false);
     add_pushes_for_against_results(&push_result, player_block_pushes, player_index, allowed_to_push_result, world);
     if(!push_result.busy){
          if(!push_result.pushed){
               // if we didn't push, pretend we did so we still add the entangle pushes, we will still undo this later
               BlockPushResult_t result {};
               block_do_push(block_to_push, block_to_push->pos, block_to_push->pos_delta, push_direction, world, false, &result,
                             allowed_to_push_result->mass_ratio, nullptr, nullptr);
          }

          PushFromEntangler_t from_entangler = build_push_from_entangler(block_to_push, push_direction, allowed_to_push_result->mass_ratio);

          S16 block_mass = block_get_mass(block_to_push);
          S16 original_block_index = block_to_push - world->blocks.elements;
          S16 entangle_index = block_to_push->entangle_index;
          while(entangle_index != (S16)(original_block_index) && entangle_index >= 0){
               Block_t* entangled_block = world->blocks.elements + entangle_index;
               bool held_down = block_held_down_by_another_block(entangled_block, world->block_qt, world->interactive_qt, &world->tilemap).held();
               bool on_frictionless = block_on_frictionless(entangled_block, &world->tilemap, world->interactive_qt, world->block_qt);
               if(!held_down || on_frictionless){
                    auto rotations_between = direction_rotations_between((Direction_t)(entangled_block->rotation), (Direction_t)(block_to_push->rotation));
                    Direction_t rotated_dir = direction_rotate_clockwise(push_direction, rotations_between);

                    S16 entangled_block_mass = block_get_mass(entangled_block);
                    F32 entangled_mass_ratio = (F32)(block_mass) / (F32)(entangled_block_mass);

                    auto entangle_allowed_result = allowed_to_push(world, entangled_block, rotated_dir, entangled_mass_ratio);
                    if(entangle_allowed_result.push){
                         if(!resize(player_block_pushes, player_block_pushes->count + 1)){
                              LOG("%d: Ran out of memory trying to do player %d block pushes...\n", __LINE__, player_block_pushes->count + 1);
                         }else{
                              // update entangled allowed result mass ratio because that is the force we are going to be using for our push
                              entangle_allowed_result.mass_ratio = allowed_to_push_result->mass_ratio * entangled_mass_ratio * entangle_allowed_result.mass_ratio;

                              auto* player_block_push = player_block_pushes->elements + (player_block_pushes->count - 1);
                              player_block_push->performed = false;
                              player_block_push->entangled_push_index = entangled_push_index;
                              player_block_push->player_index = player_index;
                              player_block_push->block_index = entangled_block - world->blocks.elements;
                              player_block_push->direction = rotated_dir;
                              player_block_push->allowed_to_push = entangle_allowed_result;
                              player_block_push->push_from_entangler = from_entangler;

                              Block_t save_entangled_block = *entangled_block;
                              auto entangled_push_result = block_push(entangled_block, rotated_dir, world, false, entangle_allowed_result.mass_ratio, nullptr, nullptr, 1, false);
                              if(entangled_push_result.pushed){
                                   add_pushes_for_against_results(&entangled_push_result, player_block_pushes, -1, &entangle_allowed_result, world);
                              }
                              *entangled_block = save_entangled_block;
                         }
                    }
               }
               entangle_index = entangled_block->entangle_index;
          }
     }
     *block_to_push = save_block;
}
