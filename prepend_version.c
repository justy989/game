// gcc prepend_version.c -o prepend_version
#include <stdio.h>
#include <stdint.h>

int main(int argc, char** argv){
     if(argc != 3){
          printf("%s [src] [dst]\n", argv[0]);
          return -1;
     }

     int32_t version = 1;

     FILE* src_file = fopen(argv[1], "rb");
     FILE* dst_file = fopen(argv[2], "wb");

     fwrite(&version, sizeof(version), 1, dst_file);

     char byte;
     while(!feof(src_file)){
          fread(&byte, sizeof(byte), 1, src_file);
          fwrite(&byte, sizeof(byte), 1, dst_file);
     }

     fclose(src_file);
     fclose(dst_file);

     return 0;
}
