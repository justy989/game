#pragma once

#include "types.h"
#include "raw.h"

// save in gimp as 24 bit bitmap with compatibility options turned off
#define BITMAP_SUPPORTED_SIZE 108

#pragma pack(push, 1)
struct BitmapFileHeader_t{
     S8 file_type [2];
     U32 file_size;
     U16 reserved_1;
     U16 reserved_2;
     U32 bitmap_offset;
};

struct BitmapInfoHeader_t{
     U32 size;
     S32 width;
     S32 height;
     U16 planes;
     U16 bit_count;
     U32 compression;
     U32 size_image;
     S32 x_pels_per_meter;
     S32 y_pels_per_meter;
     U32 clr_used;
     U32 clr_important;
};

struct BitmapPixel_t{
     U8 red;
     U8 green;
     U8 blue;
};

struct AlphaBitmapPixel_t{
     U8 red;
     U8 green;
     U8 blue;
     U8 alpha;
};
#pragma pack(pop)

struct Bitmap_t{
     BitmapFileHeader_t* header = nullptr;
     BitmapInfoHeader_t* info = nullptr;
     BitmapPixel_t* pixels = nullptr;
     Raw_t raw;
};

// save bitmap in gimp as 24-bit and turn off compatibility options
Bitmap_t bitmap_load_raw(const U8* bytes, U64 byte_count);
Bitmap_t bitmap_load_from_file(const char* filepath);

struct AlphaBitmap_t{
     AlphaBitmapPixel_t* pixels = nullptr;
     S32 width = 0;
     S32 height = 0;
};

AlphaBitmap_t bitmap_to_alpha_bitmap(const Bitmap_t* bitmap, BitmapPixel_t color_key);
