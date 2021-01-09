#include "camera.h"
#include "conversion.h"
#include "defines.h"
#include "vec.h"

Position_t Camera_t::bottom_left(){
     return pos - Vec_t{0.5f, 0.5f};
}

void Camera_t::center_on_tilemap(TileMap_t* tilemap){
     Rect_t map_rect {0, 0, tilemap->width, tilemap->height};
     center_on_room(&map_rect);
}

void Camera_t::center_on_room(Rect_t* rect){
     S16 rect_width = rect->right - rect->left;
     S16 rect_height = rect->top - rect->bottom;
     pos = pixel_to_pos(Pixel_t{(S16)(rect_width * TILE_SIZE_IN_PIXELS / 2),
                                (S16)(rect_height * TILE_SIZE_IN_PIXELS / 2)});

     // We keep the view dimensions a square so it doesn't get stretched for now
     S16 bigger_dimension = rect_width > rect_height ? rect_width : rect_height;
     view_dimension = (F32)(bigger_dimension) / (F32)(ROOM_TILE_SIZE);

     view_pos = pos - Vec_t{0.5f, 0.5f};
     world_offset = pos_to_vec(view_pos * -1.0f);

     F32 lower_offset = world_offset.x < world_offset.y ? world_offset.x : world_offset.y;

     view.left = lower_offset;
     view.right = view.left + view_dimension;
     view.bottom = lower_offset;
     view.top = view.bottom + view_dimension;

     center_offset.x = (rect_width % 2 != 0) ? 0 : HALF_TILE_SIZE;
     center_offset.y = (rect_height % 2 != 0) ? 0 : HALF_TILE_SIZE;
}

Vec_t Camera_t::normalized_to_world(Vec_t v){
     v.x = view.left + (v.x * (view.right - view.left));
     v.y = view.bottom + (v.y * (view.top - view.bottom));
     return v;
}
