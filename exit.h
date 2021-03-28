#pragma once

#include "types.h"
#include "quad.h"
#include "object_array.h"

#include <stdio.h>

#define EXIT_MAX_PATH_SIZE 64

enum ExitUIType_t : U8{
     EXIT_UI_HEADER,
     EXIT_UI_ENTRY_INCREASE_DESTINATION_INDEX,
     EXIT_UI_ENTRY_DECREASE_DESTINATION_INDEX,
     EXIT_UI_ENTRY_DESTINATION_INDEX,
     EXIT_UI_ENTRY_PATH,
     EXIT_UI_ENTRY_REMOVE,
     EXIT_UI_ENTRY_ADD,
};

struct Exit_t{
     U8 path[EXIT_MAX_PATH_SIZE];
     U8 destination_index = 0;
};

void write_exit(FILE* file, const Exit_t* exit);
void read_exit(FILE* file, Exit_t* exit);

Quad_t exit_ui_query(ExitUIType_t type, S16 index, ObjectArray_t<Exit_t>* exits);
