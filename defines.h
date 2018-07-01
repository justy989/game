#pragma once

#define SWAP(a, b) {auto c = a; a = b; b = c;}

#define MAXIMUM(a, b)((a > b) ? (a) : (b))
#define MINIMUM(a, b)((a < b) ? (a) : (b))

#define CLAMP(var, minimum, maximum) \
     if(var < minimum){              \
          var = minimum;             \
     }else if(var > maximum){        \
          var = maximum;             \
     }

// evil-genius quality right herr
#define CASE_ENUM_RET_STR(e) case e: return #e;

#define ELEM_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))

#define ON_BIT(value, bit) value |= (1 << bit)
#define OFF_BIT(value, bit) value &= ~(1 << bit)
#define ON_BIT_FLAG(value, bit_flag) value |= bit_flag
#define OFF_BIT_FLAG(value, bit_flag) value &= ~bit_flag

#define TOGGLE_BIT(value, bit){if(value & (1 << bit)){OFF_BIT(value, bit);}else{ON_BIT(value, bit);}}
#define TOGGLE_BIT_FLAG(value, bit_flag){if(value & bit_flag){OFF_BIT_FLAG(value, bit_flag);}else{ON_BIT_FLAG(value, bit_flag);}}

#define PIXEL_SIZE .00367647f
#define TILE_SIZE (16.0f / 272.0f)
#define HALF_TILE_SIZE (TILE_SIZE * 0.5f)
#define TILE_SIZE_IN_PIXELS 16
#define HALF_TILE_SIZE_IN_PIXELS 8

#define BASE_LIGHT 128
#define LIGHT_DECAY 32

#define ROOM_TILE_SIZE 17
#define ROOM_PIXEL_SIZE (ROOM_TILE_SIZE * TILE_SIZE_IN_PIXELS)

#define HEIGHT_INTERVAL 6

#define FALL_TIME 0.03f

#define BLOCK_PUSH_TIME 0.25f

#define LIGHT_DETECTOR_THRESHOLD 176

#define RESET_TIME 2.0f

#define UNDO_MEMORY (4 * 1024 * 1024)

#define BLOCK_QUAD_TREE_MAX_QUERY 16
#define MAX_TELEPORTED_DEBUG_BLOCK_COUNT 4
#define MAX_CONNECTED_PORTALS 8

#define LIGHT_MAX_LINE_LEN 8
