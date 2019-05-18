#include "carried_pos_delta.h"

void carried_pos_delta_reset(CarriedPosDelta_t* carried_pos_delta){
     carried_pos_delta->block_index = -1;
     carried_pos_delta->positive = vec_zero();
     carried_pos_delta->negative = vec_zero();
}

bool get_carried_noob(CarriedPosDelta_t* carried_pos_delta, Vec_t carry_vec, S16 block_index, bool through_entangler){
     bool no_change = false;

     if(carry_vec.x > 0){
          if(carry_vec.x > carried_pos_delta->positive.x){
               carried_pos_delta->positive.x = carry_vec.x;
               if(!through_entangler) carried_pos_delta->block_index = block_index;
          }else if(carried_pos_delta->block_index == block_index &&
                   carry_vec.x < carried_pos_delta->positive.x){
               carried_pos_delta->positive.x = carry_vec.x;
          }else{
               no_change = true;
          }
     }else if(carry_vec.x < 0){
          if(carry_vec.x < carried_pos_delta->negative.x){
               carried_pos_delta->negative.x = carry_vec.x;
               if(!through_entangler) carried_pos_delta->block_index = block_index;
          }else if(carried_pos_delta->block_index == block_index &&
                   carry_vec.x > carried_pos_delta->negative.x){
               carried_pos_delta->negative.x = carry_vec.x;
          }else{
               no_change = true;
          }
     }

     if(carry_vec.y > 0){
          if(carry_vec.y > carried_pos_delta->positive.y){
               carried_pos_delta->positive.y = carry_vec.y;
               if(!through_entangler) carried_pos_delta->block_index = block_index;
          }else if(carried_pos_delta->block_index == block_index &&
                   carry_vec.y < carried_pos_delta->positive.y){
               carried_pos_delta->positive.y = carry_vec.y;
          }else{
               no_change = true;
          }
     }else if(carry_vec.y < 0){
          if(carry_vec.y < carried_pos_delta->negative.y){
               carried_pos_delta->negative.y = carry_vec.y;
               if(!through_entangler) carried_pos_delta->block_index = block_index;
          }else if(carried_pos_delta->block_index == block_index &&
                   carry_vec.y > carried_pos_delta->negative.y){
               carried_pos_delta->negative.y = carry_vec.y;
          }else{
               no_change = true;
          }
     }

     return no_change;
}
