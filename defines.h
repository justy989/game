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
#define DOUBLE_TILE_SIZE_IN_PIXELS (S16)(32)
#define TILE_SIZE_IN_PIXELS (S16)(16)
#define HALF_TILE_SIZE_IN_PIXELS (S16)(8)

#define BASE_LIGHT (U8)(128)
#define LIGHT_DECAY (U8)(32)

#define ROOM_TILE_SIZE (S16)(17)
#define ROOM_PIXEL_SIZE (ROOM_TILE_SIZE * TILE_SIZE_IN_PIXELS)

#define HEIGHT_INTERVAL (S8)(6)

#define FALL_TIME 0.03f

#define BLOCK_PUSH_TIME 0.25f

#define LIGHT_DETECTOR_THRESHOLD 176

#define RESET_TIME 2.0f

#define UNDO_MEMORY (4 * 1024 * 1024)

#define BLOCK_QUAD_TREE_MAX_QUERY 16

#define LIGHT_MAX_LINE_LEN 8

#define MASS_DRAG 0.625f

#define BLOCK_SOLID_SIZE_IN_PIXELS (TILE_SIZE_IN_PIXELS - (S16)(1))

#define HALF_TILE_SIZE_PIXEL Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS}

#define PLAYER_ACCEL_DISTANCE (HALF_TILE_SIZE / 4.0)
#define PLAYER_ACCEL_TIME (0.075f)
#define PLAYER_ACCEL ((2.0f * PLAYER_ACCEL_DISTANCE) / (PLAYER_ACCEL_TIME * PLAYER_ACCEL_TIME))
