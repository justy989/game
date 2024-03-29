#include "editor.h"
#include "defines.h"
#include "conversion.h"
#include "utils.h"

#include <string.h>

bool init(Editor_t* editor){
     memset(editor, 0, sizeof(*editor));

     editor->entangle_indices.elements = nullptr;
     editor->entangle_indices.count = 0;

     init(&editor->category_array, EDITOR_CATEGORY_COUNT);

     auto* tile_floor_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_FLOOR;
     init(tile_floor_category, 15);

     for(S16 i = 0; i < 15; i++){
          init(&tile_floor_category->elements[i], 1);
          tile_floor_category->elements[i].elements[0].type = STAMP_TYPE_TILE_ID;
          tile_floor_category->elements[i].elements[0].tile.id = i;
          tile_floor_category->elements[i].elements[0].tile.rotation = 0;
          tile_floor_category->elements[i].elements[0].tile.solid = false;
     }

     auto* tile_solid_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_SOLIDS;
     init(tile_solid_category, 12);

     init(&tile_solid_category->elements[0], 2);
     tile_solid_category->elements[0].elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[0].elements[0].tile.id = 1;
     tile_solid_category->elements[0].elements[0].tile.rotation = 0;
     tile_solid_category->elements[0].elements[0].tile.solid = true;
     tile_solid_category->elements[0].elements[0].offset = Coord_t{0, 0};
     tile_solid_category->elements[0].elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[0].elements[1].tile.id = 0;
     tile_solid_category->elements[0].elements[1].tile.rotation = 0;
     tile_solid_category->elements[0].elements[1].tile.solid = true;
     tile_solid_category->elements[0].elements[1].offset = Coord_t{0, 1};

     init(&tile_solid_category->elements[1], 2);
     tile_solid_category->elements[1].elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[1].elements[0].tile.id = 0;
     tile_solid_category->elements[1].elements[0].tile.rotation = 2;
     tile_solid_category->elements[1].elements[0].tile.solid = true;
     tile_solid_category->elements[1].elements[0].offset = Coord_t{0, 0};
     tile_solid_category->elements[1].elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[1].elements[1].tile.id = 1;
     tile_solid_category->elements[1].elements[1].tile.rotation = 2;
     tile_solid_category->elements[1].elements[1].tile.solid = true;
     tile_solid_category->elements[1].elements[1].offset = Coord_t{0, 1};

     init(&tile_solid_category->elements[2], 2);
     tile_solid_category->elements[2].elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[2].elements[0].tile.id = 1;
     tile_solid_category->elements[2].elements[0].tile.rotation = 1;
     tile_solid_category->elements[2].elements[0].tile.solid = true;
     tile_solid_category->elements[2].elements[0].offset = Coord_t{0, 0};
     tile_solid_category->elements[2].elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[2].elements[1].tile.id = 0;
     tile_solid_category->elements[2].elements[1].tile.rotation = 1;
     tile_solid_category->elements[2].elements[1].tile.solid = true;
     tile_solid_category->elements[2].elements[1].offset = Coord_t{1, 0};

     init(&tile_solid_category->elements[3], 2);
     tile_solid_category->elements[3].elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[3].elements[0].tile.id = 0;
     tile_solid_category->elements[3].elements[0].tile.rotation = 3;
     tile_solid_category->elements[3].elements[0].tile.solid = true;
     tile_solid_category->elements[3].elements[0].offset = Coord_t{0, 0};
     tile_solid_category->elements[3].elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_category->elements[3].elements[1].tile.id = 1;
     tile_solid_category->elements[3].elements[1].tile.rotation = 3;
     tile_solid_category->elements[3].elements[1].tile.solid = true;
     tile_solid_category->elements[3].elements[1].offset = Coord_t{1, 0};

     auto *tile_solid_stamp = tile_solid_category->elements + 4;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 3;
     tile_solid_stamp->elements[0].tile.rotation = 0;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 0;
     tile_solid_stamp->elements[1].tile.rotation = 2;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 0;
     tile_solid_stamp->elements[2].tile.rotation = 3;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 2;
     tile_solid_stamp->elements[3].tile.rotation = 0;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 0;
     tile_solid_stamp->elements[0].tile.rotation = 2;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 3;
     tile_solid_stamp->elements[1].tile.rotation = 3;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 2;
     tile_solid_stamp->elements[2].tile.rotation = 3;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 0;
     tile_solid_stamp->elements[3].tile.rotation = 1;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 2;
     tile_solid_stamp->elements[0].tile.rotation = 2;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 0;
     tile_solid_stamp->elements[1].tile.rotation = 1;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 0;
     tile_solid_stamp->elements[2].tile.rotation = 0;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 3;
     tile_solid_stamp->elements[3].tile.rotation = 2;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 0;
     tile_solid_stamp->elements[0].tile.rotation = 3;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 2;
     tile_solid_stamp->elements[1].tile.rotation = 1;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 3;
     tile_solid_stamp->elements[2].tile.rotation = 1;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 0;
     tile_solid_stamp->elements[3].tile.rotation = 0;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 5;
     tile_solid_stamp->elements[0].tile.rotation = 0;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 1;
     tile_solid_stamp->elements[1].tile.rotation = 0;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 1;
     tile_solid_stamp->elements[2].tile.rotation = 1;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 4;
     tile_solid_stamp->elements[3].tile.rotation = 0;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 1;
     tile_solid_stamp->elements[0].tile.rotation = 0;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 5;
     tile_solid_stamp->elements[1].tile.rotation = 3;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 4;
     tile_solid_stamp->elements[2].tile.rotation = 3;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 1;
     tile_solid_stamp->elements[3].tile.rotation = 3;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 4;
     tile_solid_stamp->elements[0].tile.rotation = 2;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 1;
     tile_solid_stamp->elements[1].tile.rotation = 3;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 1;
     tile_solid_stamp->elements[2].tile.rotation = 2;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 5;
     tile_solid_stamp->elements[3].tile.rotation = 2;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};
     tile_solid_stamp++;

     init(tile_solid_stamp, 4);
     tile_solid_stamp->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[0].tile.id = 1;
     tile_solid_stamp->elements[0].tile.rotation = 1;
     tile_solid_stamp->elements[0].tile.solid = true;
     tile_solid_stamp->elements[0].offset = Coord_t{0, 0};
     tile_solid_stamp->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[1].tile.id = 4;
     tile_solid_stamp->elements[1].tile.rotation = 1;
     tile_solid_stamp->elements[1].tile.solid = true;
     tile_solid_stamp->elements[1].offset = Coord_t{1, 0};
     tile_solid_stamp->elements[2].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[2].tile.id = 5;
     tile_solid_stamp->elements[2].tile.rotation = 1;
     tile_solid_stamp->elements[2].tile.solid = true;
     tile_solid_stamp->elements[2].offset = Coord_t{0, 1};
     tile_solid_stamp->elements[3].type = STAMP_TYPE_TILE_ID;
     tile_solid_stamp->elements[3].tile.id = 1;
     tile_solid_stamp->elements[3].tile.rotation = 2;
     tile_solid_stamp->elements[3].tile.solid = true;
     tile_solid_stamp->elements[3].offset = Coord_t{1, 1};

     auto* tile_flags_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_FLAGS;
     init(tile_flags_category, 34);

     S16 index_offset = 0;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_UP;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_DOWN;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_DOWN;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_DOWN | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_LEFT | TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_UP | TILE_FLAG_WIRE_DOWN | TILE_FLAG_WIRE_RIGHT;
     index_offset++;
     init(&tile_flags_category->elements[index_offset], 1);
     tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_RIGHT | TILE_FLAG_WIRE_DOWN;

     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          index_offset = (S16)(15 + (i * 4));
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_RIGHT;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_LEFT | TILE_FLAG_WIRE_CLUSTER_MID | TILE_FLAG_WIRE_CLUSTER_RIGHT;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
     }

     init(tile_flags_category->elements + 32, 1);
     tile_flags_category->elements[32].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[32].elements[0].tile_flags = TILE_FLAG_ICED;
     init(tile_flags_category->elements + 33, 1);
     tile_flags_category->elements[33].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[33].elements[0].tile_flags = TILE_FLAG_RESET_IMMUNE;

     auto* block_category = editor->category_array.elements + EDITOR_CATEGORY_BLOCK;
     init(block_category, 24);
     for(S16 d = 0; d < DIRECTION_COUNT; d++){
          for(S16 i = 0; i < ELEMENT_COUNT; i++){
               S16 index = i + (d * ELEMENT_COUNT);
               init(block_category->elements + index, 1);

               block_category->elements[index].elements[0].type = STAMP_TYPE_BLOCK;
               block_category->elements[index].elements[0].block.face = DIRECTION_LEFT;
               block_category->elements[index].elements[0].block.element = (Element_t)(i);
               block_category->elements[index].elements[0].block.rotation = d;
          }
     }

     init(block_category->elements + 16, 1);
     block_category->elements[16].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[16].elements[0].block.cut = BLOCK_CUT_LEFT_HALF;

     init(block_category->elements + 17, 1);
     block_category->elements[17].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[17].elements[0].block.cut = BLOCK_CUT_RIGHT_HALF;

     init(block_category->elements + 18, 1);
     block_category->elements[18].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[18].elements[0].block.cut = BLOCK_CUT_TOP_HALF;

     init(block_category->elements + 19, 1);
     block_category->elements[19].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[19].elements[0].block.cut = BLOCK_CUT_BOTTOM_HALF;

     init(block_category->elements + 20, 1);
     block_category->elements[20].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[20].elements[0].block.cut = BLOCK_CUT_TOP_LEFT_QUARTER;

     init(block_category->elements + 21, 1);
     block_category->elements[21].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[21].elements[0].block.cut = BLOCK_CUT_TOP_RIGHT_QUARTER;

     init(block_category->elements + 22, 1);
     block_category->elements[22].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[22].elements[0].block.cut = BLOCK_CUT_BOTTOM_LEFT_QUARTER;

     init(block_category->elements + 23, 1);
     block_category->elements[23].elements[0].type = STAMP_TYPE_BLOCK;
     block_category->elements[23].elements[0].block.cut = BLOCK_CUT_BOTTOM_RIGHT_QUARTER;

     auto* interactive_lever_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_LEVER;
     init(interactive_lever_category, 1);
     init(interactive_lever_category->elements, 1);
     interactive_lever_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_lever_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_LEVER;

     auto* interactive_pressure_plate_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_PRESSURE_PLATE;
     init(interactive_pressure_plate_category, 2);
     init(interactive_pressure_plate_category->elements, 1);
     interactive_pressure_plate_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_pressure_plate_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_PRESSURE_PLATE;
     init(interactive_pressure_plate_category->elements + 1, 1);
     interactive_pressure_plate_category->elements[1].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_pressure_plate_category->elements[1].elements[0].interactive.type = INTERACTIVE_TYPE_PRESSURE_PLATE;
     interactive_pressure_plate_category->elements[1].elements[0].interactive.pressure_plate.iced_under = true;

     auto* interactive_popup_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_POPUP;
     init(interactive_popup_category, 4);
     init(interactive_popup_category->elements, 1);
     interactive_popup_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_popup_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_POPUP;
     interactive_popup_category->elements[0].elements[0].interactive.popup.lift.ticks = HEIGHT_INTERVAL + 1;
     interactive_popup_category->elements[0].elements[0].interactive.popup.lift.up = true;
     init(interactive_popup_category->elements + 1, 1);
     interactive_popup_category->elements[1].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_popup_category->elements[1].elements[0].interactive.type = INTERACTIVE_TYPE_POPUP;
     interactive_popup_category->elements[1].elements[0].interactive.popup.lift.ticks = 1;
     interactive_popup_category->elements[1].elements[0].interactive.popup.lift.up = false;
     init(interactive_popup_category->elements + 2, 2);
     interactive_popup_category->elements[2].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_popup_category->elements[2].elements[0].interactive.type = INTERACTIVE_TYPE_POPUP;
     interactive_popup_category->elements[2].elements[0].interactive.popup.lift.ticks = HEIGHT_INTERVAL + 1;
     interactive_popup_category->elements[2].elements[0].interactive.popup.lift.up = true;
     interactive_popup_category->elements[2].elements[0].interactive.popup.iced    = true;
     init(interactive_popup_category->elements + 3, 1);
     interactive_popup_category->elements[3].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_popup_category->elements[3].elements[0].interactive.type = INTERACTIVE_TYPE_POPUP;
     interactive_popup_category->elements[3].elements[0].interactive.popup.lift.ticks = 1;
     interactive_popup_category->elements[3].elements[0].interactive.popup.lift.up = false;
     interactive_popup_category->elements[3].elements[0].interactive.popup.iced    = true;

     auto* interactive_door_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_DOOR;
     init(interactive_door_category, 8);
     for(S8 j = 0; j < DIRECTION_COUNT; j++){
          init(interactive_door_category->elements + j, 1);
          interactive_door_category->elements[j].elements[0].type = STAMP_TYPE_INTERACTIVE;
          interactive_door_category->elements[j].elements[0].interactive.type = INTERACTIVE_TYPE_DOOR;
          interactive_door_category->elements[j].elements[0].interactive.door.lift.ticks = DOOR_MAX_HEIGHT;
          interactive_door_category->elements[j].elements[0].interactive.door.lift.up = true;
          interactive_door_category->elements[j].elements[0].interactive.door.face = (Direction_t)(j);
     }

     for(S8 j = 0; j < DIRECTION_COUNT; j++){
          init(interactive_door_category->elements + j + 4, 1);
          interactive_door_category->elements[j + 4].elements[0].type = STAMP_TYPE_INTERACTIVE;
          interactive_door_category->elements[j + 4].elements[0].interactive.type = INTERACTIVE_TYPE_DOOR;
          interactive_door_category->elements[j + 4].elements[0].interactive.door.lift.ticks = 0;
          interactive_door_category->elements[j + 4].elements[0].interactive.door.lift.up = false;
          interactive_door_category->elements[j + 4].elements[0].interactive.door.face = (Direction_t)(j);

     }

     auto* interactive_light_detector_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_LIGHT_DETECTOR;
     init(interactive_light_detector_category, 1);
     init(interactive_light_detector_category->elements, 1);
     interactive_light_detector_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_light_detector_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_LIGHT_DETECTOR;

     auto* interactive_ice_detector_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_ICE_DETECTOR;
     init(interactive_ice_detector_category, 1);
     init(interactive_ice_detector_category->elements, 1);
     interactive_ice_detector_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_ice_detector_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_ICE_DETECTOR;

     auto* interactive_bow_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_BOW;
     init(interactive_bow_category, 1);
     init(interactive_bow_category->elements, 1);
     interactive_bow_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_bow_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_BOW;

     auto* interactive_portal_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_PORTAL;
     init(interactive_portal_category, 8);
     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          init(interactive_portal_category->elements + i, 1);
          interactive_portal_category->elements[i].elements[0].type = STAMP_TYPE_INTERACTIVE;
          interactive_portal_category->elements[i].elements[0].interactive.type = INTERACTIVE_TYPE_PORTAL;
          interactive_portal_category->elements[i].elements[0].interactive.portal.face = (Direction_t)(i);
          interactive_portal_category->elements[i].elements[0].interactive.portal.on = false;
     }

     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          S8 index = i + DIRECTION_COUNT;
          init(interactive_portal_category->elements + index, 1);
          interactive_portal_category->elements[index].elements[0].type = STAMP_TYPE_INTERACTIVE;
          interactive_portal_category->elements[index].elements[0].interactive.type = INTERACTIVE_TYPE_PORTAL;
          interactive_portal_category->elements[index].elements[0].interactive.portal.face = (Direction_t)(i);
          interactive_portal_category->elements[index].elements[0].interactive.portal.on = true;
     }

     auto* interactive_wire_cross_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_WIRE_CROSS;
     init(interactive_wire_cross_category, 12);
     S8 index = 0;

     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          for(S8 j = i; j < DIRECTION_COUNT; j++){
               if(i == j) continue;

               auto mask = direction_to_direction_mask(static_cast<Direction_t>(i));
               mask = direction_mask_add(mask, static_cast<Direction_t>(j));

               init(interactive_wire_cross_category->elements + index, 1);
               interactive_wire_cross_category->elements[index].elements[0].type = STAMP_TYPE_INTERACTIVE;
               interactive_wire_cross_category->elements[index].elements[0].interactive.type = INTERACTIVE_TYPE_WIRE_CROSS;
               interactive_wire_cross_category->elements[index].elements[0].interactive.wire_cross.mask = mask;

               index++;
          }
     }

     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          for(S8 j = i; j < DIRECTION_COUNT; j++){
               if(i == j) continue;

               auto mask = direction_to_direction_mask(static_cast<Direction_t>(i));
               mask = direction_mask_add(mask, static_cast<Direction_t>(j));

               init(interactive_wire_cross_category->elements + index, 1);
               interactive_wire_cross_category->elements[index].elements[0].type = STAMP_TYPE_INTERACTIVE;
               interactive_wire_cross_category->elements[index].elements[0].interactive.type = INTERACTIVE_TYPE_WIRE_CROSS;
               interactive_wire_cross_category->elements[index].elements[0].interactive.wire_cross.mask = mask;
               interactive_wire_cross_category->elements[index].elements[0].interactive.wire_cross.on = true;

               index++;
          }
     }

     auto* interactive_clone_killer_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_CLONE_KILLER;
     init(interactive_clone_killer_category, 1);
     init(interactive_clone_killer_category->elements, 1);
     interactive_clone_killer_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_clone_killer_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_CLONE_KILLER;

     auto* interactive_pit_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_PIT;
     init(interactive_pit_category, 22);
     for(S8 i = 0; i < 22; i++){
          init(interactive_pit_category->elements + i, 1);
          interactive_pit_category->elements[i].elements[0].type = STAMP_TYPE_INTERACTIVE;
          interactive_pit_category->elements[i].elements[0].interactive.type = INTERACTIVE_TYPE_PIT;
          interactive_pit_category->elements[i].elements[0].interactive.pit.id = i;
     }

     auto* interactive_checkpoint_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_CHECKPOINT;
     init(interactive_checkpoint_category, 1);
     init(interactive_checkpoint_category->elements, 1);
     interactive_checkpoint_category->elements[0].elements[0].type = STAMP_TYPE_INTERACTIVE;
     interactive_checkpoint_category->elements[0].elements[0].interactive.type = INTERACTIVE_TYPE_CHECKPOINT;

     auto* interactive_stair_category = editor->category_array.elements + EDITOR_CATEGORY_INTERACTIVE_STAIRS;
     init(interactive_stair_category, 8);
     index = 0;
     for(S8 i = 0; i < 2; i++){
          for(S8 d = 0; d < DIRECTION_COUNT; d++){
               init(interactive_stair_category->elements + index, 1);
               interactive_stair_category->elements[index].elements[0].type = STAMP_TYPE_INTERACTIVE;
               interactive_stair_category->elements[index].elements[0].interactive.type = INTERACTIVE_TYPE_STAIRS;
               interactive_stair_category->elements[index].elements[0].interactive.stairs.up = i;
               interactive_stair_category->elements[index].elements[0].interactive.stairs.face = static_cast<Direction_t>(d);
               interactive_stair_category->elements[index].elements[0].interactive.stairs.exit_index = 0;
               index++;
          }
     }

     return true;
}

void sort_selection(Editor_t* editor){
     if(editor->selection_start.x > editor->selection_end.x) SWAP(editor->selection_start.x, editor->selection_end.x);
     if(editor->selection_start.y > editor->selection_end.y) SWAP(editor->selection_start.y, editor->selection_end.y);
}

void move_selection(Editor_t* editor, Direction_t direction){
     switch(direction){
     default:
          break;
     case DIRECTION_LEFT:
          editor->selection_start.x--;
          editor->selection_end.x--;
          break;
     case DIRECTION_RIGHT:
          editor->selection_start.x++;
          editor->selection_end.x++;
          break;
     case DIRECTION_UP:
          editor->selection_start.y++;
          editor->selection_end.y++;
          break;
     case DIRECTION_DOWN:
          editor->selection_start.y--;
          editor->selection_end.y--;
          break;
     }
}

void destroy(Editor_t* editor){
     for(S16 i = 0; i < editor->category_array.count; i++){
          for(int j = 0; j < editor->category_array.elements[i].count; j++){
               destroy(editor->category_array.elements[i].elements + j);
          }
          destroy(editor->category_array.elements + i);
     }

     destroy(&editor->category_array);
     destroy(&editor->selection);
     destroy(&editor->clipboard);
}

Coord_t stamp_array_dimensions(ObjectArray_t<Stamp_t>* object_array){
     Coord_t min {0, 0};
     Coord_t max {0, 0};

     for(S32 i = 0; i < object_array->count; ++i){
          Stamp_t* stamp = object_array->elements + i;

          if(stamp->offset.x < min.x){min.x = stamp->offset.x;}
          if(stamp->offset.y < min.y){min.y = stamp->offset.y;}

          if(stamp->offset.x > max.x){max.x = stamp->offset.x;}
          if(stamp->offset.y > max.y){max.y = stamp->offset.y;}
     }

     return (max - min) + Coord_t{1, 1};
}

void apply_stamp(Stamp_t* stamp, Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>** interactive_quad_tree, bool combine){
     switch(stamp->type){
     default:
          break;
     case STAMP_TYPE_TILE_ID:
     {
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          if(tile){
               tile->id = stamp->tile.id;
               tile->rotation = stamp->tile.rotation;
               if(stamp->tile.solid){
                    tile->flags |= TILE_FLAG_SOLID;
               }
          }
     } break;
     case STAMP_TYPE_TILE_FLAGS:
     {
          Tile_t* tile = tilemap_get_tile(tilemap, coord);
          if(tile){
               if(combine){
                    tile->flags |= stamp->tile_flags;
               }else{
                    tile->flags = stamp->tile_flags;
               }
          }
     } break;
     case STAMP_TYPE_BLOCK:
     {
          Rect_t coord_rect = rect_surrounding_coord(coord);

          S8 z = 0;

          for(S16 i = 0; i < block_array->count; i++){
               auto block = block_array->elements + i;
               Rect_t block_rect = block_get_inclusive_rect(block);
               if(block->pos.z + HEIGHT_INTERVAL > z && rect_in_rect(coord_rect, block_rect)){
                    z = block->pos.z + HEIGHT_INTERVAL;
               }
          }

          int index = block_array->count;
          resize(block_array, block_array->count + (S16)(1));
          // TODO: Check if block is in the way with the quad tree

          Block_t* block = block_array->elements + index;
          default_block(block);
          block->pos = coord_to_pos(coord);
          block->pos.z = z;
          block->element = stamp->block.element;
          block->rotation = stamp->block.rotation;
          block->cut = stamp->block.cut;
     } break;
     case STAMP_TYPE_INTERACTIVE:
     {
          Interactive_t* interactive = quad_tree_interactive_find_at(*interactive_quad_tree, coord);
          if(interactive) return;

          int index = interactive_array->count;
          resize(interactive_array, interactive_array->count + (S16)(1));
          interactive_array->elements[index] = stamp->interactive;
          interactive_array->elements[index].coord = coord;
          quad_tree_free(*interactive_quad_tree);
          *interactive_quad_tree = quad_tree_build(interactive_array);
     } break;
     }
}

// editor.h
void coord_clear(Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array){
     Tile_t* tile = tilemap_get_tile(tilemap, coord);
     if(tile){
          tile->id = 0;
          tile->flags = 0;
          tile->rotation = 0;
     }

     auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
     if(interactive){
          S16 index = (S16)(interactive - interactive_array->elements);
          if(index >= 0){
               remove(interactive_array, index);
               quad_tree_free(interactive_quad_tree);
               interactive_quad_tree = quad_tree_build(interactive_array);
          }
     }

     bool found_block = false;
     do
     {
          found_block = false;

          S16 block_index = -1;
          for(S16 i = 0; i < block_array->count; i++){
               if(pos_to_coord(block_array->elements[i].pos) == coord){
                    block_index = i;
                    found_block = true;
                    break;
               }
          }

          if(block_index >= 0){
               // if the block is entangled, clear the entangle index on any blocks it was linked with
               {
                    auto* block = block_array->elements + block_index;
                    if(block->entangle_index >= 0){
                         S16 entangle_index = block->entangle_index;
                         while(entangle_index != block_index && entangle_index != -1){
                              Block_t* entangled_block = block_array->elements + entangle_index;
                              entangle_index = entangled_block->entangle_index;
                              entangled_block->entangle_index = -1;
                         }
                    }
               }
               remove(block_array, block_index);

               // a remove means the last element is put into the slot we removed, so any blocks at that index, if
               // they are entangled, tell their entangler what the new index is
               S16 previous_last_index = block_array->count;
               for(S16 i = 0; i < block_array->count; i++){
                    auto* block = block_array->elements + i;
                    if(block->entangle_index == previous_last_index){
                         block->entangle_index = block_index;
                    }
               }
          }
     }while(found_block);
}

Rect_t editor_selection_bounds(Editor_t* editor){
     Rect_t rect {};
     for(S16 i = 0; i < editor->selection.count; i++){
          auto* stamp = editor->selection.elements + i;
          if(rect.left > stamp->offset.x) rect.left = stamp->offset.x;
          if(rect.bottom > stamp->offset.y) rect.bottom = stamp->offset.y;
          if(rect.right < stamp->offset.x) rect.right = stamp->offset.x;
          if(rect.top < stamp->offset.y) rect.top = stamp->offset.y;
     }

     rect.left += editor->selection_start.x;
     rect.right += editor->selection_start.x;
     rect.top += editor->selection_start.y;
     rect.bottom += editor->selection_start.y;

     return rect;
}

S32 mouse_select_stamp_index(Coord_t screen_coord, ObjectArray_t<ObjectArray_t<Stamp_t>>* stamp_array){
     S32 index = -1;
     Rect_t current_rect = {};
     S16 row_height = 0;
     for(S16 i = 0; i < stamp_array->count; i++){
          Coord_t dimensions = stamp_array_dimensions(stamp_array->elements + i);

          if(row_height < dimensions.y) row_height = dimensions.y; // track max

          current_rect.right = current_rect.left + dimensions.x;
          current_rect.top = current_rect.bottom + dimensions.y;

          if(screen_coord.x >= current_rect.left && screen_coord.x < current_rect.right &&
             screen_coord.y >= current_rect.bottom && screen_coord.y < current_rect.top){
               index = i;
               break;
          }

          current_rect.left += dimensions.x;

          // wrap around to next row if necessary
          if(current_rect.left >= ROOM_TILE_SIZE){
               current_rect.left = 0;
               current_rect.bottom += row_height;
          }
     }
     return index;
}

