#pragma once

#include "types.h"

struct Raw_t{
     U8* bytes;
     U64 byte_count;
};

Raw_t raw_load_file(const char* filename);
bool raw_save_file(Raw_t* raw, const char* filename);
