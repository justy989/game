#ifndef DEFINES_H
#define DEFINES_H

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

#define TOGGLE_BIT(value, bit){if(value & (1 << bit)){OFF_BIT(value, bit);}else{ON_BIT(value, bit);}}

#define LOG_MISMATCH(name, fmt_spec, chk, act)\
     {                                                                                                                     \
          char mismatch_fmt_string[128];                                                                                   \
          snprintf(mismatch_fmt_string, 128, "mismatched '%s' value. demo '%s', actual '%s'\n", name, fmt_spec, fmt_spec); \
          LOG(mismatch_fmt_string, chk, act);                                                                              \
          test_passed = false;                                                                                             \
     }

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
#define RESET_TIME 1.5f

#endif
