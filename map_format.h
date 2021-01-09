#pragma once

#include "coord.h"
#include "tile.h"
#include "object_array.h"
#include "block.h"
#include "interactive.h"
#include "raw.h"

#include <stdio.h>

// version 4 is just the addition of the thumbnail
// version 7 adds the iced flag to the pit
// version 8 adds rooms
#define MAP_VERSION 8

#pragma pack(push, 1)
struct MapTileV1_t{
     U8 id;
     U16 flags;
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

struct MapInteractiveV1_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          MapPopupV1_t popup;
          Stairs_t stairs;
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
          Stairs_t stairs;
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
          Stairs_t stairs;
          MapDoorV1_t door;
          PortalV1_t portal;
          WireCross_t wire_cross;
          PitV2_t pit;
     };
};
#pragma pack(pop)

bool save_map_to_file(FILE* file, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                      ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array, bool* tags, Raw_t* thumbnail);

bool save_map(const char* filepath, Coord_t player_start, const TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array, bool* tags, Raw_t* thumbnail);

bool load_map_from_file(FILE* file, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
                        ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array, const char* filepath);

bool load_map(const char* filepath, Coord_t* player_start, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array,
              ObjectArray_t<Interactive_t>* interactive_array, ObjectArray_t<Rect_t>* room_array);

bool load_map_thumbnail(const char* filepath, Raw_t* thumbnail);
bool load_map_tags(const char* filepath, bool* tags);
