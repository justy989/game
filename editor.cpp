#include "editor.h"
#include "defines.h"
#include "tile.h"

#include <string.h>

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

bool init(Editor_t* editor){
     memset(editor, 0, sizeof(*editor));
     init(&editor->category_array, EDITOR_CATEGORY_COUNT);

     auto* tile_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_ID;
     init(tile_category, 26);
     for(S16 i = 0; i < 14; i++){
          init(&tile_category->elements[i], 1);
          tile_category->elements[i].elements[0].type = STAMP_TYPE_TILE_ID;
          tile_category->elements[i].elements[0].tile_id = (U8)(i);
     }

     auto* tile_id_array = tile_category->elements + 13;

     tile_id_array++;
     init(tile_id_array, 2);
     tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[0].tile_id = 32;
     tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[1].tile_id = 16;
     tile_id_array->elements[1].offset = Coord_t{0, 1};

     tile_id_array++;
     init(tile_id_array, 2);
     tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[0].tile_id = 33;
     tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[1].tile_id = 17;
     tile_id_array->elements[1].offset = Coord_t{0, 1};

     tile_id_array++;
     init(tile_id_array, 2);
     tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[0].tile_id = 18;
     tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[1].tile_id = 19;
     tile_id_array->elements[1].offset = Coord_t{1, 0};

     tile_id_array++;
     init(tile_id_array, 2);
     tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[0].tile_id = 34;
     tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
     tile_id_array->elements[1].tile_id = 35;
     tile_id_array->elements[1].offset = Coord_t{1, 0};

     for(S16 i = 0; i < 6; i++){
          tile_id_array++;
          init(tile_id_array, 4);
          tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[0].tile_id = 36 + (i * 2);
          tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[1].tile_id = 37 + (i * 2);
          tile_id_array->elements[1].offset = Coord_t{1, 0};
          tile_id_array->elements[2].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[2].tile_id = 20 + (i * 2);
          tile_id_array->elements[2].offset = Coord_t{0, 1};
          tile_id_array->elements[3].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[3].tile_id = 21 + (i * 2);
          tile_id_array->elements[3].offset = Coord_t{1, 1};
     }

     for(S16 i = 0; i < 2; i++){
          tile_id_array++;
          init(tile_id_array, 4);
          tile_id_array->elements[0].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[0].tile_id = 50 + i * 4;
          tile_id_array->elements[1].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[1].tile_id = 51 + i * 4;
          tile_id_array->elements[1].offset = Coord_t{1, 0};
          tile_id_array->elements[2].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[2].tile_id = 48 + i * 4;
          tile_id_array->elements[2].offset = Coord_t{0, 1};
          tile_id_array->elements[3].type = STAMP_TYPE_TILE_ID;
          tile_id_array->elements[3].tile_id = 49 + i * 4;
          tile_id_array->elements[3].offset = Coord_t{1, 1};
     }

     auto* tile_flags_category = editor->category_array.elements + EDITOR_CATEGORY_TILE_FLAGS;
     init(tile_flags_category, 62);
     for(S8 i = 0; i < 2; i++){
          S8 index_offset = i * 15;
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
     }

     for(S8 i = 0; i < 15; i++){
          tile_flags_category->elements[15 + i].elements[0].tile_flags |= TILE_FLAG_WIRE_STATE;
     }

     for(S8 i = 0; i < DIRECTION_COUNT; i++){
          S8 index_offset = 30 + (i * 7);
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_LEFT;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_MID;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
          init(tile_flags_category->elements + index_offset, 1);
          tile_flags_category->elements[index_offset].elements[0].type = STAMP_TYPE_TILE_FLAGS;
          tile_flags_category->elements[index_offset].elements[0].tile_flags = TILE_FLAG_WIRE_CLUSTER_RIGHT;
          tile_flags_set_cluster_direction(&tile_flags_category->elements[index_offset].elements[0].tile_flags, (Direction_t)(i));
          index_offset++;
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

     init(tile_flags_category->elements + 59, 1);
     tile_flags_category->elements[59].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[59].elements[0].tile_flags = TILE_FLAG_ICED;
     init(tile_flags_category->elements + 60, 1);
     tile_flags_category->elements[60].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[60].elements[0].tile_flags = TILE_FLAG_CHECKPOINT;
     init(tile_flags_category->elements + 61, 1);
     tile_flags_category->elements[61].elements[0].type = STAMP_TYPE_TILE_FLAGS;
     tile_flags_category->elements[61].elements[0].tile_flags = TILE_FLAG_RESET_IMMUNE;

     auto* block_category = editor->category_array.elements + EDITOR_CATEGORY_BLOCK;
     init(block_category, 4);
     for(S16 i = 0; i < block_category->count; i++){
          init(block_category->elements + i, 1);
          block_category->elements[i].elements[0].type = STAMP_TYPE_BLOCK;
          block_category->elements[i].elements[0].block.face = DIRECTION_LEFT;
          block_category->elements[i].elements[0].block.element = (Element_t)(i);
     }

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
     init(interactive_popup_category, 2);
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

     return true;
}

void destroy(Editor_t* editor){
     for(S16 i = 0; i < editor->category_array.count; i++){
          destroy(editor->category_array.elements + i);
     }

     destroy(&editor->category_array);
     destroy(&editor->selection);
     destroy(&editor->clipboard);
}
