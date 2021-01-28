#include "thumbnail.h"
#include "bitmap.h"
#include "draw.h"

Raw_t create_thumbnail_bitmap(){
    Raw_t raw;

    U32 pixel_size = THUMBNAIL_DIMENSION * THUMBNAIL_DIMENSION * 3;
    raw.byte_count = sizeof(BitmapFileHeader_t) + sizeof(BitmapInfoHeader_t) + pixel_size;
    U32 pixel_offset = sizeof(BitmapFileHeader_t) + sizeof(BitmapInfoHeader_t);

    raw.bytes = (U8*)malloc(raw.byte_count);
    if(!raw.bytes) return raw;

    BitmapFileHeader_t* file_header = (BitmapFileHeader_t*)(raw.bytes);
    BitmapInfoHeader_t* info_header = (BitmapInfoHeader_t*)(raw.bytes + sizeof(BitmapFileHeader_t));

    file_header->file_type[0] = 'B';
    file_header->file_type[1] = 'M';
    file_header->file_size = pixel_size + pixel_offset;
    file_header->bitmap_offset = pixel_offset;

    info_header->size = BITMAP_SUPPORTED_SIZE;
    info_header->width = THUMBNAIL_DIMENSION;
    info_header->height = THUMBNAIL_DIMENSION;
    info_header->planes = 1;
    info_header->bit_count = 24;
    info_header->compression = 0;
    info_header->size_image = pixel_size;
    info_header->x_pels_per_meter = 2835;
    info_header->y_pels_per_meter = 2835;
    info_header->clr_used = 0;
    info_header->clr_important = 0;

    BitmapPixel_t* pixel = (BitmapPixel_t*)(raw.bytes + pixel_offset);

    for(U32 y = 0; y < THUMBNAIL_DIMENSION; y++){
        for(U32 x = 0; x < THUMBNAIL_DIMENSION; x++){
            glReadPixels(x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);

            // swap red and blue
            U8 tmp = pixel->red;
            pixel->red = pixel->blue;
            pixel->blue = tmp;

            pixel++;
        }
    }

    return raw;
}

int map_thumbnail_comparor(const void* a, const void* b){
     MapThumbnail_t* thumbnail_a = (MapThumbnail_t*)a;
     MapThumbnail_t* thumbnail_b = (MapThumbnail_t*)b;

     return thumbnail_a->map_number > thumbnail_b->map_number;
}

S16 filter_thumbnails(ObjectArray_t<Checkbox_t>* tag_checkboxes, ObjectArray_t<MapThumbnail_t>* map_thumbnails){
     const F32 thumbnail_row_start_x = CHECKBOX_THUMBNAIL_SPLIT;
     F32 current_thumbnail_x = thumbnail_row_start_x;
     F32 current_thumbnail_y = TEXT_CHAR_HEIGHT + PIXEL_SIZE;

     bool none_checked = true;
     for(S16 c = 0; c < tag_checkboxes->count; c++){
          auto* checkbox = tag_checkboxes->elements + c + 1;
          if(checkbox->state == CHECKBOX_STATE_CHECKED){
               none_checked = false;
               break;
          }
     }

     if(none_checked){
          for(S16 m = 0; m < map_thumbnails->count; m++){
               auto* map_thumbnail = map_thumbnails->elements + m;
               map_thumbnail->pos.x = current_thumbnail_x;
               map_thumbnail->pos.y = current_thumbnail_y;

               current_thumbnail_x += THUMBNAIL_UI_DIMENSION;
               if((m + 1) % THUMBNAILS_PER_ROW == 0){
                    current_thumbnail_x = thumbnail_row_start_x;
                    current_thumbnail_y += THUMBNAIL_UI_DIMENSION;
               }
          }

          // account for integer division truncation
          return map_thumbnails->count + THUMBNAILS_PER_ROW;
     }

     bool exclusive = tag_checkboxes->elements[0].state == CHECKBOX_STATE_CHECKED;

     S16 match_index = 1;
     for(S16 m = 0; m < map_thumbnails->count; m++){
          auto* map_thumbnail = map_thumbnails->elements + m;

          bool matches = false;

          if(exclusive){
               matches = true;
               for(S16 c = 0; c < TAG_COUNT; c++){
                    auto* checkbox = tag_checkboxes->elements + c + 1;
                    if(checkbox->state == CHECKBOX_STATE_CHECKED){
                         if(!map_thumbnail->tags[c]){
                              matches = false;
                         }
                    }else if(checkbox->state == CHECKBOX_STATE_DISABLED){
                         if(map_thumbnail->tags[c]){
                              matches = false;
                              break;
                         }
                    }
               }
          }else{
               for(S16 c = 0; c < TAG_COUNT; c++){
                    auto* checkbox = tag_checkboxes->elements + c + 1;
                    if(map_thumbnail->tags[c]){
                         if(checkbox->state == CHECKBOX_STATE_CHECKED){
                              matches = true;
                         }else if(checkbox->state == CHECKBOX_STATE_DISABLED){
                              // disabled overrides any matches
                              matches = false;
                              break;
                         }
                    }
               }
          }

          if(matches){
               map_thumbnail->pos.x = current_thumbnail_x;
               map_thumbnail->pos.y = current_thumbnail_y;

               current_thumbnail_x += THUMBNAIL_UI_DIMENSION;
               if(match_index % THUMBNAILS_PER_ROW == 0){
                    current_thumbnail_x = thumbnail_row_start_x;
                    current_thumbnail_y += THUMBNAIL_UI_DIMENSION;
               }
               match_index++;
          }else{
               map_thumbnail->pos.x = -THUMBNAIL_UI_DIMENSION;
               map_thumbnail->pos.y = -THUMBNAIL_UI_DIMENSION;
          }
     }

     return ((match_index - 1) + THUMBNAILS_PER_ROW);
}

