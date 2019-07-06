#include "player.h"
#include "defines.h"

void get_player_adjacent_positions(Player_t* player, Direction_t direction, Position_t* a, Position_t* b){
     if(player->teleport){
          *a = player->teleport_pos;
          *b = player->teleport_pos;
     }else{
          *a = player->pos;
          *b = player->pos;
     }

     switch(direction){
     case DIRECTION_LEFT:
          *a += Vec_t{-(PLAYER_RADIUS + PIXEL_SIZE), -PLAYER_RADIUS};
          *b += Vec_t{-(PLAYER_RADIUS + PIXEL_SIZE), PLAYER_RADIUS};
          break;
     case DIRECTION_RIGHT:
          *a += Vec_t{PLAYER_RADIUS + PIXEL_SIZE, -PLAYER_RADIUS};
          *b += Vec_t{PLAYER_RADIUS + PIXEL_SIZE, PLAYER_RADIUS};
          break;
     case DIRECTION_UP:
          *a += Vec_t{-PLAYER_RADIUS, PLAYER_RADIUS + PIXEL_SIZE};
          *b += Vec_t{PLAYER_RADIUS,  PLAYER_RADIUS + PIXEL_SIZE};
          break;
     case DIRECTION_DOWN:
          *a += Vec_t{-PLAYER_RADIUS, -(PLAYER_RADIUS + PIXEL_SIZE)};
          *b += Vec_t{PLAYER_RADIUS,  -(PLAYER_RADIUS + PIXEL_SIZE)};
          break;
     default:
          break;
     }
}

Position_t get_player_adjacent_position(Player_t* player, Direction_t direction){
     Position_t result;

     if(player->teleport){
          result = player->teleport_pos;
     }else{
          result = player->pos;
     }

     switch(direction){
     case DIRECTION_LEFT:
          result += Vec_t{-(PLAYER_RADIUS + PIXEL_SIZE), 0};
          break;
     case DIRECTION_RIGHT:
          result += Vec_t{PLAYER_RADIUS + PIXEL_SIZE, 0};
          break;
     case DIRECTION_UP:
          result += Vec_t{0, PLAYER_RADIUS + PIXEL_SIZE};
          break;
     case DIRECTION_DOWN:
          result += Vec_t{0, -(PLAYER_RADIUS + PIXEL_SIZE)};
          break;
     default:
          break;
     }

     return result;
}
