#pragma once

#include "interactive.h"
#include "element.h"
#include "object_array.h"
#include "tile.h"
#include "block.h"
#include "interactive.h"
#include "quad_tree.h"

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
     U8 rotation;
     U8 z;
     BlockCut_t cut;
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
     EDITOR_CATEGORY_INTERACTIVE_PORTAL,
     EDITOR_CATEGORY_INTERACTIVE_WIRE_CROSS,
     EDITOR_CATEGORY_INTERACTIVE_CLONE_KILLER,
     EDITOR_CATEGORY_INTERACTIVE_PIT,
     EDITOR_CATEGORY_COUNT,
};

struct Editor_t{
     ObjectArray_t<ObjectArray_t<ObjectArray_t<Stamp_t>>> category_array;
     EditorMode_t mode;

     S32 category;
     S32 stamp;

     ObjectArray_t<S16> entangle_indices;

     Coord_t selection_start;
     Coord_t selection_end;

     ObjectArray_t<Stamp_t> selection;
     ObjectArray_t<Stamp_t> clipboard;
};

bool init(Editor_t* editor);
void sort_selection(Editor_t* editor);
void move_selection(Editor_t* editor, Direction_t direction);
void destroy(Editor_t* editor);

Coord_t stamp_array_dimensions(ObjectArray_t<Stamp_t>* object_array);
void apply_stamp(Stamp_t* stamp, Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Block_t>* block_array, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>** interactive_quad_tree, bool combine);

void coord_clear(Coord_t coord, TileMap_t* tilemap, ObjectArray_t<Interactive_t>* interactive_array,
                 QuadTreeNode_t<Interactive_t>* interactive_quad_tree, ObjectArray_t<Block_t>* block_array);

Rect_t editor_selection_bounds(Editor_t* editor);
S32 mouse_select_stamp_index(Coord_t screen_coord, ObjectArray_t<ObjectArray_t<Stamp_t>>* stamp_array);
