#include "camera.h"
#include "conversion.h"
#include "defines.h"
#include "log.h"

Position_t Camera_t::bottom_left(){
     return pos - Vec_t{0.5f, 0.5f};
}

void Camera_t::center_on_tilemap(TileMap_t* tilemap){
     pos = pixel_to_pos(Pixel_t{(S16)(tilemap->width * TILE_SIZE_IN_PIXELS / 2),
                                (S16)(tilemap->height * TILE_SIZE_IN_PIXELS / 2)});

     // We keep the view dimensions a square so it doesn't get stretched for now
     S16 bigger_dimension = tilemap->width > tilemap->height ? tilemap->width : tilemap->height;
     F32 view_dimension = (F32)(bigger_dimension) / (F32)(ROOM_TILE_SIZE);

     view_dimensions.x = view_dimension;
     view_dimensions.y = view_dimension;

     LOG("calculating view dimension: %f, %f\n", view_dimension, view_dimension);
}
