#include "interactive.h"
#include "conversion.h"
#include "defines.h"

void lift_update(Lift_t* lift, float tick_delay, float dt, S8 min_tick, S8 max_tick){
     if(lift->up && lift->ticks < max_tick){
          lift->timer += dt;
          if(lift->timer > tick_delay){
               lift->timer -= tick_delay;
               lift->ticks++;
          }
     }else if(!lift->up && lift->ticks > min_tick){
          lift->timer += dt;
          if(lift->timer > tick_delay){
               lift->timer -= tick_delay;
               lift->ticks--;
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
     case INTERACTIVE_TYPE_WIRE_CROSS:
          if(a->wire_cross.mask != b->wire_cross.mask ||
             a->wire_cross.on != b->wire_cross.on){
               return false;
          }
          break;
     case INTERACTIVE_TYPE_PIT:
          if(a->pit.iced != b->pit.iced){
               return false;
          }
          break;
     }

     return true;
}

bool is_active_portal(const Interactive_t* interactive){
     return interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on;
}

AxisLine_t get_portal_line(const Interactive_t* interactive){
     AxisLine_t result = {};
     if(interactive->type != INTERACTIVE_TYPE_PORTAL) return result;

     auto top_left_pixel = coord_to_pixel(interactive->coord);

     switch(interactive->portal.face){
     default:
          break;
     case DIRECTION_LEFT:
          result.vertical = true;
          result.offset = top_left_pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS;
          result.min = top_left_pixel.y;
          result.max = top_left_pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS;
          break;
     case DIRECTION_RIGHT:
          result.vertical = true;
          result.offset = top_left_pixel.x;
          result.min = top_left_pixel.y;
          result.max = top_left_pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS;
          break;
     case DIRECTION_UP:
          result.vertical = false;
          result.offset = top_left_pixel.y;
          result.min = top_left_pixel.x;
          result.max = top_left_pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS;
          break;
     case DIRECTION_DOWN:
          result.vertical = false;
          result.offset = top_left_pixel.y + BLOCK_SOLID_SIZE_IN_PIXELS;
          result.min = top_left_pixel.x;
          result.max = top_left_pixel.x + BLOCK_SOLID_SIZE_IN_PIXELS;
          break;
     }

     return result;
}
