#include <iostream>
#include <chrono>
#include <cstdint>
#include <cfloat>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define MAXIMUM(a, b)((a > b) ? (a) : (b))
#define MINIMUM(a, b)((a < b) ? (a) : (b))

typedef int8_t  S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef float  F32;
typedef double F64;

struct Vec_t{
     F32 x;
     F32 y;
};

Vec_t operator+(Vec_t a, Vec_t b){return Vec_t{a.x + b.x, a.y + b.y};}
Vec_t operator-(Vec_t a, Vec_t b){return Vec_t{a.x - b.x, a.y - b.y};}

void operator+=(Vec_t& a, Vec_t b){a.x += b.x; a.y += b.y;}
void operator-=(Vec_t& a, Vec_t b){a.x -= b.x; a.y -= b.y;}

Vec_t operator*(Vec_t a, F32 s){return Vec_t{a.x * s, a.y * s};}
void operator*=(Vec_t& a, F32 s){a.x *= s; a.y *= s;}

float vec_dot(Vec_t a, Vec_t b){return a.x * b.x + a.y * b.y;}

Vec_t vec_negate(Vec_t a){return Vec_t{-a.x, -a.y};}
F32 vec_magnitude(Vec_t v){return (F32)(sqrt((v.x * v.x) + (v.y * v.y)));}

Vec_t vec_normalize(Vec_t a)
{
     F32 length = vec_magnitude(a);
     if(length <= FLT_EPSILON) return a;
     return Vec_t{a.x / length, a.y / length};
}

Vec_t vec_zero()  {return Vec_t{ 0.0f,  0.0f};}
Vec_t vec_left()  {return Vec_t{-1.0f,  0.0f};}
Vec_t vec_right() {return Vec_t{ 1.0f,  0.0f};}
Vec_t vec_up()    {return Vec_t{ 0.0f,  1.0f};}
Vec_t vec_down()  {return Vec_t{ 0.0f, -1.0f};}

void vec_set   (Vec_t* v, float x, float y)   {v->x = x; v->y = y;}
void vec_move  (Vec_t* v, float dx, float dy) {v->x += dx; v->y += dy;}
void vec_move_x(Vec_t* v, float dx)           {v->x += dx;}
void vec_move_y(Vec_t* v, float dy)           {v->y += dy;}

Vec_t vec_project_onto(Vec_t a, Vec_t b){
     // find the perpendicular vector
     Vec_t b_normal = vec_normalize(b);
     F32 along_b = vec_dot(a, b_normal);

     // clamp dot
     F32 b_magnitude = vec_magnitude(b);
     if(along_b < 0.0f){
          along_b = 0.0f;
     }else if(along_b > b_magnitude){
          along_b = b_magnitude;
     }

     // find the closest point
     return b_normal * along_b;
}

enum TileFlag_t{
     TILE_FLAG_ICED = 1,
     TILE_FLAG_CHECKPOINT = 2,
};

struct Tile_t{
     U8 id;
     U8 light;
     U8 flags;
};

struct TileMap_t{
     S16 width;
     S16 height;
     Tile_t** tiles;
};

bool init(TileMap_t* tilemap, S16 width, S16 height){
     tilemap->tiles = (Tile_t**)calloc(height, sizeof(*tilemap->tiles));
     if(!tilemap->tiles) return false;

     for(S16 i = 0; i < height; i++){
          tilemap->tiles[i] = (Tile_t*)calloc(width, sizeof(*tilemap->tiles[i]));
          if(!tilemap->tiles[i]) return false;
     }

     tilemap->width = width;
     tilemap->height = height;

     return true;
}

void destroy(TileMap_t* tilemap){
     for(S16 i = 0; i < tilemap->height; i++){
          free(tilemap->tiles[i]);
     }

     free(tilemap->tiles);
     memset(tilemap, 0, sizeof(*tilemap));
}

bool collide_with_wall(float x, float y, float dx, float dy, F32 wall, F32 lower_bound, F32 upper_bound, F32* time_min)
{
     // x = pos.x + dx * t
     // (x - pos.x) / dx = t
     // when y is in range of the tile
     if(fabs(dx) > FLT_EPSILON){
          F32 time_of_collision = (wall - x) / dx;

          if(time_of_collision > 0.0f && time_of_collision < *time_min){
               F32 y_collision = y + dy * time_of_collision;

               if(y_collision >= lower_bound && y_collision <= upper_bound){
                    *time_min = MAXIMUM(0.0f, time_of_collision - FLT_EPSILON);
                    return true;
               }
          }
     }

     return false;
}

using namespace std::chrono;

int main(){
     if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0){
          return 1;
     }

     SDL_DisplayMode display_mode;

     if(SDL_GetCurrentDisplayMode(0, &display_mode) != 0){
          return 1;
     }

     int window_width = 1280;
     int window_height = 1280;

     SDL_Window* window = SDL_CreateWindow("bryte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL);
     if(!window) return 1;

     SDL_GLContext opengl_context = SDL_GL_CreateContext(window);

     SDL_GL_SetSwapInterval(1);
     glViewport(0, 0, window_width, window_height);
     glClearColor(0.0, 0.0, 0.0, 1.0);
     glEnable(GL_TEXTURE_2D);
     glViewport(0, 0, window_width, window_height);
     glMatrixMode(GL_PROJECTION);
     glLoadIdentity();
     glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
     glMatrixMode(GL_MODELVIEW);
     glLoadIdentity();
     glEnable(GL_BLEND);
     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
     glBlendEquation(GL_FUNC_ADD);

     TileMap_t tilemap;
     {
          init(&tilemap, 16, 18);

          for(U16 i = 0; i < tilemap.width; i++){
               tilemap.tiles[0][i].id = 1;
               tilemap.tiles[tilemap.height - 1][i].id = 1;
          }

          for(U16 i = 0; i < tilemap.height; i++){
               tilemap.tiles[i][0].id = 1;
               tilemap.tiles[i][tilemap.width - 1].id = 1;
          }
     }

     bool quit = false;

     Vec_t user_movement = {};

     Vec_t pos {0.3f, 0.3f};
     Vec_t vel = vec_zero();
     Vec_t accel = vec_zero();

     Vec_t line_0 {0.6f, 0.5f};
     Vec_t line_1 {0.5f, 0.7f};

     bool left_pressed = false;
     bool right_pressed = false;
     bool up_pressed = false;
     bool down_pressed = false;

     auto last_time = system_clock::now();
     auto current_time = last_time;

     F32 player_radius = 8.0f / 256.0f;
     F32 block_size = 16.0f / 256.0f;
     F32 half_block_size = block_size * 0.5f;
     bool collision = false;

     while(!quit){
          current_time = system_clock::now();
          duration<double> elapsed_seconds = current_time - last_time;
          F64 dt = (F64)(elapsed_seconds.count());

          if(dt < 0.0166666f) continue;

          SDL_Event sdl_event;
          while(SDL_PollEvent(&sdl_event)){
               switch(sdl_event.type){
               default:
                    break;
               case SDL_KEYDOWN:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         left_pressed = true;
                         break;
                    case SDL_SCANCODE_RIGHT:
                         right_pressed = true;
                         break;
                    case SDL_SCANCODE_UP:
                         up_pressed = true;
                         break;
                    case SDL_SCANCODE_DOWN:
                         down_pressed = true;
                         break;
                    }
                    break;
               case SDL_KEYUP:
                    switch(sdl_event.key.keysym.scancode){
                    default:
                         break;
                    case SDL_SCANCODE_ESCAPE:
                         quit = true;
                         break;
                    case SDL_SCANCODE_LEFT:
                         left_pressed = false;
                         break;
                    case SDL_SCANCODE_RIGHT:
                         right_pressed = false;
                         break;
                    case SDL_SCANCODE_UP:
                         up_pressed = false;
                         break;
                    case SDL_SCANCODE_DOWN:
                         down_pressed = false;
                         break;
                    }
                    break;
               }
          }

          user_movement = vec_zero();

          if(left_pressed) user_movement += Vec_t{-1, 0};
          if(right_pressed) user_movement += Vec_t{1, 0};
          if(up_pressed) user_movement += Vec_t{0, 1};
          if(down_pressed) user_movement += Vec_t{0, -1};

          last_time = current_time;

          // player movement
          {
               float movement_speed = 12.5f;
               float drag = 0.6f;

               user_movement = vec_normalize(user_movement);
               accel = user_movement * movement_speed;

               Vec_t pos_delta = (accel * dt * dt * 0.5f) + (vel * dt);
               vel += accel * dt;
               vel *= drag;

               if(fabs(vec_magnitude(vel)) > movement_speed){
                    vel = vec_normalize(vel) * movement_speed;
               }

               // collision with line
               {
                    // move data we care about to the origin
                    Vec_t p = pos - line_0;
                    Vec_t v = line_1 - line_0;
                    Vec_t v_closest = vec_project_onto(p, v);

                    F32 distance = vec_magnitude(v_closest - p);
                    if(distance < player_radius){
                         // find edge of circle in that direction
                         Vec_t col = v_closest - p;
                         Vec_t edge = vec_normalize(col) * player_radius;

                         // remove diff from pos_delta
                         Vec_t diff = (edge - col);
                         pos_delta -= diff;

                         collision = true;
                    }else{
                         collision = false;
                    }
               }

               pos += pos_delta;
          }

          glClear(GL_COLOR_BUFFER_BIT);

          // player circle
          glBegin(GL_TRIANGLE_FAN);
          if(collision){
               glColor3f(0.0f, 0.0f, 1.0f);
          }else{
               glColor3f(1.0f, 1.0f, 1.0f);
          }

          glVertex2f(pos.x, pos.y);
          glVertex2f(pos.x + player_radius, pos.y);
          S32 segments = 32;
          F32 delta = 3.14159f * 2.0f / (F32)(segments);
          F32 angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * player_radius;
               F32 dy = sin(angle) * player_radius;

               glVertex2f(pos.x + dx, pos.y + dy);

               angle += delta;
          }
          glVertex2f(pos.x + player_radius, pos.y);
          glEnd();

          // tilemap
          glBegin(GL_QUADS);
          glColor3f(0.0f, 1.0f, 1.0f);
          for(U16 y = 0; y < tilemap.height; y++){
               for(U16 x = 0; x < tilemap.width; x++){
                    if(tilemap.tiles[y][x].id){
                         Vec_t tile_pos {(F32)(x) / (F32)(tilemap.width), (F32)(y) / (F32)(tilemap.height)};
                         glVertex2f(-half_block_size + tile_pos.x, -half_block_size + tile_pos.y);
                         glVertex2f(-half_block_size + tile_pos.x,  half_block_size + tile_pos.y);
                         glVertex2f( half_block_size + tile_pos.x,  half_block_size + tile_pos.y);
                         glVertex2f( half_block_size + tile_pos.x, -half_block_size + tile_pos.y);
                    }
               }
          }
          glEnd();

          // collision line
          glBegin(GL_LINES);
          glColor3f(1.0f, 0.0f, 0.0f);
          glVertex2f(line_0.x, line_0.y);
          glVertex2f(line_1.x, line_1.y);
          glEnd();

          SDL_GL_SwapWindow(window);
     }

     destroy(&tilemap);

     SDL_GL_DeleteContext(opengl_context);
     SDL_DestroyWindow(window);
     SDL_Quit();

     return 0;
}
