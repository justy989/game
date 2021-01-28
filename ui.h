#pragma once

#include "defines.h"
#include "vec.h"
#include "quad.h"
#include "tags.h"

#ifdef __linux
    #include <GL/gl.h>
#elif __APPLE__
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl.h>
#endif

#define CHECKBOX_DIMENSION (8.0 * PIXEL_SIZE)
#define THUMBNAIL_UI_DIMENSION (0.1375f)

enum CheckBoxState_t{
     CHECKBOX_STATE_EMPTY,
     CHECKBOX_STATE_CHECKED,
     CHECKBOX_STATE_DISABLED,
};

struct Checkbox_t{
    Vec_t pos;
    CheckBoxState_t state;

    Quad_t get_area(Vec_t scroll){
        Vec_t final = pos + scroll;
        return Quad_t{final.x, final.y, (F32)(final.x + CHECKBOX_DIMENSION), (F32)(final.y + CHECKBOX_DIMENSION)};
    }
};

struct MapThumbnail_t{
    char* map_filepath = NULL;
    int map_number = 0;
    bool tags[TAG_COUNT];
    Vec_t pos;
    GLuint texture = 0;

    Quad_t get_area(Vec_t scroll){
        Vec_t final = pos + scroll;
        return Quad_t{final.x, final.y, (F32)(final.x + THUMBNAIL_UI_DIMENSION), (F32)(final.y + THUMBNAIL_UI_DIMENSION)};
    }
};
