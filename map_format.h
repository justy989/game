#pragma once

#include "coord.h"
#include "tile.h"
#include "object_array.h"
#include "block.h"
#include "interactive.h"
#include "raw.h"
#include "exit.h"

#include <stdio.h>

// version 4 is just the addition of the thumbnail
// version 7 adds the iced flag to the pit
// version 8 adds rooms, also we changed TILE_FLAG_CHECKPOINT to TILE_FLAG_SOLID, and adds rotation to tiles
// version 9 adds stairs that reference the exit and a list of exits for the map
#define MAP_VERSION 9

#pragma pack(push, 1)
struct MapTileV1_t{
     U8 id;
     U16 flags;
};

struct MapTileV2_t{
     U8 id;
     U16 flags;
     U8 rotation;
};

struct MapBlockV1_t{
     Pixel_t pixel;
     U8 rotation;
     Element_t element;
     S8 z;
};

struct MapBlockV2_t{
     Pixel_t pixel;
     U8 rotation;
     Element_t element;
     S8 z;
     S16 entangle_index;
};

struct MapBlockV3_t{
     Pixel_t pixel;
     U8 rotation;
     Element_t element;
     S8 z;
     S16 entangle_index;
     BlockCut_t cut;
};

struct MapPopupV1_t{
     bool up;
     bool iced;
};

struct MapDoorV1_t{
     bool up;
     Direction_t face;
};

struct MapDoorV2_t{
     bool up;
     Direction_t face;
     U8 ticks;
};

struct PortalV1_t{
    Direction_t face;
    bool on;
};

struct PitV1_t{
    S8 id;
};

struct PitV2_t{
    S8 id;
    bool iced;
};

struct StairsV1_t{
     bool up;
     Direction_t face;
};

struct StairsV2_t{
     bool up;
     Direction_t face;
     U8 exit_index;
};

struct MapInteractiveV1_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          StairsV1_t stairs;
          MapDoorV1_t door; // up or down
          PortalV1_t portal;
          WireCross_t wire_cross;
     };
};

struct MapInteractiveV2_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          StairsV1_t stairs;
          MapDoorV1_t door;
          PortalV1_t portal;
          WireCross_t wire_cross;
          PitV1_t pit;
     };
};

struct MapInteractiveV3_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          StairsV1_t stairs;
          MapDoorV1_t door;
          PortalV1_t portal;
          WireCross_t wire_cross;
          PitV2_t pit;
     };
};

struct MapInteractiveV4_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          StairsV2_t stairs;
          MapDoorV2_t door;
          PortalV1_t portal;
          WireCross_t wire_cross;
          PitV2_t pit;
     };
};
#pragma pack(pop)

bool save_map_to_file(FILE* file, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                      ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
                      ObjectArray_t<Exit_t>* exit_array, bool* tags, Raw_t* thumbnail);

bool save_map(const char* filepath, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
              ObjectArray_t<Exit_t>* exit_array, bool* tags, Raw_t* thumbnail);

bool load_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                        ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
                        ObjectArray_t<Exit_t>* exit_array, const char* filepath);

bool load_map(const char* filepath, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array,
              ObjectArray_t<Exit_t>* exit_array);

bool load_map_thumbnail(const char* filepath, Raw_t* thumbnail);
bool load_map_tags(const char* filepath, bool* tags);

void build_map_interactive_from_interactive(MapInteractiveV4_t* map_interactive, const Interactive_t* interactive);
void build_interactive_from_map_interactive(Interactive_t* interactive, const MapInteractiveV4_t* map_interactive);

void build_map_block_from_block(MapBlockV3_t* map_block, const Block_t* block);
void build_block_from_map_block(Block_t* block, const MapBlockV3_t* map_block);
