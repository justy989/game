/*
http://www.simonstalenhag.se/
-Grant Sanderson 3blue1brown
-Shane Hendrixson
*/

#include <iostream>
#include <chrono>
#include <cfloat>
#include <cassert>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "log.h"
#include "defines.h"
#include "direction.h"
#include "bitmap.h"
#include "conversion.h"
#include "tile.h"
#include "object_array.h"
#include "interactive.h"
#include "quad_tree.h"
#include "portal_exit.h"
#include "player.h"
#include "block.h"
#include "arrow.h"
#include "undo.h"
#include "editor.h"
#include "utils.h"
#include "map_format.h"
#include "draw.h"
#include "block_utils.h"
#include "demo.h"
#include "collision.h"
#include "world.h"

// #define BLOCKS_SQUISH_PLAYER

// new quakelive bot settings
// bot_thinktime
// challenge mode

FILE* load_demo_number(S32 map_number, const char** demo_filepath){
     char filepath[64] = {};
     snprintf(filepath, 64, "content/%03d.bd", map_number);
     *demo_filepath = strdup(filepath);
     return fopen(*demo_filepath, "rb");
}

using namespace std::chrono;

int main(int argc, char** argv){
     DemoMode_t demo_mode = DEMO_MODE_NONE;
     const char* demo_filepath = nullptr;
     const char* load_map_filepath = nullptr;
     bool test = false;
     bool suite = false;
     bool show_suite = false;
     S16 map_number = 0;
     S16 first_map_number = 0;
     S16 map_count = 0;

     for(int i = 1; i < argc; i++){
          if(strcmp(argv[i], "-play") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_PLAY;
          }else if(strcmp(argv[i], "-record") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo_filepath = argv[next];
               demo_mode = DEMO_MODE_RECORD;
          }else if(strcmp(argv[i], "-load") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               load_map_filepath = argv[next];
          }else if(strcmp(argv[i], "-test") == 0){
               test = true;
          }else if(strcmp(argv[i], "-suite") == 0){
               test = true;
               suite = true;
          }else if(strcmp(argv[i], "-show") == 0){
               show_suite = true;
          }else if(strcmp(argv[i], "-map") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               map_number = atoi(argv[next]);
               first_map_number = map_number;
          }else if(strcmp(argv[i], "-count") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               map_count = atoi(argv[next]);
          }
     }

     const char* log_path = "bryte.log";
     if(!Log_t::create(log_path)){
          fprintf(stderr, "failed to create log file: '%s'\n", log_path);
          return -1;
     }

     if(test && !load_map_filepath && !suite){
          LOG("cannot test without specifying a map to load\n");
          return 1;
     }

     int window_width = 1080;
     int window_height = 1080;
     SDL_Window* window = nullptr;
     SDL_GLContext opengl_context = 0;
     GLuint theme_texture = 0;
     GLuint player_texture = 0;
     GLuint arrow_texture = 0;

     if(!suite || show_suite){
          if(SDL_Init(SDL_INIT_EVERYTHING) != 0){
               return 1;
          }

          SDL_DisplayMode display_mode;
          if(SDL_GetCurrentDisplayMode(0, &display_mode) != 0){
               return 1;
          }

          LOG("Create window: %d, %d\n", window_width, window_height);
          window = SDL_CreateWindow("bryte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL);
          if(!window) return 1;

          opengl_context = SDL_GL_CreateContext(window);

          SDL_GL_SetSwapInterval(SDL_TRUE);
          glViewport(0, 0, window_width, window_height);
          glClearColor(0.0, 0.0, 0.0, 1.0);
          glEnable(GL_TEXTURE_2D);
          glViewport(0, 0, window_width, window_height);
          glMatrixMode(GL_PROJECTION);
          glLoadIdentity();
          glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);
          glMatrixMode(GL_MODELVIEW);
          glLoadIdentity();
          glEnable(GL_BLEND);
          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          glBlendEquation(GL_FUNC_ADD);

          theme_texture = transparent_texture_from_file("content/theme.bmp");
          if(theme_texture == 0) return 1;
          player_texture = transparent_texture_from_file("content/player.bmp");
          if(player_texture == 0) return 1;
          arrow_texture = transparent_texture_from_file("content/arrow.bmp");
          if(arrow_texture == 0) return 1;
     }

     FILE* demo_file = nullptr;
     DemoEntry_t demo_entry {};
     switch(demo_mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          demo_file = fopen(demo_filepath, "w");
          if(!demo_file){
               LOG("failed to open demo file: %s\n", demo_filepath);
               return 1;
          }
          // TODO: write header
          break;
     case DEMO_MODE_PLAY:
          demo_file = fopen(demo_filepath, "r");
          if(!demo_file){
               LOG("failed to open demo file: %s\n", demo_filepath);
               return 1;
          }
          LOG("playing demo %s\n", demo_filepath);
          // TODO: read header
          demo_entry_get(&demo_entry, demo_file);
          break;
     }

     TileMap_t tilemap = {};
     ObjectArray_t<Block_t> block_array = {};
     ObjectArray_t<Interactive_t> interactive_array = {};
     ArrowArray_t arrow_array = {};
     Coord_t player_start {2, 8};

     if(load_map_filepath){
          if(!load_map(load_map_filepath, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }
     }else if(suite){
          if(!load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }

          demo_mode = DEMO_MODE_PLAY;
          demo_file = load_demo_number(map_number, &demo_filepath);
          if(!demo_file){
               LOG("missing map %d corresponding demo.\n", map_number);
               return 1;
          }
          LOG("testing demo %s\n", demo_filepath);
          demo_entry_get(&demo_entry, demo_file);
     }else if(map_number){
          if(!load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
               return 1;
          }
     }else{
          init(&tilemap, ROOM_TILE_SIZE, ROOM_TILE_SIZE);

          for(S16 i = 0; i < tilemap.width; i++){
               tilemap.tiles[0][i].id = 33;
               tilemap.tiles[1][i].id = 17;
               tilemap.tiles[tilemap.height - 1][i].id = 16;
               tilemap.tiles[tilemap.height - 2][i].id = 32;
          }

          for(S16 i = 0; i < tilemap.height; i++){
               tilemap.tiles[i][0].id = 18;
               tilemap.tiles[i][1].id = 19;
               tilemap.tiles[i][tilemap.width - 2].id = 34;
               tilemap.tiles[i][tilemap.height - 1].id = 35;
          }

          tilemap.tiles[0][0].id = 36;
          tilemap.tiles[0][1].id = 37;
          tilemap.tiles[1][0].id = 20;
          tilemap.tiles[1][1].id = 21;

          tilemap.tiles[16][0].id = 22;
          tilemap.tiles[16][1].id = 23;
          tilemap.tiles[15][0].id = 38;
          tilemap.tiles[15][1].id = 39;

          tilemap.tiles[15][15].id = 40;
          tilemap.tiles[15][16].id = 41;
          tilemap.tiles[16][15].id = 24;
          tilemap.tiles[16][16].id = 25;

          tilemap.tiles[0][15].id = 42;
          tilemap.tiles[0][16].id = 43;
          tilemap.tiles[1][15].id = 26;
          tilemap.tiles[1][16].id = 27;
          if(!init(&interactive_array, 1)){
               return 1;
          }
          interactive_array.elements[0].coord.x = -1;
          interactive_array.elements[0].coord.y = -1;

          if(!init(&block_array, 1)){
               return 1;
          }
          block_array.elements[0].pos = coord_to_pos(Coord_t{-1, -1});
     }

     Player_t player;
     QuadTreeNode_t<Interactive_t>* interactive_quad_tree = nullptr;
     QuadTreeNode_t<Block_t>* block_quad_tree = nullptr;

     reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);

     Undo_t undo = {};
     init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);

     undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);

     bool resetting = false;
     F32 reset_timer = 1.0f;

     bool quit = false;

     Vec_t user_movement = {};
     PlayerAction_t player_action {};

     auto last_time = system_clock::now();
     auto current_time = last_time;

     Position_t camera = coord_to_pos(Coord_t{8, 8});

     Block_t* last_block_pushed = nullptr;
     Direction_t last_block_pushed_direction = DIRECTION_LEFT;
     Block_t* block_to_push = nullptr;

     // bool left_click_down = false;
     Vec_t mouse_screen = {}; // 0.0f to 1.0f
     Position_t mouse_world = {};
     bool ctrl_down;

     Editor_t editor;
     init(&editor);

     S64 frame_count = 0;
     F32 dt = 0.0f;

     while(!quit){
          if(!suite || show_suite){
               current_time = system_clock::now();
               duration<double> elapsed_seconds = current_time - last_time;
               dt = (F64)(elapsed_seconds.count());
               if(dt < 0.0166666f) continue; // limit 60 fps
          }

          // TODO: consider 30fps as minimum for random noobs computers
          // if(demo_mode) dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.
          dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.

          quad_tree_free(block_quad_tree);
          block_quad_tree = quad_tree_build(&block_array);

          frame_count++;

          last_time = current_time;

          last_block_pushed = block_to_push;
          block_to_push = nullptr;

          player_action.last_activate = player_action.activate;
          player_action.reface = false;

          if(demo_mode == DEMO_MODE_PLAY){
               bool end_of_demo = false;
               if(demo_entry.player_action_type == PLAYER_ACTION_TYPE_END_DEMO){
                    end_of_demo = (frame_count == demo_entry.frame);
               }else{
                    while(frame_count == demo_entry.frame && !feof(demo_file)){
                         player_action_perform(&player_action, &player, demo_entry.player_action_type, demo_mode,
                                               demo_file, frame_count);
                         demo_entry_get(&demo_entry, demo_file);
                    }
               }

               if(end_of_demo){
                    if(test){
                         TileMap_t check_tilemap = {};
                         ObjectArray_t<Block_t> check_block_array = {};
                         ObjectArray_t<Interactive_t> check_interactive_array = {};
                         Coord_t check_player_start;
                         Pixel_t check_player_pixel;
                         if(!load_map_from_file(demo_file, &check_player_start, &check_tilemap, &check_block_array, &check_interactive_array, demo_filepath)){
                              LOG("failed to load map state from end of file\n");
                              demo_mode = DEMO_MODE_NONE;
                         }else{
                              bool test_passed = true;

#define LOG_MISMATCH(name, fmt_spec, chk, act)\
     {                                                                                                                     \
          char mismatch_fmt_string[128];                                                                                   \
          snprintf(mismatch_fmt_string, 128, "mismatched '%s' value. demo '%s', actual '%s'\n", name, fmt_spec, fmt_spec); \
          LOG(mismatch_fmt_string, chk, act);                                                                              \
          test_passed = false;                                                                                             \
     }

                              fread(&check_player_pixel, sizeof(check_player_pixel), 1, demo_file);
                              if(check_tilemap.width != tilemap.width){
                                   LOG_MISMATCH("tilemap width", "%d", check_tilemap.width, tilemap.width);
                              }else if(check_tilemap.height != tilemap.height){
                                   LOG_MISMATCH("tilemap height", "%d", check_tilemap.height, tilemap.height);
                              }else{
                                   for(S16 j = 0; j < check_tilemap.height; j++){
                                        for(S16 i = 0; i < check_tilemap.width; i++){
                                             if(check_tilemap.tiles[j][i].flags != tilemap.tiles[j][i].flags){
                                                  char name[64];
                                                  snprintf(name, 64, "tile %d, %d flags", i, j);
                                                  LOG_MISMATCH(name, "%d", check_tilemap.tiles[j][i].flags, tilemap.tiles[j][i].flags);
                                             }
                                        }
                                   }
                              }

                              if(check_player_pixel.x != player.pos.pixel.x){
                                   LOG_MISMATCH("player pixel x", "%d", check_player_pixel.x, player.pos.pixel.x);
                              }

                              if(check_player_pixel.y != player.pos.pixel.y){
                                   LOG_MISMATCH("player pixel y", "%d", check_player_pixel.y, player.pos.pixel.y);
                              }

                              if(check_block_array.count != block_array.count){
                                   LOG_MISMATCH("block count", "%d", check_block_array.count, block_array.count);
                              }else{
                                   for(S16 i = 0; i < check_block_array.count; i++){
                                        // TODO: consider checking other things
                                        Block_t* check_block = check_block_array.elements + i;
                                        Block_t* block = block_array.elements + i;
                                        if(check_block->pos.pixel.x != block->pos.pixel.x){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos x", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.pixel.x, block->pos.pixel.x);
                                        }

                                        if(check_block->pos.pixel.y != block->pos.pixel.y){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos y", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.pixel.y, block->pos.pixel.y);
                                        }

                                        if(check_block->pos.z != block->pos.z){
                                             char name[64];
                                             snprintf(name, 64, "block %d pos z", i);
                                             LOG_MISMATCH(name, "%d", check_block->pos.z, block->pos.z);
                                        }

                                        if(check_block->element != block->element){
                                             char name[64];
                                             snprintf(name, 64, "block %d element", i);
                                             LOG_MISMATCH(name, "%d", check_block->element, block->element);
                                        }
                                   }
                              }

                              if(check_interactive_array.count != interactive_array.count){
                                   LOG_MISMATCH("interactive count", "%d", check_interactive_array.count, interactive_array.count);
                              }else{
                                   for(S16 i = 0; i < check_interactive_array.count; i++){
                                        // TODO: consider checking other things
                                        Interactive_t* check_interactive = check_interactive_array.elements + i;
                                        Interactive_t* interactive = interactive_array.elements + i;

                                        if(check_interactive->type != interactive->type){
                                             LOG_MISMATCH("interactive type", "%d", check_interactive->type, interactive->type);
                                        }else{
                                             switch(check_interactive->type){
                                             default:
                                                  break;
                                             case INTERACTIVE_TYPE_PRESSURE_PLATE:
                                                  if(check_interactive->pressure_plate.down != interactive->pressure_plate.down){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d pressure plate down",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->pressure_plate.down,
                                                                    interactive->pressure_plate.down);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_ICE_DETECTOR:
                                             case INTERACTIVE_TYPE_LIGHT_DETECTOR:
                                                  if(check_interactive->detector.on != interactive->detector.on){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d detector on",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->detector.on,
                                                                    interactive->detector.on);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_POPUP:
                                                  if(check_interactive->popup.iced != interactive->popup.iced){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d popup iced",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->popup.iced,
                                                                    interactive->popup.iced);
                                                  }
                                                  if(check_interactive->popup.lift.up != interactive->popup.lift.up){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d popup lift up",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->popup.lift.up,
                                                                    interactive->popup.lift.up);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_DOOR:
                                                  if(check_interactive->door.lift.up != interactive->door.lift.up){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d door lift up",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->door.lift.up,
                                                                    interactive->door.lift.up);
                                                  }
                                                  break;
                                             case INTERACTIVE_TYPE_PORTAL:
                                                  if(check_interactive->portal.on != interactive->portal.on){
                                                       char name[64];
                                                       snprintf(name, 64, "interactive at %d, %d portal on",
                                                                interactive->coord.x, interactive->coord.y);
                                                       LOG_MISMATCH(name, "%d", check_interactive->portal.on,
                                                                    interactive->portal.on);
                                                  }
                                                  break;
                                             }
                                        }
                                   }
                              }

                              if(!test_passed){
                                   LOG("test failed\n");
                                   demo_mode = DEMO_MODE_NONE;
                                   if(suite && !show_suite) return 1;
                              }else if(suite){
                                   map_number++;
                                   S16 maps_tested = map_number - first_map_number;
                                   if(map_count > 0 && maps_tested >= map_count){
                                        LOG("Done Testing %d maps.\n", map_count);
                                        return 0;
                                   }
                                   if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                                        reset_map(&player, player_start, &interactive_array, &interactive_quad_tree, &arrow_array);
                                        destroy(&undo);
                                        init(&undo, UNDO_MEMORY, tilemap.width, tilemap.height, block_array.count, interactive_array.count);
                                        undo_snapshot(&undo, &player, &tilemap, &block_array, &interactive_array);

                                        // reset some vars
                                        player_action = {};
                                        last_block_pushed = nullptr;
                                        last_block_pushed_direction = DIRECTION_LEFT;
                                        block_to_push = nullptr;

                                        fclose(demo_file);
                                        demo_file = load_demo_number(map_number, &demo_filepath);
                                        if(demo_file){
                                             LOG("testing demo %s\n", demo_filepath);
                                             demo_entry_get(&demo_entry, demo_file);
                                             frame_count = 0;
                                             continue; // reset to the top of the loop
                                        }else{
                                             LOG("missing map %d corresponding demo.\n", map_number);
                                             return 1;
                                        }
                                   }else{
                                        LOG("Done Testing %d maps.\n", maps_tested);
                                        return 0;
                                   }
                              }else{
                                   LOG("test passed\n");
                              }
                         }
                    }else{
                         demo_mode = DEMO_MODE_NONE;
                         LOG("end of demo %s\n", demo_filepath);
                    }
               }
          }

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
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x--;
                              editor.selection_end.x--;
                         }else if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x++;
                              editor.selection_end.x++;
                         }else if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_UP:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y++;
                              editor.selection_end.y++;
                         }else if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y--;
                              editor.selection_end.y--;
                         }else if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_E:
                         if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_SPACE:
                         if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_SHOOT_START, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_L:
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              setup_map(&player, player_start, &interactive_array, &interactive_quad_tree, &block_array,
                                        &block_quad_tree, &undo, &tilemap, &arrow_array);
                         }
                         break;
                    case SDL_SCANCODE_LEFTBRACKET:
                         map_number--;
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              setup_map(&player, player_start, &interactive_array, &interactive_quad_tree, &block_array,
                                        &block_quad_tree, &undo, &tilemap, &arrow_array);
                         }else{
                              map_number++;
                         }
                         break;
                    case SDL_SCANCODE_RIGHTBRACKET:
                         map_number++;
                         if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                              setup_map(&player, player_start, &interactive_array, &interactive_quad_tree, &block_array,
                                        &block_quad_tree, &undo, &tilemap, &arrow_array);
                         }else{
                              map_number--;
                         }
                         break;
                    case SDL_SCANCODE_V:
                    {
                         char filepath[64];
                         snprintf(filepath, 64, "content/%03d.bm", map_number);
                         save_map(filepath, player_start, &tilemap, &block_array, &interactive_array);
                    } break;
                    case SDL_SCANCODE_U:
                         if(!resetting){
                              player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_UNDO, demo_mode,
                                                    demo_file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_N:
                    {
                         Tile_t* tile = tilemap_get_tile(&tilemap, mouse_select_world(mouse_screen, camera));
                         if(tile){
                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_LEFT_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_LEFT_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_MID_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_MID_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT){
                                   if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT_ON){
                                        tile->flags &= ~TILE_FLAG_WIRE_CLUSTER_RIGHT_ON;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_CLUSTER_RIGHT_ON;
                                   }
                              }

                              if(tile->flags & TILE_FLAG_WIRE_LEFT ||
                                 tile->flags & TILE_FLAG_WIRE_UP ||
                                 tile->flags & TILE_FLAG_WIRE_RIGHT ||
                                 tile->flags & TILE_FLAG_WIRE_DOWN){
                                   if(tile->flags & TILE_FLAG_WIRE_STATE){
                                        tile->flags &= ~TILE_FLAG_WIRE_STATE;
                                   }else{
                                        tile->flags |= TILE_FLAG_WIRE_STATE;
                                   }
                              }
                         }
                    } break;
                    // TODO: #ifdef DEBUG
                    case SDL_SCANCODE_GRAVE:
                         if(editor.mode == EDITOR_MODE_OFF){
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }else{
                              editor.mode = EDITOR_MODE_OFF;
                              editor.selection_start = {};
                              editor.selection_end = {};
                         }
                         break;
                    case SDL_SCANCODE_TAB:
                         if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                              editor.mode = EDITOR_MODE_STAMP_HIDE;
                         }else{
                              editor.mode = EDITOR_MODE_STAMP_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_RETURN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);

                              // clear coords below stamp
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }

                              for(int i = 0; i < editor.selection.count; i++){
                                   Coord_t coord = editor.selection_start + editor.selection.elements[i].offset;
                                   apply_stamp(editor.selection.elements + i, coord,
                                               &tilemap, &block_array, &interactive_array, &interactive_quad_tree, ctrl_down);
                              }

                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_T:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              sort_selection(&editor);

                              S16 height_offset = (editor.selection_end.y - editor.selection_start.y) - 1;

                              // perform rotation on each offset
                              for(S16 i = 0; i < editor.selection.count; i++){
                                   auto* stamp = editor.selection.elements + i;
                                   Coord_t rot {stamp->offset.y, (S16)(-stamp->offset.x + height_offset)};
                                   stamp->offset = rot;
                              }
                         }
                         break;
                    case SDL_SCANCODE_X:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              destroy(&editor.clipboard);
                              shallow_copy(&editor.selection, &editor.clipboard);
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_P:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT && editor.clipboard.count){
                              destroy(&editor.selection);
                              shallow_copy(&editor.clipboard, &editor.selection);
                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         }
                         break;
                    case SDL_SCANCODE_M:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              player_start = mouse_select_world(mouse_screen, camera);
                         }else if(editor.mode == EDITOR_MODE_OFF){
                              resetting = true;
                         }
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = true;
                         break;
                    case SDL_SCANCODE_B:
#if 0
                    {
                         undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                         Coord_t min = pos_to_coord(player.pos) - Coord_t{1, 1};
                         Coord_t max = pos_to_coord(player.pos) + Coord_t{1, 1};
                         for(S16 y = min.y; y <= max.y; y++){
                              for(S16 x = min.x; x <= max.x; x++){
                                   Coord_t coord {x, y};
                                   coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                              }
                         }
                    } break;
#endif
                    case SDL_SCANCODE_5:
                         player.pos.pixel = mouse_select_world_pixel(mouse_screen, camera) + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
                         player.pos.decimal.x = 0;
                         player.pos.decimal.y = 0;
                         break;
                    case SDL_SCANCODE_H:
                    {
                         Coord_t coord = mouse_select_world(mouse_screen, camera);
                         describe_coord(coord, &tilemap, interactive_quad_tree, block_quad_tree);
                    } break;
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
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_LEFT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_UP_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_MOVE_DOWN_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_ACTIVATE_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_SPACE:
                         player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_SHOOT_STOP, demo_mode,
                                               demo_file, frame_count);
                         break;
                    case SDL_SCANCODE_LCTRL:
                         ctrl_down = false;
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONDOWN:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = true;

                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_OFF:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              Coord_t mouse_coord = vec_to_coord(mouse_screen);
                              S32 select_index = (mouse_coord.y * ROOM_TILE_SIZE) + mouse_coord.x;
                              if(select_index < EDITOR_CATEGORY_COUNT){
                                   editor.mode = EDITOR_MODE_STAMP_SELECT;
                                   editor.category = select_index;
                                   editor.stamp = 0;
                              }else{
                                   editor.mode = EDITOR_MODE_CREATE_SELECTION;
                                   editor.selection_start = mouse_select_world(mouse_screen, camera);
                                   editor.selection_end = editor.selection_start;
                              }
                         } break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              S32 select_index = mouse_select_stamp_index(vec_to_coord(mouse_screen),
                                                                          editor.category_array.elements + editor.category);
                              if(editor.mode != EDITOR_MODE_STAMP_HIDE && select_index < editor.category_array.elements[editor.category].count && select_index >= 0){
                                   editor.stamp = select_index;
                              }else{
                                   undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                                   Coord_t select_coord = mouse_select_world(mouse_screen, camera);
                                   auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
                                   for(S16 s = 0; s < stamp_array->count; s++){
                                        auto* stamp = stamp_array->elements + s;
                                        apply_stamp(stamp, select_coord + stamp->offset,
                                                    &tilemap, &block_array, &interactive_array, &interactive_quad_tree, ctrl_down);
                                   }

                                   quad_tree_free(block_quad_tree);
                                   block_quad_tree = quad_tree_build(&block_array);
                              }
                         } break;
                         }
                         break;
                    case SDL_BUTTON_RIGHT:
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              coord_clear(mouse_select_world(mouse_screen, camera), &tilemap, &interactive_array,
                                          interactive_quad_tree, &block_array);
                              break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              Coord_t start = mouse_select_world(mouse_screen, camera);
                              Coord_t end = start + stamp_array_dimensions(editor.category_array.elements[editor.category].elements + editor.stamp);
                              for(S16 j = start.y; j < end.y; j++){
                                   for(S16 i = start.x; i < end.x; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }
                         } break;
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &tilemap, &interactive_array, interactive_quad_tree, &block_array);
                                   }
                              }
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEBUTTONUP:
                    switch(sdl_event.button.button){
                    default:
                         break;
                    case SDL_BUTTON_LEFT:
                         // left_click_down = false;
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CREATE_SELECTION:
                         {
                              editor.selection_end = mouse_select_world(mouse_screen, camera);

                              sort_selection(&editor);

                              destroy(&editor.selection);

                              S16 stamp_count = (((editor.selection_end.x - editor.selection_start.x) + 1) * ((editor.selection_end.y - editor.selection_start.y) + 1)) * 2;
                              init(&editor.selection, stamp_count);
                              S16 stamp_index = 0;
                              for(S16 j = editor.selection_start.y; j <= editor.selection_end.y; j++){
                                   for(S16 i = editor.selection_start.x; i <= editor.selection_end.x; i++){
                                        Coord_t coord = {i, j};
                                        Coord_t offset = coord - editor.selection_start;

                                        // tile id
                                        Tile_t* tile = tilemap_get_tile(&tilemap, coord);
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_ID;
                                        editor.selection.elements[stamp_index].tile_id = tile->id;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // tile flags
                                        editor.selection.elements[stamp_index].type = STAMP_TYPE_TILE_FLAGS;
                                        editor.selection.elements[stamp_index].tile_flags = tile->flags;
                                        editor.selection.elements[stamp_index].offset = offset;
                                        stamp_index++;

                                        // interactive
                                        auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
                                        if(interactive){
                                             resize(&editor.selection, editor.selection.count + 1);
                                             auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                             stamp->type = STAMP_TYPE_INTERACTIVE;
                                             stamp->interactive = *interactive;
                                             stamp->offset = offset;
                                        }

                                        for(S16 b = 0; b < block_array.count; b++){
                                             auto* block = block_array.elements + b;
                                             if(pos_to_coord(block->pos) == coord){
                                                  resize(&editor.selection, editor.selection.count + 1);
                                                  auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                                  stamp->type = STAMP_TYPE_BLOCK;
                                                  stamp->block.face = block->face;
                                                  stamp->block.element = block->element;
                                                  stamp->offset = offset;
                                             }
                                        }
                                   }
                              }

                              editor.mode = EDITOR_MODE_SELECTION_MANIPULATION;
                         } break;
                         }
                         break;
                    }
                    break;
               case SDL_MOUSEMOTION:
                    mouse_screen = Vec_t{((F32)(sdl_event.button.x) / (F32)(window_width)),
                                         1.0f - ((F32)(sdl_event.button.y) / (F32)(window_height))};
                    mouse_world = vec_to_pos(mouse_screen);
                    switch(editor.mode){
                    default:
                         break;
                    case EDITOR_MODE_CREATE_SELECTION:
                         if(editor.selection_start.x >= 0 && editor.selection_start.y >= 0){
                              editor.selection_end = pos_to_coord(mouse_world);
                         }
                         break;
                    }
                    break;
               }
          }

          // reset base light
          for(S16 j = 0; j < tilemap.height; j++){
               for(S16 i = 0; i < tilemap.width; i++){
                    tilemap.tiles[j][i].light = BASE_LIGHT;
               }
          }

          // update interactives
          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               if(interactive->type == INTERACTIVE_TYPE_POPUP){
                    lift_update(&interactive->popup.lift, POPUP_TICK_DELAY, dt, 1, HEIGHT_INTERVAL + 1);
               }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                    lift_update(&interactive->door.lift, POPUP_TICK_DELAY, dt, 0, DOOR_MAX_HEIGHT);
               }else if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                    bool should_be_down = false;
                    Coord_t player_coord = pos_to_coord(player.pos);
                    if(interactive->coord == player_coord){
                         should_be_down = true;
                    }else{
                         Tile_t* tile = tilemap_get_tile(&tilemap, interactive->coord);
                         if(tile){
                              if(!tile_is_iced(tile)){
                                   Pixel_t center = coord_to_pixel(interactive->coord) + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
                                   Rect_t rect = {};
                                   rect.left = center.x - (2 * TILE_SIZE_IN_PIXELS);
                                   rect.right = center.x + (2 * TILE_SIZE_IN_PIXELS);
                                   rect.bottom = center.y - (2 * TILE_SIZE_IN_PIXELS);
                                   rect.top = center.y + (2 * TILE_SIZE_IN_PIXELS);

                                   S16 block_count = 0;
                                   Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                                   quad_tree_find_in(block_quad_tree, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                   for(S16 b = 0; b < block_count; b++){
                                        Coord_t block_coord = block_get_coord(blocks[b]);
                                        if(interactive->coord == block_coord){
                                             should_be_down = true;
                                             break;
                                        }
                                   }
                              }
                         }
                    }

                    if(should_be_down != interactive->pressure_plate.down){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->pressure_plate.down = should_be_down;
                    }
               }
          }

          // update arrows
          for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
               Arrow_t* arrow = arrow_array.arrows + i;
               if(!arrow->alive) continue;

               Coord_t pre_move_coord = pixel_to_coord(arrow->pos.pixel);

               if(arrow->element == ELEMENT_FIRE){
                    illuminate(pre_move_coord, 255 - LIGHT_DECAY, &tilemap, block_quad_tree);
               }

               if(arrow->stuck_time > 0.0f){
                    arrow->stuck_time += dt;

                    switch(arrow->stuck_type){
                    default:
                         break;
                    case STUCK_BLOCK:
                         arrow->pos = arrow->stuck_block->pos + arrow->stuck_offset;
                         break;
                    }

                    if(arrow->stuck_time > ARROW_DISINTEGRATE_DELAY){
                         arrow->alive = false;
                    }
                    continue;
               }

               F32 arrow_friction = 0.9999f;

               if(arrow->pos.z > 0){
                    // TODO: fall based on the timer !
                    arrow->fall_time += dt;
                    if(arrow->fall_time > ARROW_FALL_DELAY){
                         arrow->fall_time -= ARROW_FALL_DELAY;
                         arrow->pos.z--;
                    }
               }else{
                    arrow_friction = 0.9f;
               }

               Vec_t direction = {};
               switch(arrow->face){
               default:
                    break;
               case DIRECTION_LEFT:
                    direction.x = -1;
                    break;
               case DIRECTION_RIGHT:
                    direction.x = 1;
                    break;
               case DIRECTION_DOWN:
                    direction.y = -1;
                    break;
               case DIRECTION_UP:
                    direction.y = 1;
                    break;
               }

               arrow->pos += (direction * dt * arrow->vel);
               arrow->vel *= arrow_friction;
               Coord_t post_move_coord = pixel_to_coord(arrow->pos.pixel);

               Rect_t coord_rect {(S16)(arrow->pos.pixel.x - TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.y - TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.x + TILE_SIZE_IN_PIXELS),
                                  (S16)(arrow->pos.pixel.y + TILE_SIZE_IN_PIXELS)};

               S16 block_count = 0;
               Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
               quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
               for(S16 b = 0; b < block_count; b++){
                    // blocks on the coordinate and on the ground block light
                    Rect_t block_rect = block_get_rect(blocks[b]);
                    S16 block_index = blocks[b] - block_array.elements;
                    S16 block_bottom = blocks[b]->pos.z;
                    S16 block_top = block_bottom + HEIGHT_INTERVAL;
                    if(pixel_in_rect(arrow->pos.pixel, block_rect) && arrow->element_from_block != block_index){
                         if(arrow->pos.z >= block_bottom && arrow->pos.z <= block_top){
                              arrow->stuck_time = dt;
                              arrow->stuck_offset = arrow->pos - blocks[b]->pos;
                              arrow->stuck_type = STUCK_BLOCK;
                              arrow->stuck_block = blocks[b];
                         }else if(arrow->pos.z > block_top && arrow->pos.z < (block_top + HEIGHT_INTERVAL)){
                              arrow->element_from_block = block_index;
                              if(arrow->element != blocks[b]->element){
                                   Element_t arrow_element = arrow->element;
                                   arrow->element = transition_element(arrow->element, blocks[b]->element);
                                   if(arrow_element){
                                        blocks[b]->element = transition_element(blocks[b]->element, arrow_element);
                                   }
                              }
                         }
                         break;
                    }
               }

               if(block_count == 0){
                    arrow->element_from_block = -1;
               }

               Coord_t skip_coord[DIRECTION_COUNT];
               find_portal_adjacents_to_skip_collision_check(pre_move_coord, interactive_quad_tree, skip_coord);

               if(pre_move_coord != post_move_coord){
                    bool skip = false;
                    for(S8 d = 0; d < DIRECTION_COUNT; d++){
                         if(post_move_coord == skip_coord[d]){
                              skip = true;
                         }
                    }

                    if(!skip){
                         Tile_t* tile = tilemap_get_tile(&tilemap, post_move_coord);
                         if(tile_is_solid(tile)){
                              arrow->stuck_time = dt;
                         }
                    }

                    // catch or give elements
                    if(arrow->element == ELEMENT_FIRE){
                         melt_ice(post_move_coord, 0, &tilemap, interactive_quad_tree, block_quad_tree, false);
                    }else if(arrow->element == ELEMENT_ICE){
                         spread_ice(post_move_coord, 0, &tilemap, interactive_quad_tree, block_quad_tree, false);
                    }

                    Interactive_t* interactive = quad_tree_interactive_find_at(interactive_quad_tree, post_move_coord);
                    if(interactive){
                         if(interactive->type == INTERACTIVE_TYPE_LEVER){
                              if(arrow->pos.z >= HEIGHT_INTERVAL){
                                   activate(&tilemap, interactive_quad_tree, post_move_coord);
                              }else{
                                   arrow->stuck_time = dt;
                              }
                         }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                              if(interactive->door.lift.ticks < arrow->pos.z){
                                   arrow->stuck_time = dt;
                                   // TODO: stuck in door
                              }
                         }else if(interactive->type == INTERACTIVE_TYPE_POPUP){
                              if(interactive->popup.lift.ticks > arrow->pos.z){
                                   LOG("arrow z: %d, popup lift: %d\n", arrow->pos.z, interactive->popup.lift.ticks);
                                   arrow->stuck_time = dt;
                                   // TODO: stuck in popup
                              }
                         }else if(interactive->type == INTERACTIVE_TYPE_PORTAL){
                              if(!interactive->portal.on){
                                   arrow->stuck_time = dt;
                                   // TODO: arrow drops if portal turns on
                              }else if(!portal_has_destination(post_move_coord, &tilemap, interactive_quad_tree)){
                                   // TODO: arrow drops if portal turns on
                                   arrow->stuck_time = dt;
                              }
                         }
                    }

                    S8 rotations_between = teleport_position_across_portal(&arrow->pos, NULL, interactive_quad_tree, &tilemap, pre_move_coord,
                                                                           post_move_coord);
                    if(rotations_between >= 0){
                         arrow->face = direction_rotate_clockwise(arrow->face, rotations_between);
                    }
               }
          }

          // update player
          user_movement = vec_zero();

          if(player_action.move_left){
               Direction_t direction = DIRECTION_LEFT;
               direction = direction_rotate_clockwise(direction, player_action.move_left_rotation);
               user_movement += direction_to_vec(direction);
               if(player_action.reface) player.face = direction;
          }

          if(player_action.move_right){
               Direction_t direction = DIRECTION_RIGHT;
               direction = direction_rotate_clockwise(direction, player_action.move_right_rotation);
               user_movement += direction_to_vec(direction);
               if(player_action.reface) player.face = direction;
          }

          if(player_action.move_up){
               Direction_t direction = DIRECTION_UP;
               direction = direction_rotate_clockwise(direction, player_action.move_up_rotation);
               user_movement += direction_to_vec(direction);
               if(player_action.reface) player.face = direction;
          }

          if(player_action.move_down){
               Direction_t direction = DIRECTION_DOWN;
               direction = direction_rotate_clockwise(direction, player_action.move_down_rotation);
               user_movement += direction_to_vec(direction);
               if(player_action.reface) player.face = direction;
          }

          if(player_action.activate && !player_action.last_activate){
               undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
               activate(&tilemap, interactive_quad_tree, pos_to_coord(player.pos) + player.face);
          }

          if(player_action.undo){
               undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
               undo_revert(&undo, &player, &tilemap, &block_array, &interactive_array);
               quad_tree_free(interactive_quad_tree);
               interactive_quad_tree = quad_tree_build(&interactive_array);
               quad_tree_free(block_quad_tree);
               block_quad_tree = quad_tree_build(&block_array);
               player_action.undo = false;
          }

          if(player.has_bow && player_action.shoot && player.bow_draw_time < PLAYER_BOW_DRAW_DELAY){
               player.bow_draw_time += dt;
          }else if(!player_action.shoot){
               if(player.bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                    undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                    Position_t arrow_pos = player.pos;
                    switch(player.face){
                    default:
                         break;
                    case DIRECTION_LEFT:
                         arrow_pos.pixel.y -= 2;
                         arrow_pos.pixel.x -= 8;
                         break;
                    case DIRECTION_RIGHT:
                         arrow_pos.pixel.y -= 2;
                         arrow_pos.pixel.x += 8;
                         break;
                    case DIRECTION_UP:
                         arrow_pos.pixel.y += 7;
                         break;
                    case DIRECTION_DOWN:
                         arrow_pos.pixel.y -= 11;
                         break;
                    }
                    arrow_pos.z += ARROW_SHOOT_HEIGHT;
                    arrow_spawn(&arrow_array, arrow_pos, player.face);
               }
               player.bow_draw_time = 0.0f;
          }

          if(!player_action.move_left && !player_action.move_right && !player_action.move_up && !player_action.move_down){
               player.walk_frame = 1;
          }else{
               player.walk_frame_time += dt;

               if(player.walk_frame_time > PLAYER_WALK_DELAY){
                    if(vec_magnitude(player.vel) > PLAYER_IDLE_SPEED){
                         player.walk_frame_time = 0.0f;

                         player.walk_frame += player.walk_frame_delta;
                         if(player.walk_frame > 2 || player.walk_frame < 0){
                              player.walk_frame = 1;
                              player.walk_frame_delta = -player.walk_frame_delta;
                         }
                    }else{
                         player.walk_frame = 1;
                         player.walk_frame_time = 0.0f;
                    }
               }
          }

          Vec_t pos_vec = pos_to_vec(player.pos);

#if 0
          // TODO: do we want this in the future?
          // figure out what room we should focus on
          Position_t room_center {};
          for(U32 i = 0; i < ELEM_COUNT(rooms); i++){
               if(coord_in_rect(vec_to_coord(pos_vec), rooms[i])){
                    S16 half_room_width = (rooms[i].right - rooms[i].left) / 2;
                    S16 half_room_height = (rooms[i].top - rooms[i].bottom) / 2;
                    Coord_t room_center_coord {(S16)(rooms[i].left + half_room_width),
                                               (S16)(rooms[i].bottom + half_room_height)};
                    room_center = coord_to_pos(room_center_coord);
                    break;
               }
          }
#else
          Position_t room_center = coord_to_pos(Coord_t{8, 8});
#endif

          Position_t camera_movement = room_center - camera;
          camera += camera_movement * 0.05f;

          float drag = 0.625f;

          // block movement
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;

               //Coord_t block_prev_coord = pos_to_coord(block->pos);

               // TODO: compress with player movement
               Vec_t pos_delta = (block->accel * dt * dt * 0.5f) + (block->vel * dt);
               block->vel += block->accel * dt;
               block->vel *= drag;

               // TODO: blocks with velocity need to be checked against other blocks

               Position_t pre_move = block->pos;
               block->pos += pos_delta;

               bool stop_on_boundary_x = false;
               bool stop_on_boundary_y = false;
               bool held_up = false;

               if(pos_delta.x != 0.0f || pos_delta.y != 0.0f){
                    check_block_collision_with_other_blocks(block, block_quad_tree, interactive_quad_tree, &tilemap,
                                                            &player, last_block_pushed, last_block_pushed_direction);
               }

               // get the current coord of the center of the block
               Pixel_t center = block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               Coord_t coord = pixel_to_coord(center);

               Coord_t skip_coord[DIRECTION_COUNT];
               find_portal_adjacents_to_skip_collision_check(coord, interactive_quad_tree, skip_coord);

               // check for adjacent walls
               if(block->vel.x > 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_RIGHT, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_x = true;
                    }else if(coord_b != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }else if(block->vel.x < 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_LEFT, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_x = true;
                    }else if(coord_b != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_x = true;
                    }else{
                         stop_on_boundary_x = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }

               if(block->vel.y > 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_UP, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_UP] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_y = true;
                    }else if(coord_b != skip_coord[DIRECTION_UP] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }else if(block->vel.y < 0.0f){
                    Pixel_t pixel_a;
                    Pixel_t pixel_b;
                    block_adjacent_pixels_to_check(block, DIRECTION_DOWN, &pixel_a, &pixel_b);
                    Coord_t coord_a = pixel_to_coord(pixel_a);
                    Coord_t coord_b = pixel_to_coord(pixel_b);
                    if(coord_a != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&tilemap, coord_a)){
                         stop_on_boundary_y = true;
                    }else if(coord_b != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&tilemap, coord_b)){
                         stop_on_boundary_y = true;
                    }else{
                         stop_on_boundary_y = quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_a) ||
                                              quad_tree_interactive_solid_at(interactive_quad_tree, &tilemap, coord_b);
                    }
               }

               if(block != last_block_pushed && !block_on_ice(block, &tilemap, interactive_quad_tree)){
                    stop_on_boundary_x = true;
                    stop_on_boundary_y = true;
               }

               if(stop_on_boundary_x){
                    // stop on tile boundaries separately for each axis
                    S16 boundary_x = range_passes_tile_boundary(pre_move.pixel.x, block->pos.pixel.x, block->push_start.x);
                    if(boundary_x){
                         block->pos.pixel.x = boundary_x;
                         block->pos.decimal.x = 0.0f;
                         block->vel.x = 0.0f;
                         block->accel.x = 0.0f;
                    }
               }

               if(stop_on_boundary_y){
                    S16 boundary_y = range_passes_tile_boundary(pre_move.pixel.y, block->pos.pixel.y, block->push_start.y);
                    if(boundary_y){
                         block->pos.pixel.y = boundary_y;
                         block->pos.decimal.y = 0.0f;
                         block->vel.y = 0.0f;
                         block->accel.y = 0.0f;
                    }
               }

               held_up = block_held_up_by_another_block(block, block_quad_tree);

               // TODO: should we care about the decimal component of the position ?
               auto* interactive = quad_tree_interactive_find_at(interactive_quad_tree, coord);
               if(interactive){
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         if(block->pos.z == interactive->popup.lift.ticks - 2){
                              block->pos.z++;
                              held_up = true;
                         }else if(block->pos.z > (interactive->popup.lift.ticks - 1)){
                              block->fall_time += dt;
                              if(block->fall_time >= FALL_TIME){
                                   block->fall_time -= FALL_TIME;
                                   block->pos.z--;
                              }
                              held_up = true;
                         }else if(block->pos.z == (interactive->popup.lift.ticks - 1)){
                              held_up = true;
                         }
                    }
               }

               if(!held_up && block->pos.z > 0){
                    block->fall_time += dt;
                    if(block->fall_time >= FALL_TIME){
                         block->fall_time -= FALL_TIME;
                         block->pos.z--;
                    }
               }

               coord = pixel_to_coord(block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
               Coord_t premove_coord = pixel_to_coord(pre_move.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});

               Position_t block_center = block->pos;
               block_center.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};
               S8 rotations_between = teleport_position_across_portal(&block_center, NULL, interactive_quad_tree, &tilemap, premove_coord,
                                                                      coord);
               if(rotations_between >= 0){
                    block->pos = block_center;
                    block->pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

                    block->vel = vec_rotate_quadrants_clockwise(block->vel, rotations_between);
                    block->accel = vec_rotate_quadrants_clockwise(block->accel, rotations_between);

                    check_block_collision_with_other_blocks(block, block_quad_tree, interactive_quad_tree, &tilemap,
                                                            &player, last_block_pushed, last_block_pushed_direction);

                    // try teleporting if we collided with a block
                    premove_coord = pixel_to_coord(block_center.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});
                    coord = pixel_to_coord(block->pos.pixel + Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS});

                    block_center = block->pos;
                    block_center.pixel += Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

                    rotations_between = teleport_position_across_portal(&block_center, NULL, interactive_quad_tree, &tilemap, premove_coord,
                                                    coord);
                    if(rotations_between >= 0){
                         block->pos = block_center;
                         block->pos.pixel -= Pixel_t{HALF_TILE_SIZE_IN_PIXELS, HALF_TILE_SIZE_IN_PIXELS};

                         block->vel = vec_rotate_quadrants_clockwise(block->vel, rotations_between);
                         block->accel = vec_rotate_quadrants_clockwise(block->accel, rotations_between);
                    }
               }
          }

          // illuminate and ice
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;
               if(block->element == ELEMENT_FIRE){
                    illuminate(block_get_coord(block), 255, &tilemap, block_quad_tree);
               }else if(block->element == ELEMENT_ICE){
                    auto block_coord = block_get_coord(block);
                    spread_ice(block_coord, 1, &tilemap, interactive_quad_tree, block_quad_tree, false);
               }
          }

          // melt ice
          for(S16 i = 0; i < block_array.count; i++){
               Block_t* block = block_array.elements + i;
               if(block->element == ELEMENT_FIRE){
                    melt_ice(block_get_coord(block), 1, &tilemap, interactive_quad_tree, block_quad_tree, false);
               }
          }

          for(S16 i = 0; i < interactive_array.count; i++){
               Interactive_t* interactive = interactive_array.elements + i;
               if(interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR){
                    Tile_t* tile = tilemap_get_tile(&tilemap, interactive->coord);
                    Rect_t coord_rect = rect_surrounding_adjacent_coords(interactive->coord);

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Block_t* block = nullptr;
                    for(S16 b = 0; b < block_count; b++){
                         // blocks on the coordinate and on the ground block light
                         if(block_get_coord(blocks[b]) == interactive->coord && blocks[b]->pos.z == 0){
                              block = blocks[b];
                              break;
                         }
                    }

                    if(interactive->detector.on && (tile->light < LIGHT_DETECTOR_THRESHOLD || block)){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->detector.on = false;
                    }else if(!interactive->detector.on && tile->light >= LIGHT_DETECTOR_THRESHOLD && !block){
                         activate(&tilemap, interactive_quad_tree, interactive->coord);
                         interactive->detector.on = true;
                    }
               }else if(interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR){
                    Tile_t* tile = tilemap_get_tile(&tilemap, interactive->coord);
                    if(tile){
                         if(interactive->detector.on && !tile_is_iced(tile)){
                              activate(&tilemap, interactive_quad_tree, interactive->coord);
                              interactive->detector.on = false;
                         }else if(!interactive->detector.on && tile_is_iced(tile)){
                              activate(&tilemap, interactive_quad_tree, interactive->coord);
                              interactive->detector.on = true;
                         }
                    }
               }
          }

          Position_t portal_player_pos = {};

          // player movement
          {
               user_movement = vec_normalize(user_movement);
               player.accel = user_movement * PLAYER_SPEED;

               Vec_t pos_delta = (player.accel * dt * dt * 0.5f) + (player.vel * dt);
               player.vel += player.accel * dt;
               player.vel *= drag;

               if(fabs(vec_magnitude(player.vel)) > PLAYER_SPEED){
                    player.vel = vec_normalize(player.vel) * PLAYER_SPEED;
               }

               Coord_t skip_coord[DIRECTION_COUNT];
               Coord_t player_previous_coord = pos_to_coord(player.pos);
               Coord_t player_coord = pos_to_coord(player.pos + pos_delta);

               find_portal_adjacents_to_skip_collision_check(player_coord, interactive_quad_tree, skip_coord);
               S8 rotations_between = teleport_position_across_portal(&player.pos, &pos_delta, interactive_quad_tree, &tilemap, player_previous_coord,
                                                                      player_coord);

               player_coord = pos_to_coord(player.pos + pos_delta);

               bool collide_with_interactive = false;
               Vec_t player_delta_pos = move_player_position_through_world(player.pos, pos_delta, player.face, skip_coord,
                                                                           &player, &tilemap, interactive_quad_tree,
                                                                           &block_array, &block_to_push,
                                                                           &last_block_pushed_direction,
                                                                           &collide_with_interactive, &resetting);

               player_coord = pos_to_coord(player.pos + player_delta_pos);

               if(block_to_push){
                    DirectionMask_t block_move_dir_mask = vec_direction_mask(block_to_push->vel);
                    if(direction_in_mask(direction_mask_opposite(block_move_dir_mask), player.face))
                    {
                         // if the player is pushing against a block moving towards them, the block wins
                         player.push_time = 0;
                         block_to_push = nullptr;
                    }else{
                         F32 before_time = player.push_time;

                         player.push_time += dt;
                         if(player.push_time > BLOCK_PUSH_TIME){
                              if(before_time <= BLOCK_PUSH_TIME) undo_commit(&undo, &player, &tilemap, &block_array, &interactive_array);
                              block_push(block_to_push, last_block_pushed_direction, &tilemap, interactive_quad_tree, block_quad_tree, false);
                              if(block_to_push->pos.z > 0) player.push_time = -0.5f;
                         }
                    }
               }else{
                    player.push_time = 0;
               }

               if(rotations_between < 0){
                    rotations_between = teleport_position_across_portal(&player.pos, &player_delta_pos, interactive_quad_tree, &tilemap, player_previous_coord,
                                                                        player_coord);
               }else{
                    teleport_position_across_portal(&player.pos, &player_delta_pos, interactive_quad_tree, &tilemap, player_previous_coord, player_coord);
               }

               if(rotations_between >= 0){
                    player.face = direction_rotate_clockwise(player.face, rotations_between);
                    player.vel = vec_rotate_quadrants_clockwise(player.vel, rotations_between);
                    player.accel = vec_rotate_quadrants_clockwise(player.accel, rotations_between);

                    // set rotations for each direction the player wants to move
                    if(player_action.move_left) player_action.move_left_rotation = (player_action.move_left_rotation + rotations_between) % DIRECTION_COUNT;
                    if(player_action.move_right) player_action.move_right_rotation = (player_action.move_right_rotation + rotations_between) % DIRECTION_COUNT;
                    if(player_action.move_up) player_action.move_up_rotation = (player_action.move_up_rotation + rotations_between) % DIRECTION_COUNT;
                    if(player_action.move_down) player_action.move_down_rotation = (player_action.move_down_rotation + rotations_between) % DIRECTION_COUNT;
               }

               player.pos += player_delta_pos;
          }

          if(resetting){
               reset_timer += dt;
               if(reset_timer >= RESET_TIME){
                    resetting = false;

                    if(load_map_number(map_number, &player_start, &tilemap, &block_array, &interactive_array)){
                         setup_map(&player, player_start, &interactive_array, &interactive_quad_tree, &block_array,
                                   &block_quad_tree, &undo, &tilemap, &arrow_array);
                    }
               }
          }else{
               reset_timer -= dt;
               if(reset_timer <= 0) reset_timer = 0;
          }

          if(suite && !show_suite) continue;

          glClear(GL_COLOR_BUFFER_BIT);

          Position_t screen_camera = camera - Vec_t{0.5f, 0.5f} + Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};

          Coord_t min = pos_to_coord(screen_camera);
          Coord_t max = min + Coord_t{ROOM_TILE_SIZE, ROOM_TILE_SIZE};
          min = coord_clamp_zero_to_dim(min, tilemap.width - 1, tilemap.height - 1);
          max = coord_clamp_zero_to_dim(max, tilemap.width - 1, tilemap.height - 1);
          Position_t tile_bottom_left = coord_to_pos(min);
          Vec_t camera_offset = pos_to_vec(tile_bottom_left - screen_camera);

          // draw flats
          glBindTexture(GL_TEXTURE_2D, theme_texture);
          glBegin(GL_QUADS);
          glColor3f(1.0f, 1.0f, 1.0f);

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    Tile_t* tile = tilemap.tiles[y] + x;
                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, x, y);
                    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                         Coord_t coord {x, y};
                         PortalExit_t portal_exits = find_portal_exits(coord, &tilemap, interactive_quad_tree);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                                   Tile_t* portal_tile = tilemap.tiles[portal_coord.y] + portal_coord.x;
                                   Interactive_t* portal_interactive = quad_tree_find_at(interactive_quad_tree, portal_coord.x, portal_coord.y);
                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                   draw_flats(tile_pos, portal_tile, portal_interactive, theme_texture, portal_rotations);
                              }
                         }
                    }else{
                         draw_flats(tile_pos, tile, interactive, theme_texture, 0);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    Coord_t coord {x, y};
                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, coord.x, coord.y);

                    if(interactive && interactive->type == INTERACTIVE_TYPE_PORTAL && interactive->portal.on){
                         PortalExit_t portal_exits = find_portal_exits(coord, &tilemap, interactive_quad_tree);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));

                                   S16 px = portal_coord.x * TILE_SIZE_IN_PIXELS;
                                   S16 py = portal_coord.y * TILE_SIZE_IN_PIXELS;
                                   Rect_t coord_rect {(S16)(px), (S16)(py),
                                                      (S16)(px + TILE_SIZE_IN_PIXELS),
                                                      (S16)(py + TILE_SIZE_IN_PIXELS)};

                                   S16 block_count = 0;
                                   Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

                                   quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                   Interactive_t* portal_interactive = quad_tree_find_at(interactive_quad_tree, portal_coord.x, portal_coord.y);

                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                   Player_t* player_ptr = nullptr;
                                   Pixel_t portal_center_pixel = coord_to_pixel_at_center(portal_coord);
                                   if(pixel_distance_between(portal_center_pixel, player.pos.pixel) <= 20){
                                        player_ptr = &player;
                                   }
                                   draw_solids(tile_pos, portal_interactive, blocks, block_count, player_ptr, screen_camera,
                                               theme_texture, player_texture, portal_coord, coord, portal_rotations,
                                               &tilemap, interactive_quad_tree);
                              }
                         }

                         draw_interactive(interactive, tile_pos, coord, &tilemap, interactive_quad_tree);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = tilemap.tiles[y] + x;
                    if(tile && tile->id >= 16){
                         Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                         (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                         draw_tile_id(tile->id, tile_pos);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               Player_t* player_ptr = nullptr;
               if(pos_to_coord(player.pos).y == y) player_ptr = &player;

               for(S16 x = min.x; x <= max.x; x++){
                    Coord_t coord {x, y};

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    S16 px = coord.x * TILE_SIZE_IN_PIXELS;
                    S16 py = coord.y * TILE_SIZE_IN_PIXELS;
                    Rect_t coord_rect {(S16)(px - HALF_TILE_SIZE_IN_PIXELS), (S16)(py - HALF_TILE_SIZE_IN_PIXELS),
                                       (S16)(px + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS),
                                       (S16)(py + TILE_SIZE_IN_PIXELS + HALF_TILE_SIZE_IN_PIXELS)};

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(block_quad_tree, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Interactive_t* interactive = quad_tree_find_at(interactive_quad_tree, x, y);

                    draw_solids(tile_pos, interactive, blocks, block_count, player_ptr, screen_camera, theme_texture, player_texture,
                                coord, Coord_t{-1, -1}, 0, &tilemap, interactive_quad_tree);

               }

               // draw arrows
               static Vec_t arrow_tip_offset[DIRECTION_COUNT] = {
                    {0.0f,               9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  16.0f * PIXEL_SIZE},
                    {16.0f * PIXEL_SIZE, 9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  0.0f * PIXEL_SIZE},
               };

               for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
                    Arrow_t* arrow = arrow_array.arrows + a;
                    if(!arrow->alive) continue;
                    if((arrow->pos.pixel.y / TILE_SIZE_IN_PIXELS) != y) continue;

                    Vec_t arrow_vec = pos_to_vec(arrow->pos - screen_camera);
                    arrow_vec.x -= arrow_tip_offset[arrow->face].x;
                    arrow_vec.y -= arrow_tip_offset[arrow->face].y;

                    glEnd();
                    glBindTexture(GL_TEXTURE_2D, arrow_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);

                    // shadow
                    //arrow_vec.y -= (arrow->pos.z * PIXEL_SIZE);
                    Vec_t tex_vec = arrow_frame(arrow->face, 1);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(arrow_vec.x, arrow_vec.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y);

                    arrow_vec.y += (arrow->pos.z * PIXEL_SIZE);

                    S8 y_frame = 0;
                    if(arrow->element) y_frame = 2 + ((arrow->element - 1) * 4);

                    tex_vec = arrow_frame(arrow->face, y_frame);
                    glTexCoord2f(tex_vec.x, tex_vec.y);
                    glVertex2f(arrow_vec.x, arrow_vec.y);
                    glTexCoord2f(tex_vec.x, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y + ARROW_FRAME_HEIGHT);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y + TILE_SIZE);
                    glTexCoord2f(tex_vec.x + ARROW_FRAME_WIDTH, tex_vec.y);
                    glVertex2f(arrow_vec.x + TILE_SIZE, arrow_vec.y);

                    glEnd();

                    glBindTexture(GL_TEXTURE_2D, theme_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }
          }

          glEnd();

          // player circle
          Position_t player_camera_offset = player.pos - screen_camera;
          pos_vec = pos_to_vec(player_camera_offset);

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_LINES);
          if(block_to_push){
               glColor3f(1.0f, 0.0f, 0.0f);
          }else{
               glColor3f(1.0f, 1.0f, 1.0f);
          }
          Vec_t prev_vec {pos_vec.x + PLAYER_RADIUS, pos_vec.y};
          S32 segments = 32;
          F32 delta = 3.14159f * 2.0f / (F32)(segments);
          F32 angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * PLAYER_RADIUS;
               F32 dy = sin(angle) * PLAYER_RADIUS;

               glVertex2f(prev_vec.x, prev_vec.y);
               glVertex2f(pos_vec.x + dx, pos_vec.y + dy);
               prev_vec.x = pos_vec.x + dx;
               prev_vec.y = pos_vec.y + dy;
               angle += delta;
          }

          pos_vec = pos_to_vec(portal_player_pos - screen_camera);
          prev_vec = {pos_vec.x + PLAYER_RADIUS, pos_vec.y};
          segments = 32;
          delta = 3.14159f * 2.0f / (F32)(segments);
          angle = 0.0f  + delta;
          for(S32 i = 0; i <= segments; i++){
               F32 dx = cos(angle) * PLAYER_RADIUS;
               F32 dy = sin(angle) * PLAYER_RADIUS;

               glVertex2f(prev_vec.x, prev_vec.y);
               glVertex2f(pos_vec.x + dx, pos_vec.y + dy);
               prev_vec.x = pos_vec.x + dx;
               prev_vec.y = pos_vec.y + dy;
               angle += delta;
          }

          glEnd();

#if 0
          Vec_t collided_with_center = {(float)(g_collided_with_pixel.x) * PIXEL_SIZE, (float)(g_collided_with_pixel.y) * PIXEL_SIZE};
          const float half_block_size = PIXEL_SIZE * HALF_TILE_SIZE_IN_PIXELS;
          Quad_t collided_with_quad = {collided_with_center.x - half_block_size, collided_with_center.y - half_block_size,
                                       collided_with_center.x + half_block_size, collided_with_center.y + half_block_size};
          draw_quad_wireframe(&collided_with_quad, 255.0f, 0.0f, 255.0f);
#endif

#if 0
          // light
          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_QUADS);
          for(S16 y = min.y; y <= max.y; y++){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = tilemap.tiles[y] + x;

                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                    glColor4f(0.0f, 0.0f, 0.0f, (F32)(255 - tile->light) / 255.0f);
                    glVertex2f(tile_pos.x, tile_pos.y);
                    glVertex2f(tile_pos.x, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y + TILE_SIZE);
                    glVertex2f(tile_pos.x + TILE_SIZE, tile_pos.y);

               }
          }
          glEnd();
#endif

          // player start
          draw_selection(player_start, player_start, screen_camera, 0.0f, 1.0f, 0.0f);

          // editor
          switch(editor.mode){
          default:
               break;
          case EDITOR_MODE_OFF:
               // pass
               break;
          case EDITOR_MODE_CATEGORY_SELECT:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               Vec_t vec = {0.0f, 0.0f};

               for(S32 g = 0; g < editor.category_array.count; ++g){
                    auto* category = editor.category_array.elements + g;
                    auto* stamp_array = category->elements + 0;

                    for(S16 s = 0; s < stamp_array->count; s++){
                         auto* stamp = stamp_array->elements + s;
                         if(g && (g % ROOM_TILE_SIZE) == 0){
                              vec.x = 0.0f;
                              vec.y += TILE_SIZE;
                         }

                         switch(stamp->type){
                         default:
                              break;
                         case STAMP_TYPE_TILE_ID:
                              draw_tile_id(stamp->tile_id, vec);
                              break;
                         case STAMP_TYPE_TILE_FLAGS:
                              draw_tile_flags(stamp->tile_flags, vec);
                              break;
                         case STAMP_TYPE_BLOCK:
                         {
                              Block_t block = {};
                              block.element = stamp->block.element;
                              block.face = stamp->block.face;

                              draw_block(&block, vec);
                         } break;
                         case STAMP_TYPE_INTERACTIVE:
                         {
                              draw_interactive(&stamp->interactive, vec, Coord_t{-1, -1}, &tilemap, interactive_quad_tree);
                         } break;
                         }
                    }

                    vec.x += TILE_SIZE;
               }

               glEnd();
          } break;
          case EDITOR_MODE_STAMP_SELECT:
          case EDITOR_MODE_STAMP_HIDE:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               // draw stamp at mouse
               auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
               Coord_t mouse_coord = mouse_select_coord(mouse_screen);

               for(S16 s = 0; s < stamp_array->count; s++){
                    auto* stamp = stamp_array->elements + s;
                    Vec_t stamp_pos = coord_to_screen_position(mouse_coord + stamp->offset);
                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         draw_tile_id(stamp->tile_id, stamp_pos);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         draw_tile_flags(stamp->tile_flags, stamp_pos);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;
                         draw_block(&block, stamp_pos);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         draw_interactive(&stamp->interactive, stamp_pos, Coord_t{-1, -1}, &tilemap, interactive_quad_tree);
                    } break;
                    }
               }

               if(editor.mode == EDITOR_MODE_STAMP_SELECT){
                    // draw stamps to select from at the bottom
                    Vec_t pos = {0.0f, 0.0f};
                    S16 row_height = 1;
                    auto* category = editor.category_array.elements + editor.category;

                    for(S32 g = 0; g < category->count; ++g){
                         stamp_array = category->elements + g;
                         Coord_t dimensions = stamp_array_dimensions(stamp_array);
                         if(dimensions.y > row_height) row_height = dimensions.y;

                         for(S32 s = 0; s < stamp_array->count; s++){
                              auto* stamp = stamp_array->elements + s;
                              Vec_t stamp_vec = pos + coord_to_vec(stamp->offset);

                              switch(stamp->type){
                              default:
                                   break;
                              case STAMP_TYPE_TILE_ID:
                                   draw_tile_id(stamp->tile_id, stamp_vec);
                                   break;
                              case STAMP_TYPE_TILE_FLAGS:
                                   draw_tile_flags(stamp->tile_flags, stamp_vec);
                                   break;
                              case STAMP_TYPE_BLOCK:
                              {
                                   Block_t block = {};
                                   block.element = stamp->block.element;
                                   block.face = stamp->block.face;
                                   draw_block(&block, stamp_vec);
                              } break;
                              case STAMP_TYPE_INTERACTIVE:
                              {
                                   draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &tilemap, interactive_quad_tree);
                              } break;
                              }
                         }

                         pos.x += (dimensions.x * TILE_SIZE);
                         if(pos.x >= 1.0f){
                              pos.x = 0.0f;
                              pos.y += row_height * TILE_SIZE;
                              row_height = 1;
                         }
                    }
               }

               glEnd();
          } break;
          case EDITOR_MODE_CREATE_SELECTION:
               draw_selection(editor.selection_start, editor.selection_end, screen_camera, 1.0f, 0.0f, 0.0f);
               break;
          case EDITOR_MODE_SELECTION_MANIPULATION:
          {
               glBindTexture(GL_TEXTURE_2D, theme_texture);
               glBegin(GL_QUADS);
               glColor3f(1.0f, 1.0f, 1.0f);

               for(S32 g = 0; g < editor.selection.count; ++g){
                    auto* stamp = editor.selection.elements + g;
                    Position_t stamp_pos = coord_to_pos(editor.selection_start + stamp->offset);
                    Vec_t stamp_vec = pos_to_vec(stamp_pos);

                    switch(stamp->type){
                    default:
                         break;
                    case STAMP_TYPE_TILE_ID:
                         draw_tile_id(stamp->tile_id, stamp_vec);
                         break;
                    case STAMP_TYPE_TILE_FLAGS:
                         draw_tile_flags(stamp->tile_flags, stamp_vec);
                         break;
                    case STAMP_TYPE_BLOCK:
                    {
                         Block_t block = {};
                         block.element = stamp->block.element;
                         block.face = stamp->block.face;
                         draw_block(&block, stamp_vec);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &tilemap, interactive_quad_tree);
                    } break;
                    }
               }
               glEnd();

               Rect_t selection_bounds = editor_selection_bounds(&editor);
               Coord_t min_coord {selection_bounds.left, selection_bounds.bottom};
               Coord_t max_coord {selection_bounds.right, selection_bounds.top};
               draw_selection(min_coord, max_coord, screen_camera, 1.0f, 0.0f, 0.0f);
          } break;
          }

          // if(resetting){
               glBegin(GL_QUADS);
               glColor4f(0.0f, 0.0f, 0.0f, reset_timer / RESET_TIME);
               glVertex2f(0, 0);
               glVertex2f(0, 1);
               glVertex2f(1, 1);
               glVertex2f(1, 0);
               glEnd();
          // }


          SDL_GL_SwapWindow(window);
     }

     switch(demo_mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          player_action_perform(&player_action, &player, PLAYER_ACTION_TYPE_END_DEMO, demo_mode,
                                demo_file, frame_count);
          // save map and player position
          save_map_to_file(demo_file, player_start, &tilemap, &block_array, &interactive_array);
          fwrite(&player.pos.pixel, sizeof(player.pos.pixel), 1, demo_file);
          fclose(demo_file);
          break;
     case DEMO_MODE_PLAY:
          fclose(demo_file);
          break;
     }

     quad_tree_free(interactive_quad_tree);
     quad_tree_free(block_quad_tree);

     destroy(&block_array);
     destroy(&interactive_array);
     destroy(&undo);
     destroy(&tilemap);
     destroy(&editor);

     if(!suite){
          glDeleteTextures(1, &theme_texture);
          glDeleteTextures(1, &player_texture);
          glDeleteTextures(1, &arrow_texture);

          SDL_GL_DeleteContext(opengl_context);
          SDL_DestroyWindow(window);
          SDL_Quit();
     }

     Log_t::destroy();

     return 0;
}
