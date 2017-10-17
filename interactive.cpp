#include "interactive.h"

void lift_update(Lift_t* lift, float tick_delay, float dt, S8 min_tick, S8 max_tick){
     lift->timer += dt;

     if(lift->timer > tick_delay){
          lift->timer -= tick_delay;

          if(lift->up){
               if(lift->ticks < max_tick) lift->ticks++;
          }else{
               if(lift->ticks > min_tick) lift->ticks--;
          }
     }
}

bool interactive_is_solid(const Interactive_t* interactive){
     return (interactive->type == INTERACTIVE_TYPE_LEVER ||
             (interactive->type == INTERACTIVE_TYPE_POPUP && interactive->popup.lift.ticks > 1) ||
             (interactive->type == INTERACTIVE_TYPE_DOOR && interactive->door.lift.ticks < DOOR_MAX_HEIGHT));
}

S16 get_object_x(const Interactive_t* interactive){
     return interactive->coord.x;
}

S16 get_object_y(const Interactive_t* interactive){
     return interactive->coord.y;
}

