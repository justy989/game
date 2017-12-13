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
             (interactive->type == INTERACTIVE_TYPE_DOOR && interactive->door.lift.ticks < DOOR_MAX_HEIGHT) ||
             (interactive->type == INTERACTIVE_TYPE_PORTAL && !interactive->portal.on));
}

S16 get_object_x(const Interactive_t* interactive){
     return interactive->coord.x;
}

S16 get_object_y(const Interactive_t* interactive){
     return interactive->coord.y;
}

bool interactive_equal(const Interactive_t* a, const Interactive_t* b){
     if(a->type != b->type) return false;
     if(a->coord != b->coord) return false;

     switch(b->type){
     default:
          break;
     case INTERACTIVE_TYPE_PRESSURE_PLATE:
          if(a->pressure_plate.down != b->pressure_plate.down ||
             a->pressure_plate.iced_under != b->pressure_plate.iced_under){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_ICE_DETECTOR:
     case INTERACTIVE_TYPE_LIGHT_DETECTOR:
          if(a->detector.on != b->detector.on){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_POPUP:
          if(a->popup.iced != b->popup.iced ||
             a->popup.lift.up != b->popup.lift.up ||
             a->popup.lift.ticks != b->popup.lift.ticks){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_LEVER:
          // TODO
          break;
     case INTERACTIVE_TYPE_DOOR:
          if(a->door.lift.up != b->door.lift.up ||
             a->door.lift.ticks != b->door.lift.ticks){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_PORTAL:
          if(a->portal.on != b->portal.on){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_BOW:
          break;
     }

     return true;
}
