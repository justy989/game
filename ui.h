#pragma once

#include "defines.h"
#include "vec.h"
#include "tags.h"

#define CHECKBOX_DIMENSION (8.0 * PIXEL_SIZE)
#define THUMBNAIL_UI_DIMENSION (0.05f)

struct Checkbox_t{
    Vec_t pos;
    bool checked = false;

    Quad_t get_area(Vec_t scroll){
        Vec_t final = pos + scroll;
        return Quad_t{final.x, final.y, (F32)(final.x + CHECKBOX_DIMENSION), (F32)(final.y + CHECKBOX_DIMENSION)};
    }
};

struct MapThumbnail_t{
    char* map_filepath = NULL;
    bool tags[TAG_COUNT];
    Vec_t pos;
    // thumbnail

    Quad_t get_area(Vec_t scroll){
        Vec_t final = pos + scroll;
        return Quad_t{final.x, final.y, (F32)(final.x + THUMBNAIL_UI_DIMENSION), (F32)(final.y + THUMBNAIL_UI_DIMENSION)};
    }
};
