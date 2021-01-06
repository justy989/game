#pragma once

#include "world.h"

struct PlayerBlockPush_t{
     S16 player_index = 0;
     Direction_t direction = DIRECTION_COUNT;
     U32 block_index = 0;
     AllowedToPushResult_t allowed_to_push;
     PushFromEntangler_t push_from_entangler = {};
     S16 entangled_push_index = -1; // -1 sentinal that it isn't entangled
     bool performed = false;

     bool is_entangled(){return entangled_push_index >= 0;}
};

struct RestoreBlock_t{
     Block_t block;
     S16 index = -1;
};

bool player_block_push_add_ordered_physical_reqs(PlayerBlockPush_t* player_block_push, ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                                 World_t* world, ObjectArray_t<PlayerBlockPush_t*>* ordered_player_block_pushes,
                                                 ObjectArray_t<RestoreBlock_t>* restore_blocks);
bool player_block_push_add_ordered_entangled_reqs(PlayerBlockPush_t* player_block_push,
                                                  ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                                  World_t* world, ObjectArray_t<PlayerBlockPush_t*>* ordered_player_block_pushes,
                                                  ObjectArray_t<RestoreBlock_t>* restore_blocks);
void add_pushes_for_against_results(BlockPushResult_t* push_result, ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                    S16 player_index, AllowedToPushResult_t* allowed_to_push_result, World_t* world);
void add_entangled_player_block_pushes(ObjectArray_t<PlayerBlockPush_t>* player_block_pushes,
                                       Block_t* block_to_push, Direction_t push_direction,
                                       AllowedToPushResult_t* allowed_to_push_result, S16 player_index,
                                       S16 entangled_push_index, World_t* world);
