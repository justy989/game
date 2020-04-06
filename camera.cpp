#include "camera.h"
#include "conversion.h"
#include "defines.h"
#include "vec.h"

Position_t Camera_t::bottom_left(){
     return pos - Vec_t{0.5f, 0.5f};
}

void Camera_t::center_on_tilemap(TileMap_t* tilemap){
     pos = pixel_to_pos(Pixel_t{(S16)(tilemap->width * TILE_SIZE_IN_PIXELS / 2),
                                (S16)(tilemap->height * TILE_SIZE_IN_PIXELS / 2)});

     // We keep the view dimensions a square so it doesn't get stretched for now
     S16 bigger_dimension = tilemap->width > tilemap->height ? tilemap->width : tilemap->height;
     view_dimension = (F32)(bigger_dimension) / (F32)(ROOM_TILE_SIZE);

     view_pos = pos - Vec_t{0.5f, 0.5f};
     world_offset = pos_to_vec(view_pos * -1.0f);

     F32 lower_offset = world_offset.x < world_offset.y ? world_offset.x : world_offset.y;

     view.left = lower_offset;
     view.right = view.left + view_dimension;
     view.bottom = lower_offset;
     view.top = view.bottom + view_dimension;

     center_offset.x = (tilemap->width % 2 != 0) ? 0 : HALF_TILE_SIZE;
     center_offset.y = (tilemap->height % 2 != 0) ? 0 : HALF_TILE_SIZE;
}

Vec_t Camera_t::normalized_to_world(Vec_t v){
     v.x = view.left + (v.x * (view.right - view.left));
     v.y = view.bottom + (v.y * (view.top - view.bottom));
     return v;
}
