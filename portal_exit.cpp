#include "portal_exit.h"
#include "utils.h"

static bool is_acceptable_portal(Interactive_t* interactive, bool require_on, bool from_on_wire){
    if(require_on) return is_active_portal(interactive);

    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
        if(!from_on_wire){
            return interactive->portal.wants_to_turn_off;
        }
        return true;
    }
    return false;
}

void portal_exit_add(PortalExit_t* portal_exit, Direction_t direction, Coord_t coord){
     PortalExitCoords_t* portal_exit_direction = portal_exit->directions + direction;
     if(portal_exit_direction->count < MAX_PORTAL_EXITS){
          portal_exit_direction->coords[portal_exit_direction->count] = coord;
          portal_exit_direction->count++;
     }
}

void find_portal_exits_impl(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                            PortalExit_t* portal_exit, Direction_t from, bool from_on_wire, bool require_on){
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(is_acceptable_portal(interactive, require_on, from_on_wire)){
          portal_exit_add(portal_exit, interactive->portal.face, coord);
          return;
     }

     bool connecting_to_wire_cross = false;

     if(interactive && interactive->type == INTERACTIVE_TYPE_WIRE_CROSS){
          if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT && from == DIRECTION_LEFT){
               connecting_to_wire_cross = true;
          }else if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT && from == DIRECTION_RIGHT){
               connecting_to_wire_cross = true;
          }else if(interactive->wire_cross.mask & DIRECTION_MASK_UP && from == DIRECTION_UP){
               connecting_to_wire_cross = true;
          }else if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN && from == DIRECTION_DOWN){
               connecting_to_wire_cross = true;
          }
     }

     if(connecting_to_wire_cross){
          if((require_on && interactive->wire_cross.on) | !require_on){
              if(interactive->wire_cross.mask & DIRECTION_MASK_LEFT && from != DIRECTION_LEFT){
                   find_portal_exits_impl(coord + DIRECTION_LEFT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_RIGHT, interactive->wire_cross.on, require_on);
              }

              if(interactive->wire_cross.mask & DIRECTION_MASK_RIGHT && from != DIRECTION_RIGHT){
                   find_portal_exits_impl(coord + DIRECTION_RIGHT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_LEFT, interactive->wire_cross.on, require_on);
              }

              if(interactive->wire_cross.mask & DIRECTION_MASK_UP && from != DIRECTION_UP){
                   find_portal_exits_impl(coord + DIRECTION_UP, tilemap, interactive_quad_tree, portal_exit, DIRECTION_DOWN, interactive->wire_cross.on, require_on);
              }

              if(interactive->wire_cross.mask & DIRECTION_MASK_DOWN && from != DIRECTION_DOWN){
                   find_portal_exits_impl(coord + DIRECTION_DOWN, tilemap, interactive_quad_tree, portal_exit, DIRECTION_UP, interactive->wire_cross.on, require_on);
              }
          }
     }else{
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          bool wire_on = (tile->flags & TILE_FLAG_WIRE_STATE);
          if(tile && ((require_on && wire_on) || !require_on)){
               if((tile->flags & TILE_FLAG_WIRE_LEFT) && from != DIRECTION_LEFT){
                    find_portal_exits_impl(coord + DIRECTION_LEFT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_RIGHT, wire_on, require_on);
               }

               if((tile->flags & TILE_FLAG_WIRE_RIGHT) && from != DIRECTION_RIGHT){
                    find_portal_exits_impl(coord + DIRECTION_RIGHT, tilemap, interactive_quad_tree, portal_exit, DIRECTION_LEFT, wire_on, require_on);
               }

               if((tile->flags & TILE_FLAG_WIRE_UP) && from != DIRECTION_UP){
                    find_portal_exits_impl(coord + DIRECTION_UP, tilemap, interactive_quad_tree, portal_exit, DIRECTION_DOWN, wire_on, require_on);
               }

               if((tile->flags & TILE_FLAG_WIRE_DOWN) && from != DIRECTION_DOWN){
                    find_portal_exits_impl(coord + DIRECTION_DOWN, tilemap, interactive_quad_tree, portal_exit, DIRECTION_UP, wire_on, require_on);
               }
          }
     }
}

PortalExit_t find_portal_exits(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                               bool require_on){
     PortalExit_t portal_exit = {};
     Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(is_acceptable_portal(interactive, require_on, true)){
          for(S8 d = 0; d < DIRECTION_COUNT; d++){
               Coord_t adjacent_coord = coord + (Direction_t)(d);
               Tile_t* tile = tilemap_get_tile(tilemap, adjacent_coord);
               if(!tile) continue;
               bool wire_on = (tile->flags & TILE_FLAG_WIRE_STATE);
               if(require_on){
                   if(!wire_on) continue;
               }
               if((d == DIRECTION_LEFT  && (tile->flags & TILE_FLAG_WIRE_RIGHT)) ||
                  (d == DIRECTION_UP    && (tile->flags & TILE_FLAG_WIRE_DOWN)) ||
                  (d == DIRECTION_RIGHT && (tile->flags & TILE_FLAG_WIRE_LEFT)) ||
                  (d == DIRECTION_DOWN  && (tile->flags & TILE_FLAG_WIRE_UP))){
                    find_portal_exits_impl(adjacent_coord, tilemap, interactive_quad_tree,
                                           &portal_exit, DIRECTION_COUNT, wire_on, require_on);
               }
          }
     }
     return portal_exit;
}

S8 portal_exit_count(const PortalExit_t* portal_exit){
    S8 count = 0;
    for (const auto &direction : portal_exit->directions) {
        count += direction.count;
    }
    return count;
}
