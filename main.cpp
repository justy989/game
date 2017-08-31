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

     bool quit = false;

     Vec_t user_movement = {};

     Vec_t pos = vec_zero();
     Vec_t vel = vec_zero();
     Vec_t accel = vec_zero();

     Vec_t block_pos {0.5f, 0.5f};

     bool left_pressed = false;
     bool right_pressed = false;
     bool up_pressed = false;
     bool down_pressed = false;

     auto last_time = system_clock::now();
     auto current_time = last_time;

     float player_width = 16.0f / 256.0f;
     float player_height = player_width;
     float half_player_width = player_width * 0.5f;
     float half_player_height = player_height * 0.5f;

     float half_block_width = half_player_width;
     float half_block_height = half_player_width;

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

               if(fabs(vec_magnitude(pos - block_pos)) < 2.0f * (half_player_height + half_block_height)){
                    for(int c = 0; c < 4; ++c){
                         // player block collision
                         float collision_dt = 1.0f;
                         float left = block_pos.x - (half_block_width + half_player_width);
                         float right = block_pos.x + (half_block_width + half_player_width);
                         float bottom = block_pos.y - (half_block_height + half_player_height);
                         float top = block_pos.y + (half_block_height + half_player_height);
                         Vec_t wall_normal = vec_zero();

                         if(collide_with_wall(pos.x, pos.y, pos_delta.x, pos_delta.y, left, bottom, top, &collision_dt)){
                              wall_normal = vec_left();
                         }

                         if(collide_with_wall(pos.x, pos.y, pos_delta.x, pos_delta.y, right, bottom, top, &collision_dt)){
                              wall_normal = vec_right();
                         }

                         if(collide_with_wall(pos.y, pos.x, pos_delta.y, pos_delta.x, bottom, left, right, &collision_dt)){
                              wall_normal = vec_down();
                         }

                         if(collide_with_wall(pos.y, pos.x, pos_delta.y, pos_delta.x, top, left, right, &collision_dt)){
                              wall_normal = vec_up();
                         }

                         vel -= (wall_normal * vec_dot(vel, wall_normal));
                         pos_delta -= (wall_normal * vec_dot(pos_delta, wall_normal));
                    }
               }

               pos += pos_delta;
          }

          glClear(GL_COLOR_BUFFER_BIT);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);
          glVertex2f(-half_player_width + pos.x, -half_player_height + pos.y);
          glVertex2f(-half_player_width + pos.x,  half_player_height + pos.y);
          glVertex2f( half_player_width + pos.x,  half_player_height + pos.y);
          glVertex2f( half_player_width + pos.x, -half_player_height + pos.y);
          glEnd();

          glBegin(GL_QUADS);
          glColor3f(0.0f, 1.0f, 1.0f);
          glVertex2f(-half_block_width + block_pos.x, -half_block_height + block_pos.y);
          glVertex2f(-half_block_width + block_pos.x,  half_block_height + block_pos.y);
          glVertex2f( half_block_width + block_pos.x,  half_block_height + block_pos.y);
          glVertex2f( half_block_width + block_pos.x, -half_block_height + block_pos.y);
          glEnd();

          SDL_GL_SwapWindow(window);
     }

     SDL_GL_DeleteContext(opengl_context);
     SDL_DestroyWindow(window);
     SDL_Quit();

     return 0;
}
