#include "exit.h"
#include "draw.h"

#include <string.h>

void write_exit(FILE* file, const Exit_t* exit){
     size_t length = strnlen((const char*)(exit->path), EXIT_MAX_PATH_SIZE);
     fwrite(&length, sizeof(length), 1, file);
     fwrite(&exit->path, length, 1, file);
     fwrite(&exit->destination_index, sizeof(exit->destination_index), 1, file);
}

void read_exit(FILE* file, Exit_t* exit){
     memset(exit->path, 0, EXIT_MAX_PATH_SIZE);
     size_t length = 0;
     fread(&length, sizeof(length), 1, file);
     fread(&exit->path, length, 1, file);
     fread(&exit->destination_index, sizeof(exit->destination_index), 1, file);
}

Quad_t exit_ui_query(ExitUIType_t type, S16 index, ObjectArray_t<Exit_t>* exits){
     const F32 text_height_spacing = TEXT_CHAR_HEIGHT + TEXT_CHAR_SPACING;
     const F32 text_width_spacing = TEXT_CHAR_WIDTH + TEXT_CHAR_SPACING;
     const F32 destination_index_padding = 3 * text_width_spacing;
     const F32 destination_control_padding = TILE_SIZE;
     const F32 left = 0.01f;
     const F32 header_y_start = 0.85f;

     if (type == EXIT_UI_HEADER) {
          // we only fill out width/height for active ui elements
          return Quad_t{left, header_y_start, 0, 0};
     }

     if (type == EXIT_UI_ENTRY_PATH || type == EXIT_UI_ENTRY_DESTINATION_INDEX) {
          F32 y = header_y_start - text_height_spacing;
          for(S16 i = 0; i < exits->count; i++){
               if(index == i){
                    if(type == EXIT_UI_ENTRY_DESTINATION_INDEX){
                         return Quad_t{left + destination_control_padding, y, 0, 0};
                    }
                    if(type == EXIT_UI_ENTRY_PATH){
                         size_t path_len = strnlen((char*)(exits->elements[i].path), EXIT_MAX_PATH_SIZE);
                         F32 final_left = left + destination_control_padding + destination_index_padding;
                         return Quad_t{final_left, y, final_left + (F32)(path_len + 1) * text_width_spacing,
                                       y + TEXT_CHAR_HEIGHT};
                    }
               }

               y -= text_height_spacing;
          }
     }

     if(type == EXIT_UI_ENTRY_INCREASE_DESTINATION_INDEX ||
        type == EXIT_UI_ENTRY_DECREASE_DESTINATION_INDEX ||
        type == EXIT_UI_ENTRY_REMOVE ||
        type == EXIT_UI_ENTRY_ADD){
          F32 y = header_y_start - text_height_spacing;

          for(S16 i = 0; i < exits->count; i++){
               if(index == i){
                    if(type == EXIT_UI_ENTRY_DECREASE_DESTINATION_INDEX){
                         return Quad_t{left, y, left + HALF_TILE_SIZE, y + HALF_TILE_SIZE};
                    }

                    if(type == EXIT_UI_ENTRY_INCREASE_DESTINATION_INDEX){
                         return Quad_t{left + HALF_TILE_SIZE, y, left + TILE_SIZE, y + HALF_TILE_SIZE};
                    }

                    if(type == EXIT_UI_ENTRY_REMOVE){
                         F32 x = (F32)(strnlen((char *)(exits->elements[i].path), EXIT_MAX_PATH_SIZE) + 1) * text_width_spacing;
                         x += destination_index_padding;
                         x += destination_control_padding;
                         return Quad_t{x, y, x + HALF_TILE_SIZE, y + HALF_TILE_SIZE};
                    }
               }

               y -= text_height_spacing;
          }

          if (type == EXIT_UI_ENTRY_ADD){
               return Quad_t{left, y, left + HALF_TILE_SIZE, y + HALF_TILE_SIZE};
          }
     }

     return Quad_t{};
}
