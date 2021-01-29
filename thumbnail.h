#pragma once

#include "object_array.h"
#include "raw.h"
#include "ui.h"

#define THUMBNAIL_DIMENSION 128

#define CHECKBOX_THUMBNAIL_SPLIT 0.45f
#define THUMBNAILS_PER_ROW 4
#define CHECKBOX_TAG_START_INDEX 2

Raw_t create_thumbnail_bitmap();
int map_thumbnail_comparor(const void* a, const void* b);
S16 filter_thumbnails(ObjectArray_t<Checkbox_t>* tag_checkboxes, ObjectArray_t<MapThumbnail_t>* map_thumbnails);
