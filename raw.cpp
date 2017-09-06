#include "raw.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Raw_t raw_load_file(const char* filename)
{
     Raw_t raw;
     memset(&raw, 0, sizeof(raw));

     FILE* file = fopen(filename, "rb");
     if(!file){
          LOG("%s() failed to load '%s'\n", __FUNCTION__, filename);
          return raw;
     }

     fseek(file, 0, SEEK_END);
     raw.byte_count = ftell(file); // returns a long, is that enough to get 64 bytes?
     fseek(file, 0, SEEK_SET);

     raw.bytes = (U8*)(malloc((size_t)(raw.byte_count)));
     if(!raw.bytes){
          fclose(file);
          return raw;
     }

     size_t items_read = fread(raw.bytes, (size_t)(raw.byte_count), 1, file);
     if(items_read != 1){
          fprintf(stderr, "%s() failed to read %lu bytes from %s\n", __FUNCTION__, raw.byte_count, filename);
          memset(&raw, 0, sizeof(raw));
          fclose(file);
          return raw;
     }

     fclose(file);
     return raw;
}
