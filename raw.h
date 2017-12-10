#ifndef RAW_H
#define RAW_H

#include "types.h"

struct Raw_t{
     U8* bytes;
     U64 byte_count;
};

Raw_t raw_load_file(const char* filename);

#endif
