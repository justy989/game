#include "camera.h"
#include "conversion.h"
#include "defines.h"
#include "vec.h"
#include "utils.h"

static Position_t interp_position(Position_t* a, Position_t* b, F32 tx, F32 ty){
     Position_t diff_pos = *b - *a;
     Vec_t diff_vec = pos_to_vec(diff_pos);
     Vec_t result_vec {diff_vec.x * tx, diff_vec.y * ty};
     return *a + vec_to_pos(result_vec);
}

void Camera_t::center_on_tilemap(TileMap_t* tilemap){
     Rect_t map_rect {0, 0, (S16)(tilemap->width - 1), (S16)(tilemap->height - 1)};
     center_on_room(&map_rect);
}

void Camera_t::center_on_room(Rect_t* rect){
     S16 rect_width = (rect->right - rect->left) + 1;
     S16 rect_height = (rect->top - rect->bottom) + 1;

     Position_t center_pos = pixel_to_pos(Pixel_t{(S16)((rect->left * TILE_SIZE_IN_PIXELS) + (rect_width * HALF_TILE_SIZE_IN_PIXELS)),
                                                  (S16)((rect->bottom * TILE_SIZE_IN_PIXELS) + (rect_height * HALF_TILE_SIZE_IN_PIXELS))});

     // We keep the view dimensions a square so it doesn't get stretched for now
     S16 bigger_dimension = rect_width > rect_height ? rect_width : rect_height;
     F32 view_dimension = (F32)(bigger_dimension) * TILE_SIZE;

     if(bigger_dimension % 2 != 0){
          center_pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
     }

     target_view.left = 0;
     target_view.bottom = 0;
     target_view.right = view_dimension;
     target_view.top = view_dimension;

     // offset by the bottom left
     target_offset = center_pos - Pixel_t{(S16)((S16)(bigger_dimension / 2) * TILE_SIZE_IN_PIXELS),
                                   (S16)((S16)(bigger_dimension / 2) * TILE_SIZE_IN_PIXELS)};
     target_offset.pixel.x = -target_offset.pixel.x;
     target_offset.pixel.y = -target_offset.pixel.y;

     initial_view = view;
     initial_offset = offset;
}

Position_t Camera_t::normalized_to_world(Vec_t v){
     Position_t bottom_left = offset;
     bottom_left.pixel.x = -offset.pixel.x;
     bottom_left.pixel.y = -offset.pixel.y;
     bottom_left.decimal.x = -offset.decimal.x;
     bottom_left.decimal.y = -offset.decimal.y;
     canonicalize(&bottom_left);

     Vec_t screen_world_dimensions{(view.right - view.left), (view.top - view.bottom)};
     Position_t top_right = bottom_left + screen_world_dimensions;

     return interp_position(&bottom_left, &top_right, v.x, v.y);
}

void Camera_t::move_towards_target(F32 t){
     view.left = lerp(initial_view.left, target_view.left, t);
     view.bottom = lerp(initial_view.bottom, target_view.bottom, t);
     view.right = lerp(initial_view.right, target_view.right, t);
     view.top = lerp(initial_view.top, target_view.top, t);

     offset = interp_position(&initial_offset, &target_offset, t, t);
}
