#pragma once

#include "tile.h"
#include "quad_tree.h"
#include "interactive.h"

#define MAX_PORTAL_EXITS 4

struct PortalExitCoords_t{
     Coord_t coords[MAX_PORTAL_EXITS];
     S8 count;
};

struct PortalExit_t{
     PortalExitCoords_t directions[DIRECTION_COUNT];
};

void portal_exit_add(PortalExit_t* portal_exit, Direction_t direction, Coord_t coord);
void find_portal_exits_impl(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                            PortalExit_t* portal_exit, Direction_t from, bool from_on_wire, bool require_on = true);
PortalExit_t find_portal_exits(Coord_t coord, TileMap_t* tilemap, QuadTreeNode_t<Interactive_t>* interactive_quad_tree,
                               bool require_on = true);

S8 portal_exit_count(const PortalExit_t* portal_exit);
