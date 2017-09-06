#include "bitmap.h"
#include "log.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool bitmap_interpret_bytes(Bitmap_t* bitmap)
{
     bitmap->header = (BitmapFileHeader_t*)(bitmap->raw.bytes);
     bitmap->info = (BitmapInfoHeader_t*)(bitmap->raw.bytes + sizeof(BitmapFileHeader_t));

     if(bitmap->header->file_type[0] != 'B' || bitmap->header->file_type[1] != 'M'){
          LOG("%s() failed: bitmap header file_type does not match: 'BM'\n", __FUNCTION__);
          return false;
     }

     if(bitmap->info->size != BITMAP_SUPPORTED_SIZE){
          LOG("%s() failed: bitmap info size is %d instead of expected %d\n", __FUNCTION__, bitmap->info->size, BITMAP_SUPPORTED_SIZE);
          return false;
     }

     bitmap->pixels = (BitmapPixel_t*)(bitmap->raw.bytes + bitmap->header->bitmap_offset);

     // swap blue and green
     U64 pixel_count = bitmap->info->width * bitmap->info->height;
     for(U64 i = 0; i < pixel_count; ++i){
          U8 t = bitmap->pixels[i].blue;
          bitmap->pixels[i].blue = bitmap->pixels[i].red;
          bitmap->pixels[i].red = t;
     }

     return true;
}

Bitmap_t bitmap_load_raw(const U8* bytes, U64 byte_count)
{
     Bitmap_t bitmap;
     memset(&bitmap, 0, sizeof(bitmap));

     bitmap.raw.bytes = (U8*)(malloc(byte_count));
     if(!bitmap.raw.bytes){
          LOG("%s() failed: failed to allocate %lu bytes for bitmap\n", __FUNCTION__, byte_count);
     }
     bitmap.raw.byte_count = byte_count;

     for(U64 i = 0; i < byte_count; ++i){
          bitmap.raw.bytes[i] = bytes[i];
     }

     if(!bitmap_interpret_bytes(&bitmap)){
          free(bitmap.raw.bytes);
          memset(&bitmap, 0, sizeof(bitmap));
          return bitmap;
     }

     return bitmap;
}

Bitmap_t bitmap_load_from_file(const char* filepath)
{
     Bitmap_t bitmap;
     memset(&bitmap, 0, sizeof(bitmap));

     bitmap.raw = raw_load_file(filepath);
     if(!bitmap.raw.bytes) return bitmap;

     if(!bitmap_interpret_bytes(&bitmap)){
          free(bitmap.raw.bytes);
          memset(&bitmap, 0, sizeof(bitmap));
          return bitmap;
     }

     return bitmap;
}

AlphaBitmap_t bitmap_to_alpha_bitmap(const Bitmap_t* bitmap, BitmapPixel_t color_key)
{
     AlphaBitmap_t alpha_bitmap;

     if(!bitmap->pixels) return alpha_bitmap;

     S32 pixel_count = bitmap->info->width * bitmap->info->height;
     alpha_bitmap.pixels = (AlphaBitmapPixel_t*)(malloc(pixel_count * sizeof(*alpha_bitmap.pixels)));
     if(!alpha_bitmap.pixels) return alpha_bitmap;

     alpha_bitmap.width = bitmap->info->width;
     alpha_bitmap.height = bitmap->info->height;

     BitmapPixel_t* bitmap_pixel = bitmap->pixels;
     AlphaBitmapPixel_t* alpha_bitmap_pixel = alpha_bitmap.pixels;

     for(S32 i = 0; i < pixel_count; ++i){
          alpha_bitmap_pixel->red = bitmap_pixel->red;
          alpha_bitmap_pixel->green = bitmap_pixel->green;
          alpha_bitmap_pixel->blue = bitmap_pixel->blue;

          if(bitmap_pixel->red == color_key.red &&
             bitmap_pixel->green == color_key.green &&
             bitmap_pixel->blue == color_key.blue){
               alpha_bitmap_pixel->alpha = 0;
          }else{
               alpha_bitmap_pixel->alpha = 255;
          }

          bitmap_pixel++;
          alpha_bitmap_pixel++;
     }

     return alpha_bitmap;
}
