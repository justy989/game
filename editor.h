#pragma once

#include "interactive.h"
#include "element.h"
#include "object_array.h"

enum StampType_t{
     STAMP_TYPE_NONE,
     STAMP_TYPE_TILE_ID,
     STAMP_TYPE_TILE_FLAGS,
     STAMP_TYPE_BLOCK,
     STAMP_TYPE_INTERACTIVE,
};

struct StampBlock_t{
     Element_t element;
     Direction_t face;
};

struct Stamp_t{
     StampType_t type;

     union{
          U8 tile_id;
          U16 tile_flags;
          StampBlock_t block;
          Interactive_t interactive;
     };

     Coord_t offset;
};

enum EditorMode_t : U8{
     EDITOR_MODE_OFF,
     EDITOR_MODE_CATEGORY_SELECT,
     EDITOR_MODE_STAMP_SELECT,
     EDITOR_MODE_STAMP_HIDE,
     EDITOR_MODE_CREATE_SELECTION,
     EDITOR_MODE_SELECTION_MANIPULATION,
};

enum EditorCategory_t : U8{
     EDITOR_CATEGORY_TILE_ID,
     EDITOR_CATEGORY_TILE_FLAGS,
     EDITOR_CATEGORY_BLOCK,
     EDITOR_CATEGORY_INTERACTIVE_LEVER,
     EDITOR_CATEGORY_INTERACTIVE_PRESSURE_PLATE,
     EDITOR_CATEGORY_INTERACTIVE_POPUP,
     EDITOR_CATEGORY_INTERACTIVE_DOOR,
     EDITOR_CATEGORY_INTERACTIVE_LIGHT_DETECTOR,
     EDITOR_CATEGORY_INTERACTIVE_ICE_DETECTOR,
     EDITOR_CATEGORY_INTERACTIVE_BOW,
     EDITOR_CATEGORY_COUNT,
};

struct Editor_t{
     ObjectArray_t<ObjectArray_t<ObjectArray_t<Stamp_t>>> category_array;
     EditorMode_t mode = EDITOR_MODE_OFF;

     S32 category = 0;
     S32 stamp = 0;

     Coord_t selection_start;
     Coord_t selection_end;

     Coord_t clipboard_start_offset;
     Coord_t clipboard_end_offset;

     ObjectArray_t<Stamp_t> selection;
     ObjectArray_t<Stamp_t> clipboard;
};

Coord_t stamp_array_dimensions(ObjectArray_t<Stamp_t>* object_array);
bool init(Editor_t* editor);
void destroy(Editor_t* editor);
