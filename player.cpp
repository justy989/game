#include "player.h"

Rect_t get_player_rect(Player_t* player){

     S16 over_radius = static_cast<S16>(PLAYER_RADIUS_IN_SUB_PIXELS) + 1;

     Rect_t result;

     result.left = player->pos.pixel.x - over_radius;
     result.bottom = player->pos.pixel.y - over_radius;
     result.right = player->pos.pixel.x + over_radius;
     result.top = player->pos.pixel.y + over_radius;

     return result;
}
