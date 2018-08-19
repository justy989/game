/*
http://www.simonstalenhag.se/
-Grant Sanderson 3blue1brown
-Shane Hendrixson

TODO:
Entanglement:
- entangle puzzle where there is a line of pressure plates against the wall with a line of popups on the other side that would
  trap an entangled block if it got to close, stopping the player from using walls to get blocks closer
- entangle puzzle with an extra non-entangled block where you don't want one of the entangled blocks to move, and you use
  the non-entangled block to accomplish that
- arrow entanglement
- arrow kills player

Big Features:
- Block splitting
- Block ice collision with masses
- 3D

NOTES:
- Only 2 blocks high can go through portals
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
#include "object_array.h"
#include "portal_exit.h"
#include "block.h"
#include "editor.h"
#include "utils.h"
#include "map_format.h"
#include "draw.h"
#include "block_utils.h"
#include "demo.h"
#include "collision.h"
#include "world.h"

// #define BLOCKS_SQUISH_PLAYER

FILE* load_demo_number(S32 map_number, const char** demo_filepath){
     char filepath[64] = {};
     snprintf(filepath, 64, "content/%03d.bd", map_number);
     *demo_filepath = strdup(filepath);
     return fopen(*demo_filepath, "rb");
}

void cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                         ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(&world->tilemap, demo_starting_tilemap);
     deep_copy(&world->blocks, demo_starting_blocks);
     deep_copy(&world->interactives, demo_starting_interactives);
}

void fetch_cache_for_demo_seek(World_t* world, TileMap_t* demo_starting_tilemap, ObjectArray_t<Block_t>* demo_starting_blocks,
                               ObjectArray_t<Interactive_t>* demo_starting_interactives){
     deep_copy(demo_starting_tilemap, &world->tilemap);
     deep_copy(demo_starting_blocks, &world->blocks);
     deep_copy(demo_starting_interactives, &world->interactives);
}

bool load_map_number_demo(Demo_t* demo, S16 map_number, S64* frame_count){
     if(demo->file) fclose(demo->file);
     demo->file = load_demo_number(map_number, &demo->filepath);
     if(!demo->file){
          LOG("missing map %d corresponding demo.\n", map_number);
          return false;
     }

     free(demo->entries.entries);
     demo->entry_index = 0;
     fread(&demo->version, sizeof(demo->version), 1, demo->file);
     demo->entries = demo_entries_get(demo->file);
     *frame_count = 0;
     demo->last_frame = demo->entries.entries[demo->entries.count - 1].frame;
     LOG("testing demo %s: version %d with %ld actions across %ld frames\n", demo->filepath,
         demo->version, demo->entries.count, demo->last_frame);
     return true;
}

bool load_map_number_map(S16 map_number, World_t* world, Undo_t* undo,
                         Coord_t* player_start, PlayerAction_t* player_action){
     if(load_map_number(map_number, player_start, world)){
          setup_map(*player_start, world, undo);
          *player_action = {};
          return true;
     }

     return false;
}

Block_t block_from_stamp(Stamp_t* stamp){
     Block_t block = {};
     block.element = stamp->block.element;
     // block.rotation = stamp->block.rotation;
     block.entangle_index = stamp->block.entangle_index;
     block.clone_start = Coord_t{};
     block.clone_id = 0;
     return block;
}

int main(int argc, char** argv){
     const char* load_map_filepath = nullptr;
     bool test = false;
     bool suite = false;
     bool show_suite = false;
     S16 map_number = 0;
     S16 first_map_number = 0;

     Demo_t demo {};

     for(int i = 1; i < argc; i++){
          if(strcmp(argv[i], "-play") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo.filepath = argv[next];
               demo.mode = DEMO_MODE_PLAY;
          }else if(strcmp(argv[i], "-record") == 0){
               int next = i + 1;
               if(next >= argc) continue;
               demo.filepath = argv[next];
               demo.mode = DEMO_MODE_RECORD;
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
               map_number = (S16)(atoi(argv[next]));
               first_map_number = map_number;
          }else if(strcmp(argv[i], "-h") == 0){
               printf("%s [options]\n", argv[0]);
               printf("  -play   <demo filepath> replay a recorded demo file\n");
               printf("  -record <demo filepath> record a demo file\n");
               printf("  -load   <map filepath>  load a map\n");
               printf("  -test                   validate the map state is correct after playing a demo\n");
               printf("  -suite                  run map/demo combos in succession validating map state after each headless\n");
               printf("  -show                   use in combination with -suite to run with a head\n");
               printf("  -map    <number>        load a map by number\n");
               printf("  -h this help.\n");
               return 0;
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
     SDL_GLContext opengl_context = nullptr;
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

          // SDL_GL_SetSwapInterval(SDL_TRUE);
          SDL_GL_SetSwapInterval(SDL_FALSE);
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

     switch(demo.mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          demo.file = fopen(demo.filepath, "w");
          if(!demo.file){
               LOG("failed to open demo file: %s\n", demo.filepath);
               return 1;
          }
          demo.version = 2;
          fwrite(&demo.version, sizeof(demo.version), 1, demo.file);
          break;
     case DEMO_MODE_PLAY:
          demo.file = fopen(demo.filepath, "r");
          if(!demo.file){
               LOG("failed to open demo file: %s\n", demo.filepath);
               return 1;
          }
          fread(&demo.version, sizeof(demo.version), 1, demo.file);
          demo.entries = demo_entries_get(demo.file);
          demo.last_frame = demo.entries.entries[demo.entries.count - 1].frame;
          LOG("playing demo %s: version %d with %ld actions across %ld frames\n", demo.filepath,
              demo.version, demo.entries.count, demo.last_frame);
          break;
     }

     World_t world {};

     Editor_t editor {};
     Undo_t undo {};

     Coord_t player_start {2, 8};

     S64 frame_count = 0;

     bool quit = false;
     bool seeked_with_mouse = false;
     bool resetting = false;
     F32 reset_timer = 1.0f;

     PlayerAction_t player_action {};
     Position_t camera = coord_to_pos(Coord_t{8, 8});

     Vec_t mouse_screen = {}; // 0.0f to 1.0f
     Position_t mouse_world = {};
     bool ctrl_down = false;

     // cached to seek in demo faster
     TileMap_t demo_starting_tilemap {};
     ObjectArray_t<Block_t> demo_starting_blocks {};
     ObjectArray_t<Interactive_t> demo_starting_interactives {};

     Quad_t pct_bar_outline_quad = {0, 2.0f * PIXEL_SIZE, 1.0f, 0.02f};

     if(load_map_filepath){
          if(!load_map(load_map_filepath, &player_start, &world.tilemap, &world.blocks, &world.interactives)){
               return 1;
          }

          if(demo.mode == DEMO_MODE_PLAY){
               cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
          }
     }else if(suite){
          if(!load_map_number(map_number, &player_start, &world)){
               return 1;
          }

          cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

          demo.mode = DEMO_MODE_PLAY;
          if(!load_map_number_demo(&demo, map_number, &frame_count)){
               return 1;
          }
     }else if(map_number){
          if(!load_map_number(map_number, &player_start, &world)){
               return 1;
          }

          if(demo.mode == DEMO_MODE_PLAY){
               cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
          }
     }else{
          init(&world.tilemap, ROOM_TILE_SIZE, ROOM_TILE_SIZE);

          for(S16 i = 0; i < world.tilemap.width; i++){
               world.tilemap.tiles[0][i].id = 33;
               world.tilemap.tiles[1][i].id = 17;
               world.tilemap.tiles[world.tilemap.height - 1][i].id = 16;
               world.tilemap.tiles[world.tilemap.height - 2][i].id = 32;
          }

          for(S16 i = 0; i < world.tilemap.height; i++){
               world.tilemap.tiles[i][0].id = 18;
               world.tilemap.tiles[i][1].id = 19;
               world.tilemap.tiles[i][world.tilemap.width - 2].id = 34;
               world.tilemap.tiles[i][world.tilemap.height - 1].id = 35;
          }

          world.tilemap.tiles[0][0].id = 36;
          world.tilemap.tiles[0][1].id = 37;
          world.tilemap.tiles[1][0].id = 20;
          world.tilemap.tiles[1][1].id = 21;

          world.tilemap.tiles[16][0].id = 22;
          world.tilemap.tiles[16][1].id = 23;
          world.tilemap.tiles[15][0].id = 38;
          world.tilemap.tiles[15][1].id = 39;

          world.tilemap.tiles[15][15].id = 40;
          world.tilemap.tiles[15][16].id = 41;
          world.tilemap.tiles[16][15].id = 24;
          world.tilemap.tiles[16][16].id = 25;

          world.tilemap.tiles[0][15].id = 42;
          world.tilemap.tiles[0][16].id = 43;
          world.tilemap.tiles[1][15].id = 26;
          world.tilemap.tiles[1][16].id = 27;
          if(!init(&world.interactives, 1)){
               return 1;
          }
          world.interactives.elements[0].coord.x = -1;
          world.interactives.elements[0].coord.y = -1;

          if(!init(&world.blocks, 1)){
               return 1;
          }
          world.blocks.elements[0].pos = coord_to_pos(Coord_t{-1, -1});
     }

     setup_map(player_start, &world, &undo);
     init(&editor);

     F32 dt = 0.0f;

     auto last_time = std::chrono::system_clock::now();
     auto current_time = last_time;

     while(!quit){
          if((!suite || show_suite) && demo.seek_frame < 0){
               current_time = std::chrono::system_clock::now();
               std::chrono::duration<double> elapsed_seconds = current_time - last_time;
               dt = (F32)(elapsed_seconds.count());

               if(demo.mode == DEMO_MODE_PLAY){
                    if(dt < (0.0166666f / demo.dt_scalar)) continue;
               }else{
                    if(dt < 0.0166666f) continue; // limit 60 fps
               }
          }

          last_time = current_time;

          // TODO: consider 30fps as minimum for random noobs computers
          // if(demo.mode) dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.
          dt = 0.0166666f; // the game always runs as if a 60th of a frame has occurred.

          quad_tree_free(world.block_qt);
          world.block_qt = quad_tree_build(&world.blocks);

          if(!demo.paused || demo.seek_frame >= 0){
               frame_count++;
               if(demo.seek_frame == frame_count) demo.seek_frame = -1;
          }

          player_action.last_activate = player_action.activate;
          for(S16 i = 0; i < world.players.count; i++) {
               world.players.elements[i].reface = false;
          }

          if(demo.mode == DEMO_MODE_PLAY){
               bool end_of_demo = false;
               if(demo.entries.entries[demo.entry_index].player_action_type == PLAYER_ACTION_TYPE_END_DEMO){
                    end_of_demo = (frame_count == demo.entries.entries[demo.entry_index].frame);
               }else{
                    while(frame_count == demo.entries.entries[demo.entry_index].frame){
                         player_action_perform(&player_action, &world.players,
                                               demo.entries.entries[demo.entry_index].player_action_type, demo.mode,
                                               demo.file, frame_count);
                         demo.entry_index++;
                    }
               }

               if(end_of_demo){
                    if(test){
                         if(!test_map_end_state(&world, &demo)){
                              LOG("test failed\n");
                              demo.mode = DEMO_MODE_NONE;
                              if(suite && !show_suite) return 1;
                         }else if(suite){
                              map_number++;
                              S16 maps_tested = map_number - first_map_number;

                              if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){

                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }else{
                                   LOG("Done Testing %d maps.\n", maps_tested);
                                   return 0;
                              }
                         }
                    }else{
                         demo.paused = true;
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
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_LEFT_START,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_RIGHT:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.x++;
                              editor.selection_end.x++;
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_RIGHT_START,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_UP:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y++;
                              editor.selection_end.y++;
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_UP_START,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_DOWN:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              editor.selection_start.y--;
                              editor.selection_end.y--;
                         }else if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_DOWN_START,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_E:
                         if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_ACTIVATE_START,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_SPACE:
                         if(demo.mode == DEMO_MODE_PLAY){
                              demo.paused = !demo.paused;
                         }else{
                              if(!resetting){
                                   player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_SHOOT_START,
                                                         demo.mode, demo.file, frame_count);
                              }
                         }
                         break;
                    case SDL_SCANCODE_L:
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);
                              }
                         }
                         break;
                    case SDL_SCANCODE_LEFTBRACKET:
                         map_number--;
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }
                         }else{
                              map_number++;
                         }
                         break;
                    case SDL_SCANCODE_RIGHTBRACKET:
                         map_number++;
                         if(load_map_number_map(map_number, &world, &undo, &player_start, &player_action)){
                              if(demo.mode == DEMO_MODE_PLAY){
                                   cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                   if(load_map_number_demo(&demo, map_number, &frame_count)){
                                        continue; // reset to the top of the loop
                                   }else{
                                        return 1;
                                   }
                              }
                         }else{
                              map_number--;
                         }
                         break;
                    case SDL_SCANCODE_MINUS:
                         if(demo.mode == DEMO_MODE_PLAY){
                              if(demo.dt_scalar > 0.1f){
                                   demo.dt_scalar -= 0.1f;
                                   LOG("demo dt scalar: %.1f\n", demo.dt_scalar);
                              }
                         }
                         break;
                    case SDL_SCANCODE_EQUALS:
                         if(demo.mode == DEMO_MODE_PLAY){
                              demo.dt_scalar += 0.1f;
                              LOG("demo dt scalar: %.1f\n", demo.dt_scalar);
                         }
                         break;
                    case SDL_SCANCODE_V:
                    {
                         char filepath[64];
                         snprintf(filepath, 64, "content/%03d.bm", map_number);
                         save_map(filepath, player_start, &world.tilemap, &world.blocks, &world.interactives);
                    } break;
                    case SDL_SCANCODE_U:
                         if(!resetting){
                              player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_UNDO,
                                                    demo.mode, demo.file, frame_count);
                         }
                         break;
                    case SDL_SCANCODE_N:
                    {
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, mouse_select_world(mouse_screen, camera));
                         if(tile){
                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_LEFT){
                                   TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_LEFT_ON);
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_MID){
                                   TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_MID_ON);
                              }

                              if(tile->flags & TILE_FLAG_WIRE_CLUSTER_RIGHT){
                                   TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_CLUSTER_RIGHT_ON);
                              }

                              if(tile->flags & TILE_FLAG_WIRE_LEFT ||
                                 tile->flags & TILE_FLAG_WIRE_UP ||
                                 tile->flags & TILE_FLAG_WIRE_RIGHT ||
                                 tile->flags & TILE_FLAG_WIRE_DOWN){
                                   TOGGLE_BIT_FLAG(tile->flags, TILE_FLAG_WIRE_STATE);
                              }
                         }
                    } break;
                    case SDL_SCANCODE_8:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto coord = mouse_select_world(mouse_screen, camera);
                              auto rect = rect_surrounding_coord(coord);

                              S16 block_count = 0;
                              Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                              quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                              if(block_count > 1){
                                   LOG("error: too man blocks in coord, unsure which one to entangle!\\n");
                              }else if(block_count == 1){
                                   S16 block_index = (S16)(blocks[0] - world.blocks.elements);
                                   if(block_index >= 0 && block_index < world.blocks.count){
                                        if(editor.block_entangle_index_save >= 0 && editor.block_entangle_index_save != block_index){
                                             undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                                             // the magic happens here
                                             Block_t* other = world.blocks.elements + editor.block_entangle_index_save;

                                             // TODO: Probably not what we want in the future when things are
                                             //       circularly entangled, but fine for now
                                             if(other->entangle_index >= 0){
                                                  Block_t* other_other = world.blocks.elements + other->entangle_index;
                                                  other_other->entangle_index = -1;
                                             }

                                             other->entangle_index = block_index;
                                             blocks[0]->entangle_index = editor.block_entangle_index_save;

                                             // reset once we are done
                                             editor.block_entangle_index_save = -1;
                                             LOG("editor: entangled: %d <-> %d\n", blocks[0]->entangle_index, block_index);
                                        }else{
                                             editor.block_entangle_index_save = block_index;
                                             LOG("editor: entangle index save: %d\n", block_index);
                                        }
                                   }
                              }else if(block_count == 0){
                                   LOG("editor: clear entangle index save (was %d)\n", editor.block_entangle_index_save);
                                   editor.block_entangle_index_save = -1;
                              }
                         }
                         break;
                    case SDL_SCANCODE_2:
                         if(editor.mode == EDITOR_MODE_CATEGORY_SELECT){
                              auto pixel = mouse_select_world_pixel(mouse_screen, camera);

                              // TODO: make this a function where you can pass in entangler_index
                              S16 new_index = world.players.count;
                              if(resize(&world.players, world.players.count + (S16)(1))){
                                   Player_t* new_player = world.players.elements + new_index;
                                   *new_player = world.players.elements[0];
                                   new_player->pos = pixel_to_pos(pixel);
                              }
                         }
                         break;
                    // TODO: #ifdef DEBUG
                    case SDL_SCANCODE_GRAVE:
                         if(editor.mode == EDITOR_MODE_OFF){
                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }else{
                              editor.mode = EDITOR_MODE_OFF;
                              editor.selection_start = {};
                              editor.selection_end = {};
                              editor.block_entangle_index_save = -1;
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
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                              // clear coords below stamp
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                                   }
                              }

                              for(int i = 0; i < editor.selection.count; i++){
                                   Coord_t coord = editor.selection_start + editor.selection.elements[i].offset;
                                   apply_stamp(editor.selection.elements + i, coord,
                                               &world.tilemap, &world.blocks, &world.interactives, &world.interactive_qt, ctrl_down);
                              }

                              editor.mode = EDITOR_MODE_CATEGORY_SELECT;
                         }
                         break;
                    case SDL_SCANCODE_T:
                         if(editor.mode == EDITOR_MODE_SELECTION_MANIPULATION){
                              sort_selection(&editor);

                              S16 height_offset = (S16)((editor.selection_end.y - editor.selection_start.y) - 1);

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
                         undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         Coord_t player_coord = pos_to_coord(world.players.elements->pos)
                         Coord_t min = player_coord - Coord_t{1, 1};
                         Coord_t max = player_coord + Coord_t{1, 1};
                         for(S16 y = min.y; y <= max.y; y++){
                              for(S16 x = min.x; x <= max.x; x++){
                                   Coord_t coord {x, y};
                                   coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                              }
                         }
                    } break;
#endif
                    case SDL_SCANCODE_5:
                    {
                         reset_players(&world.players);
                         Player_t* player = world.players.elements;
                         player->pos.pixel = mouse_select_world_pixel(mouse_screen, camera) + HALF_TILE_SIZE_PIXEL;
                         player->pos.decimal.x = 0;
                         player->pos.decimal.y = 0;
                         break;
                    }
                    case SDL_SCANCODE_H:
                    {
                         Pixel_t pixel = mouse_select_world_pixel(mouse_screen, camera);
                         Coord_t coord = mouse_select_world(mouse_screen, camera);
                         LOG("mouse pixel: %d, %d, Coord: %d, %d\n", pixel.x, pixel.y, coord.x, coord.y);
                         describe_coord(coord, &world);
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
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_LEFT_STOP,
                                               demo.mode, demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_RIGHT:
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_RIGHT_STOP,
                                               demo.mode, demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_UP:
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_UP_STOP,
                                               demo.mode, demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_DOWN:
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_MOVE_DOWN_STOP,
                                               demo.mode, demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_E:
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_ACTIVATE_STOP,
                                               demo.mode, demo.file, frame_count);
                         break;
                    case SDL_SCANCODE_SPACE:
                         player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_SHOOT_STOP,
                                               demo.mode, demo.file, frame_count);
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
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_OFF:
                              if(demo.mode == DEMO_MODE_PLAY){
                                   if(vec_in_quad(&pct_bar_outline_quad, mouse_screen)){
                                        seeked_with_mouse = true;

                                        demo.seek_frame = (S64)((F32)(demo.last_frame) * mouse_screen.x);

                                        if(demo.seek_frame < frame_count){
                                             // TODO: compress with same comment elsewhere in this file
                                             fetch_cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                                             setup_map(player_start, &world, &undo);

                                             // reset some vars
                                             player_action = {};

                                             demo.entry_index = 0;
                                             frame_count = 0;
                                        }else if(demo.seek_frame == frame_count){
                                             demo.seek_frame = -1;
                                        }
                                   }
                              }
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
                                   undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                                   Coord_t select_coord = mouse_select_world(mouse_screen, camera);
                                   auto* stamp_array = editor.category_array.elements[editor.category].elements + editor.stamp;
                                   for(S16 s = 0; s < stamp_array->count; s++){
                                        auto* stamp = stamp_array->elements + s;
                                        apply_stamp(stamp, select_coord + stamp->offset,
                                                    &world.tilemap, &world.blocks, &world.interactives, &world.interactive_qt, ctrl_down);
                                   }

                                   quad_tree_free(world.block_qt);
                                   world.block_qt = quad_tree_build(&world.blocks);
                              }
                         } break;
                         }
                         break;
                    case SDL_BUTTON_RIGHT:
                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CATEGORY_SELECT:
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              coord_clear(mouse_select_world(mouse_screen, camera), &world.tilemap, &world.interactives,
                                          world.interactive_qt, &world.blocks);
                              break;
                         case EDITOR_MODE_STAMP_SELECT:
                         case EDITOR_MODE_STAMP_HIDE:
                         {
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Coord_t start = mouse_select_world(mouse_screen, camera);
                              Coord_t end = start + stamp_array_dimensions(editor.category_array.elements[editor.category].elements + editor.stamp);
                              for(S16 j = start.y; j < end.y; j++){
                                   for(S16 i = start.x; i < end.x; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
                                   }
                              }
                         } break;
                         case EDITOR_MODE_SELECTION_MANIPULATION:
                         {
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Rect_t selection_bounds = editor_selection_bounds(&editor);
                              for(S16 j = selection_bounds.bottom; j <= selection_bounds.top; j++){
                                   for(S16 i = selection_bounds.left; i <= selection_bounds.right; i++){
                                        Coord_t coord {i, j};
                                        coord_clear(coord, &world.tilemap, &world.interactives, world.interactive_qt, &world.blocks);
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
                         seeked_with_mouse = false;

                         switch(editor.mode){
                         default:
                              break;
                         case EDITOR_MODE_CREATE_SELECTION:
                         {
                              editor.selection_end = mouse_select_world(mouse_screen, camera);

                              sort_selection(&editor);

                              destroy(&editor.selection);

                              S16 stamp_count = (S16)((((editor.selection_end.x - editor.selection_start.x) + 1) *
                                                       ((editor.selection_end.y - editor.selection_start.y) + 1)) * 2);
                              init(&editor.selection, stamp_count);
                              S16 stamp_index = 0;
                              for(S16 j = editor.selection_start.y; j <= editor.selection_end.y; j++){
                                   for(S16 i = editor.selection_start.x; i <= editor.selection_end.x; i++){
                                        Coord_t coord = {i, j};
                                        Coord_t offset = coord - editor.selection_start;

                                        // tile id
                                        Tile_t* tile = tilemap_get_tile(&world.tilemap, coord);
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
                                        auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, coord);
                                        if(interactive){
                                             resize(&editor.selection, editor.selection.count + (S16)(1));
                                             auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                             stamp->type = STAMP_TYPE_INTERACTIVE;
                                             stamp->interactive = *interactive;
                                             stamp->offset = offset;
                                        }

                                        for(S16 b = 0; b < world.blocks.count; b++){
                                             auto* block = world.blocks.elements + b;
                                             if(pos_to_coord(block->pos) == coord){
                                                  resize(&editor.selection, editor.selection.count + (S16)(1));
                                                  auto* stamp = editor.selection.elements + (editor.selection.count - 1);
                                                  stamp->type = STAMP_TYPE_BLOCK;
                                                  // stamp->block.rotation = block->rotation;
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

                    if(seeked_with_mouse && demo.mode == DEMO_MODE_PLAY){
                         demo.seek_frame = (S64)((F32)(demo.last_frame) * mouse_screen.x);

                         if(demo.seek_frame < frame_count){
                              // TODO: compress with same comment elsewhere in this file
                              fetch_cache_for_demo_seek(&world, &demo_starting_tilemap, &demo_starting_blocks, &demo_starting_interactives);

                              setup_map(player_start, &world, &undo);

                              // reset some vars
                              player_action = {};

                              demo.entry_index = 0;
                              frame_count = 0;
                         }else if(demo.seek_frame == frame_count){
                              demo.seek_frame = -1;
                         }
                    }
                    break;
               }
          }

          if(!demo.paused || demo.seek_frame >= 0){
               // reset base light
               for(S16 j = 0; j < world.tilemap.height; j++){
                    for(S16 i = 0; i < world.tilemap.width; i++){
                         world.tilemap.tiles[j][i].light = BASE_LIGHT;
                    }
               }

               // update interactives
               for(S16 i = 0; i < world.interactives.count; i++){
                    Interactive_t* interactive = world.interactives.elements + i;
                    if(interactive->type == INTERACTIVE_TYPE_POPUP){
                         lift_update(&interactive->popup.lift, POPUP_TICK_DELAY, dt, 1, HEIGHT_INTERVAL + 1);
                    }else if(interactive->type == INTERACTIVE_TYPE_DOOR){
                         lift_update(&interactive->door.lift, POPUP_TICK_DELAY, dt, 0, DOOR_MAX_HEIGHT);
                    }else if(interactive->type == INTERACTIVE_TYPE_PRESSURE_PLATE){
                         bool should_be_down = false;
                         for(S16 p = 0; p < world.players.count; p++){
                              Coord_t player_coord = pos_to_coord(world.players.elements[p].pos);
                              if(interactive->coord == player_coord){
                                   should_be_down = true;
                                   break;
                              }
                         }

                         if(!should_be_down){
                              Tile_t* tile = tilemap_get_tile(&world.tilemap, interactive->coord);
                              if(tile){
                                   if(!tile_is_iced(tile)){
                                        Pixel_t center = coord_to_pixel(interactive->coord) + HALF_TILE_SIZE_PIXEL;
                                        Rect_t rect = {(S16)(center.x - DOUBLE_TILE_SIZE_IN_PIXELS),
                                                       (S16)(center.y - DOUBLE_TILE_SIZE_IN_PIXELS),
                                                       (S16)(center.x + DOUBLE_TILE_SIZE_IN_PIXELS),
                                                       (S16)(center.y + DOUBLE_TILE_SIZE_IN_PIXELS)};

                                        S16 block_count = 0;
                                        Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                                        quad_tree_find_in(world.block_qt, rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                        for(S16 b = 0; b < block_count; b++){
                                             Coord_t bottom_left = pixel_to_coord(blocks[b]->pos.pixel);
                                             Coord_t bottom_right = pixel_to_coord(blocks[b]->pos.pixel + Pixel_t{BLOCK_SOLID_SIZE_IN_PIXELS, 0});
                                             Coord_t top_left = pixel_to_coord(blocks[b]->pos.pixel + Pixel_t{0, BLOCK_SOLID_SIZE_IN_PIXELS});
                                             Coord_t top_right = pixel_to_coord(blocks[b]->pos.pixel + Pixel_t{BLOCK_SOLID_SIZE_IN_PIXELS, BLOCK_SOLID_SIZE_IN_PIXELS});
                                             if(interactive->coord == bottom_left ||
                                                interactive->coord == bottom_right ||
                                                interactive->coord == top_left ||
                                                interactive->coord == top_right){
                                                  should_be_down = true;
                                                  break;
                                             }
                                        }
                                   }
                              }
                         }

                         if(should_be_down != interactive->pressure_plate.down){
                              activate(&world, interactive->coord);
                              interactive->pressure_plate.down = should_be_down;
                         }
                    }
               }

               // update arrows
               for(S16 i = 0; i < ARROW_ARRAY_MAX; i++){
                    Arrow_t* arrow = world.arrows.arrows + i;
                    if(!arrow->alive) continue;

                    Coord_t pre_move_coord = pixel_to_coord(arrow->pos.pixel);

                    if(arrow->element == ELEMENT_FIRE){
                         illuminate(pre_move_coord, 255 - LIGHT_DECAY, &world);
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
                    quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);
                    for(S16 b = 0; b < block_count; b++){
                         // blocks on the coordinate and on the ground block light
                         Rect_t block_rect = block_get_rect(blocks[b]);
                         S16 block_index = (S16)(blocks[b] - world.blocks.elements);
                         S8 block_bottom = blocks[b]->pos.z;
                         S8 block_top = block_bottom + HEIGHT_INTERVAL;
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
                                             if(blocks[b]->entangle_index >= 0 && blocks[b]->entangle_index < world.blocks.count){
                                                  Block_t* entangled_block = world.blocks.elements + blocks[b]->entangle_index;
                                                  entangled_block->element = transition_element(entangled_block->element, arrow_element);
                                             }
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
                    find_portal_adjacents_to_skip_collision_check(pre_move_coord, world.interactive_qt, skip_coord);

                    if(pre_move_coord != post_move_coord){
                         bool skip = false;
                         for (auto d : skip_coord) {
                              if(post_move_coord == d) skip = true;
                         }

                         if(!skip){
                              Tile_t* tile = tilemap_get_tile(&world.tilemap, post_move_coord);
                              if(tile_is_solid(tile)){
                                   arrow->stuck_time = dt;
                              }
                         }

                         // catch or give elements
                         if(arrow->element == ELEMENT_FIRE){
                              melt_ice(post_move_coord, 0, &world);
                         }else if(arrow->element == ELEMENT_ICE){
                              spread_ice(post_move_coord, 0, &world);
                         }

                         Interactive_t* interactive = quad_tree_interactive_find_at(world.interactive_qt, post_move_coord);
                         if(interactive){
                              switch(interactive->type){
                              default:
                                   break;
                              case INTERACTIVE_TYPE_LEVER:
                                   if(arrow->pos.z >= HEIGHT_INTERVAL){
                                        activate(&world, post_move_coord);
                                   }else{
                                        arrow->stuck_time = dt;
                                   }
                                   break;
                              case INTERACTIVE_TYPE_DOOR:
                                   if(interactive->door.lift.ticks < arrow->pos.z){
                                        arrow->stuck_time = dt;
                                        // TODO: stuck in door
                                   }
                                   break;
                              case INTERACTIVE_TYPE_POPUP:
                                   if(interactive->popup.lift.ticks > arrow->pos.z){
                                        LOG("arrow z: %d, popup lift: %d\n", arrow->pos.z, interactive->popup.lift.ticks);
                                        arrow->stuck_time = dt;
                                        // TODO: stuck in popup
                                   }
                                   break;
                              case INTERACTIVE_TYPE_PORTAL:
                                   if(!interactive->portal.on){
                                        arrow->stuck_time = dt;
                                        // TODO: arrow drops if portal turns on
                                   }else if(!portal_has_destination(post_move_coord, &world.tilemap, world.interactive_qt)){
                                        // TODO: arrow drops if portal turns on
                                        arrow->stuck_time = dt;
                                   }
                                   break;
                              }
                         }

                         auto teleport_result = teleport_position_across_portal(arrow->pos, Vec_t{}, &world,
                                                                                pre_move_coord, post_move_coord);
                         if(teleport_result.count > 0){
                              arrow->pos = teleport_result.results[0].pos;
                              arrow->face = direction_rotate_clockwise(arrow->face, teleport_result.results[0].rotations);

                              // TODO: entangle
                         }
                    }
               }

               // TODO: deal with this for multiple players
               bool rotated_move_actions[4];
               for (bool &rotated_move_action : rotated_move_actions) rotated_move_action = false;

               // update player
               for(S16 i = 0; i < world.players.count; i++){
                    Player_t* player = world.players.elements + i;

                    for(int d = 0; d < 4; d++){
                         if(player_action.move[d]){
                              Direction_t rot_dir = direction_rotate_clockwise((Direction_t)(d), player->rotation);
                              rotated_move_actions[rot_dir] = true;
                         }
                    }

                    player->accel = vec_zero();

                    if(rotated_move_actions[DIRECTION_LEFT] && !rotated_move_actions[DIRECTION_RIGHT]){
                         if(player->horizontal_move.state == MOVE_STATE_IDLING){
                              player->horizontal_move.sign = MOVE_SIGN_NEGATIVE;
                              player->horizontal_move.state = MOVE_STATE_STARTING;
                              player->horizontal_move.distance = 0;
                              if(player->reface) player->face = DIRECTION_LEFT;
                         }
                    }

                    if(rotated_move_actions[DIRECTION_RIGHT] && !rotated_move_actions[DIRECTION_LEFT]){
                         if(player->horizontal_move.state == MOVE_STATE_IDLING){
                              player->horizontal_move.sign = MOVE_SIGN_POSITIVE;
                              player->horizontal_move.state = MOVE_STATE_STARTING;
                              player->horizontal_move.distance = 0;
                              if(player->reface) player->face = DIRECTION_RIGHT;
                         }
                    }

#if 0
                    for(int d = 0; d < DIRECTION_COUNT; d++){
                         if(rotated_move_actions[d]){
                              auto direction = (Direction_t)(d);
                              direction = direction_rotate_clockwise(direction, player->move_rotation[d]);
                              player->accel += direction_to_vec(direction);
                              if(player->reface) player->face = direction;
                         }
                    }
#endif

                    if(player_action.activate && !player_action.last_activate){
                         undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         activate(&world, pos_to_coord(player->pos) + player->face);
                    }

                    if(player_action.undo){
                         undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         undo_revert(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                         quad_tree_free(world.interactive_qt);
                         world.interactive_qt = quad_tree_build(&world.interactives);
                         quad_tree_free(world.block_qt);
                         world.block_qt = quad_tree_build(&world.blocks);
                         player_action.undo = false;
                    }

                    if(player->has_bow && player_action.shoot && player->bow_draw_time < PLAYER_BOW_DRAW_DELAY){
                         player->bow_draw_time += dt;
                    }else if(!player_action.shoot){
                         if(player->bow_draw_time >= PLAYER_BOW_DRAW_DELAY){
                              undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);
                              Position_t arrow_pos = player->pos;
                              switch(player->face){
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
                              arrow_spawn(&world.arrows, arrow_pos, player->face);
                         }
                         player->bow_draw_time = 0.0f;
                    }

                    if(!player_action.move[DIRECTION_LEFT] && !player_action.move[DIRECTION_RIGHT] &&
                       !player_action.move[DIRECTION_UP] && !player_action.move[DIRECTION_DOWN]){
                         player->walk_frame = 1;
                    }else{
                         player->walk_frame_time += dt;

                         if(player->walk_frame_time > PLAYER_WALK_DELAY){
                              if(vec_magnitude(player->vel) > PLAYER_IDLE_SPEED){
                                   player->walk_frame_time = 0.0f;

                                   player->walk_frame += player->walk_frame_delta;
                                   if(player->walk_frame > 2 || player->walk_frame < 0){
                                        player->walk_frame = 1;
                                        player->walk_frame_delta = -player->walk_frame_delta;
                                   }
                              }else{
                                   player->walk_frame = 1;
                                   player->walk_frame_time = 0.0f;
                              }
                         }
                    }
               }

     #if 0
               Vec_t pos_vec = pos_to_vec(player->pos);

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

               // block movement
               // do a pass moving the block as far as possible, so that collision doesn't rely on order of blocks in the array
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    Vec_t pos_delta = mass_move(&block->vel, block->accel, dt);

                    // TODO: blocks with velocity need to be checked against other blocks

                    block->pre_move_pos = block->pos;
                    block->pos += pos_delta;
               }

               // do a collision pass on each block
               S16 update_blocks_count = world.blocks.count;
               for(S16 i = 0; i < update_blocks_count; i++){
                    Block_t* block = world.blocks.elements + i;

                    bool stop_on_boundary_x = false;
                    bool stop_on_boundary_y = false;
                    bool held_up = false;

                    Vec_t pos_delta = pos_to_vec(block->pos - block->pre_move_pos);

                    if(pos_delta.x != 0.0f || pos_delta.y != 0.0f){
                         check_block_collision_with_other_blocks(block, &world);
                    }

                    // get the current coord of the center of the block
                    Pixel_t center = block->pos.pixel + HALF_TILE_SIZE_PIXEL;
                    Coord_t coord = pixel_to_coord(center);

                    Coord_t skip_coord[DIRECTION_COUNT];
                    find_portal_adjacents_to_skip_collision_check(coord, world.interactive_qt, skip_coord);

                    // check for adjacent walls
                    if(block->vel.x > 0.0f){
                         Pixel_t pixel_a {};
                         Pixel_t pixel_b {};
                         block_adjacent_pixels_to_check(block, DIRECTION_RIGHT, &pixel_a, &pixel_b);
                         Coord_t coord_a = pixel_to_coord(pixel_a);
                         Coord_t coord_b = pixel_to_coord(pixel_b);
                         if(coord_a != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&world.tilemap, coord_a)){
                              stop_on_boundary_x = true;
                         }else if(coord_b != skip_coord[DIRECTION_RIGHT] && tilemap_is_solid(&world.tilemap, coord_b)){
                              stop_on_boundary_x = true;
                         }else{
                              stop_on_boundary_x = quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_a) ||
                                                   quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_b);
                         }
                    }else if(block->vel.x < 0.0f){
                         Pixel_t pixel_a {};
                         Pixel_t pixel_b {};
                         block_adjacent_pixels_to_check(block, DIRECTION_LEFT, &pixel_a, &pixel_b);
                         Coord_t coord_a = pixel_to_coord(pixel_a);
                         Coord_t coord_b = pixel_to_coord(pixel_b);
                         if(coord_a != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&world.tilemap, coord_a)){
                              stop_on_boundary_x = true;
                         }else if(coord_b != skip_coord[DIRECTION_LEFT] && tilemap_is_solid(&world.tilemap, coord_b)){
                              stop_on_boundary_x = true;
                         }else{
                              stop_on_boundary_x = quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_a) ||
                                                   quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_b);
                         }
                    }

                    if(block->vel.y > 0.0f){
                         Pixel_t pixel_a {};
                         Pixel_t pixel_b {};
                         block_adjacent_pixels_to_check(block, DIRECTION_UP, &pixel_a, &pixel_b);
                         Coord_t coord_a = pixel_to_coord(pixel_a);
                         Coord_t coord_b = pixel_to_coord(pixel_b);
                         if(coord_a != skip_coord[DIRECTION_UP] && tilemap_is_solid(&world.tilemap, coord_a)){
                              stop_on_boundary_y = true;
                         }else if(coord_b != skip_coord[DIRECTION_UP] && tilemap_is_solid(&world.tilemap, coord_b)){
                              stop_on_boundary_y = true;
                         }else{
                              stop_on_boundary_y = quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_a) ||
                                                   quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_b);
                         }
                    }else if(block->vel.y < 0.0f){
                         Pixel_t pixel_a {};
                         Pixel_t pixel_b {};
                         block_adjacent_pixels_to_check(block, DIRECTION_DOWN, &pixel_a, &pixel_b);
                         Coord_t coord_a = pixel_to_coord(pixel_a);
                         Coord_t coord_b = pixel_to_coord(pixel_b);
                         if(coord_a != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&world.tilemap, coord_a)){
                              stop_on_boundary_y = true;
                         }else if(coord_b != skip_coord[DIRECTION_DOWN] && tilemap_is_solid(&world.tilemap, coord_b)){
                              stop_on_boundary_y = true;
                         }else{
                              stop_on_boundary_y = quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_a) ||
                                                   quad_tree_interactive_solid_at(world.interactive_qt, &world.tilemap, coord_b);
                         }
                    }

                    // TODO: I think we may want to keep track of more of these and resolve multiple directions as well?
                    // used to make sure the pushing is smooth on the entangled block as well
                    Block_t* entangled_block_pushed = nullptr;
                    Direction_t entangled_block_pushed_dir = DIRECTION_COUNT;
                    Block_t* block_pushed = nullptr;

                    for(S16 p = 0; p < world.players.count; p++){
                         Player_t* player = world.players.elements + p;
                         if(player->pushing_block >= 0){
                              block_pushed = world.blocks.elements + player->pushing_block;
                              if(block_pushed && block_pushed->entangle_index >= 0 && block_pushed->entangle_index < world.blocks.count){
                                   entangled_block_pushed = world.blocks.elements + block_pushed->entangle_index;
                                   entangled_block_pushed_dir = player->pushing_block_dir;
                              }
                         }
                    }

                    // this instance of last_block_pushed is to keep the pushing smooth and not have it stop at the tile boundaries
                    if(block != block_pushed && !block_on_ice(block, &world.tilemap, world.interactive_qt)){
                         if(block == entangled_block_pushed){
                              // TODO: take into account block rotation
                              DirectionMask_t vel_mask = vec_direction_mask(block->vel);
                              switch(entangled_block_pushed_dir){
                              default:
                                   break;
                              case DIRECTION_LEFT:
                                   if(vel_mask & DIRECTION_MASK_RIGHT){
                                        stop_on_boundary_x = true;
                                   }else if(vel_mask & DIRECTION_MASK_UP || vel_mask & DIRECTION_MASK_DOWN){
                                        stop_on_boundary_y = true;
                                   }
                                   break;
                              case DIRECTION_RIGHT:
                                   if(vel_mask & DIRECTION_MASK_LEFT){
                                        stop_on_boundary_x = true;
                                   }else if(vel_mask & DIRECTION_MASK_UP || vel_mask & DIRECTION_MASK_DOWN){
                                        stop_on_boundary_y = true;
                                   }
                                   break;
                              case DIRECTION_UP:
                                   if(vel_mask & DIRECTION_MASK_DOWN){
                                        stop_on_boundary_y = true;
                                   }else if(vel_mask & DIRECTION_MASK_LEFT || vel_mask & DIRECTION_MASK_RIGHT){
                                        stop_on_boundary_x = true;
                                   }
                                   break;
                              case DIRECTION_DOWN:
                                   if(vel_mask & DIRECTION_MASK_UP){
                                        stop_on_boundary_y = true;
                                   }else if(vel_mask & DIRECTION_MASK_LEFT || vel_mask & DIRECTION_MASK_RIGHT){
                                        stop_on_boundary_x = true;
                                   }
                                   break;
                              }
                         }else{
                              stop_on_boundary_x = true;
                              stop_on_boundary_y = true;
                         }
                    }

                    if(stop_on_boundary_x){
                         // stop on tile boundaries separately for each axis
                         S16 boundary_x = range_passes_tile_boundary(block->pre_move_pos.pixel.x, block->pos.pixel.x, block->push_start.x);
                         if(boundary_x){
                              block->pos.pixel.x = boundary_x;
                              block->pos.decimal.x = 0.0f;
                              block->vel.x = 0.0f;
                              block->accel.x = 0.0f;
                         }
                    }

                    if(stop_on_boundary_y){
                         S16 boundary_y = range_passes_tile_boundary(block->pre_move_pos.pixel.y, block->pos.pixel.y, block->push_start.y);
                         if(boundary_y){
                              block->pos.pixel.y = boundary_y;
                              block->pos.decimal.y = 0.0f;
                              block->vel.y = 0.0f;
                              block->accel.y = 0.0f;
                         }
                    }

                    held_up = block_held_up_by_another_block(block, world.block_qt);

                    // TODO: should we care about the decimal component of the position ?
                    auto* interactive = quad_tree_interactive_find_at(world.interactive_qt, coord);
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

                    coord = pixel_to_coord(block->pos.pixel + HALF_TILE_SIZE_PIXEL);
                    Coord_t premove_coord = pixel_to_coord(block->pre_move_pos.pixel + HALF_TILE_SIZE_PIXEL);

                    Position_t block_center = block->pos;
                    block_center.pixel += HALF_TILE_SIZE_PIXEL;

                    auto teleport_result = teleport_position_across_portal(block_center, Vec_t{}, &world, premove_coord,
                                                                           coord);
                    if(teleport_result.count > block->clone_id){
                         block->pos = teleport_result.results[block->clone_id].pos;
                         block->pos.pixel -= HALF_TILE_SIZE_PIXEL;

                         block->vel = vec_rotate_quadrants_clockwise(block->vel, teleport_result.results[block->clone_id].rotations);
                         block->accel = vec_rotate_quadrants_clockwise(block->accel, teleport_result.results[block->clone_id].rotations);
                         block->rotation = teleport_result.results[block->clone_id].rotations;

                         block->pre_move_pos = block->pos;

                         check_block_collision_with_other_blocks(block, &world);

                         // try teleporting if we collided with a block
                         premove_coord = pixel_to_coord(block_center.pixel + HALF_TILE_SIZE_PIXEL);
                         coord = pixel_to_coord(block->pos.pixel + HALF_TILE_SIZE_PIXEL);

                         block_center = block_get_center(block);

                         auto collided_teleport_result = teleport_position_across_portal(block_center, Vec_t{}, &world,
                                                                                         premove_coord, coord);
                         if(collided_teleport_result.count > block->clone_id){
                              block->pos = collided_teleport_result.results[block->clone_id].pos;
                              block->pos.pixel -= HALF_TILE_SIZE_PIXEL;

                              block->vel = vec_rotate_quadrants_clockwise(block->vel, collided_teleport_result.results[block->clone_id].rotations);
                              block->accel = vec_rotate_quadrants_clockwise(block->accel, collided_teleport_result.results[block->clone_id].rotations);
                              block->rotation = collided_teleport_result.results[block->clone_id].rotations;
                         }
                    }

                    auto* portal = block_is_teleporting(block, world.interactive_qt);

                    // is the block teleporting and it hasn't been cloning ?
                    if(portal && block->clone_start.x == 0){
                         // at the first instant of the block teleporting, check if we should create an entangled_block

                         PortalExit_t portal_exits = find_portal_exits(portal->coord, &world.tilemap, world.interactive_qt);
                         S8 clone_id = 0;
                         for (auto &direction : portal_exits.directions) {
                              for(int p = 0; p < direction.count; p++){
                                   if(direction.coords[p] == portal->coord) continue;

                                   if(clone_id == 0){
                                        block->clone_id = clone_id;
                                   }else{
                                        S16 new_block_index = world.blocks.count;
                                        S16 old_block_index = (S16)(block - world.blocks.elements);

                                        if(resize(&world.blocks, world.blocks.count + (S16)(1))){
                                             // update quad tree now that we have resized the world
                                             quad_tree_free(world.block_qt);
                                             world.block_qt = quad_tree_build(&world.blocks);

                                             // a resize will kill our block ptr, so we gotta update it
                                             block = world.blocks.elements + old_block_index;
                                             block->clone_start = portal->coord;

                                             Block_t* entangled_block = world.blocks.elements + new_block_index;

                                             *entangled_block = *block;
                                             entangled_block->clone_id = clone_id;

                                             // the magic
                                             entangled_block->entangle_index = old_block_index;
                                             block->entangle_index = new_block_index;
                                        }
                                   }

                                   clone_id++;
                              }
                         }

                    // if we didn't find a portal but we have been cloning, we are done cloning! We either need to kill the clone or complete it
                    }else if(!portal && block->clone_start.x > 0 &&
                             block->entangle_index < world.blocks.count){
                         // TODO: do we need to update the block quad tree here?
                         auto block_move_dir = vec_direction(pos_delta);
                         auto block_from_coord = block_get_coord(block) - block_move_dir;
                         if(block_from_coord == block->clone_start){
                              // in this instance, despawn the clone
                              // NOTE: I think this relies on new entangle blocks being
                              S16 block_index = (S16)(block - world.blocks.elements);
                              remove(&world.blocks, block->entangle_index);

                              // TODO: This could lead to a subtle bug where our block index is no longer correct
                              // TODO: because this was the last index in the list which got swapped to our index
                              // update ptr since we could have resized
                              block = world.blocks.elements + block_index;

                              update_blocks_count--;

                              // if our entangled block was not the last element, then it was replaced by another
                              if(block->entangle_index < world.blocks.count){
                                   Block_t* replaced_block = world.blocks.elements + block->entangle_index;
                                   if(replaced_block->entangle_index >= 0){
                                        // update the block's entangler that moved into our entangled blocks spot
                                        Block_t* replaced_block_entangler = world.blocks.elements + replaced_block->entangle_index;
                                        replaced_block_entangler->entangle_index = (S16)(replaced_block - world.blocks.elements);
                                   }
                              }

                              block->entangle_index = -1;
                         }else{
                              assert(block->entangle_index >= 0);

                              // great news everyone, the clone was a success
                              Block_t* entangled_block = world.blocks.elements + block->entangle_index;

                              block->clone_id = 0;
                              entangled_block->clone_id = 0;
                              entangled_block->clone_start = Coord_t{};

                              // turn off the circuit
                              activate(&world, block->clone_start);
                              auto* src_portal = quad_tree_find_at(world.interactive_qt, block->clone_start.x, block->clone_start.y);
                              if(is_active_portal(src_portal)){
                                   src_portal->portal.on = false;
                              }
                         }

                         block->clone_start = Coord_t{};
                    }
               }

               // illuminate and ice
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    if(block->element == ELEMENT_FIRE){
                         illuminate(block_get_coord(block), 255, &world);
                    }else if(block->element == ELEMENT_ICE){
                         auto block_coord = block_get_coord(block);
                         spread_ice(block_coord, 1, &world);
                    }
               }

               // melt ice
               for(S16 i = 0; i < world.blocks.count; i++){
                    Block_t* block = world.blocks.elements + i;
                    if(block->element == ELEMENT_FIRE){
                         melt_ice(block_get_coord(block), 1, &world);
                    }
               }

               for(S16 i = 0; i < world.interactives.count; i++){
                    Interactive_t* interactive = world.interactives.elements + i;
                    if(interactive->type == INTERACTIVE_TYPE_LIGHT_DETECTOR){
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, interactive->coord);
                         Rect_t coord_rect = rect_surrounding_adjacent_coords(interactive->coord);

                         S16 block_count = 0;
                         Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                         quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                         Block_t* block = nullptr;
                         for(S16 b = 0; b < block_count; b++){
                              // blocks on the coordinate and on the ground block light
                              if(block_get_coord(blocks[b]) == interactive->coord && blocks[b]->pos.z == 0){
                                   block = blocks[b];
                                   break;
                              }
                         }

                         if(interactive->detector.on && (tile->light < LIGHT_DETECTOR_THRESHOLD || block)){
                              activate(&world, interactive->coord);
                              interactive->detector.on = false;
                         }else if(!interactive->detector.on && tile->light >= LIGHT_DETECTOR_THRESHOLD && !block){
                              activate(&world, interactive->coord);
                              interactive->detector.on = true;
                         }
                    }else if(interactive->type == INTERACTIVE_TYPE_ICE_DETECTOR){
                         Tile_t* tile = tilemap_get_tile(&world.tilemap, interactive->coord);
                         if(tile){
                              if(interactive->detector.on && !tile_is_iced(tile)){
                                   activate(&world, interactive->coord);
                                   interactive->detector.on = false;
                              }else if(!interactive->detector.on && tile_is_iced(tile)){
                                   activate(&world, interactive->coord);
                                   interactive->detector.on = true;
                              }
                         }
                    }
               }

               // player movement
               S16 update_player_count = world.players.count; // save due to adding/removing players
               for(S16 i = 0; i < update_player_count; i++){
                    Player_t* player = world.players.elements + i;

#if 0
                    player->accel = vec_normalize(player->accel);
                    player->accel = player->accel * PLAYER_SPEED;

                    Vec_t pos_delta = mass_move(&player->vel, player->accel, dt);

                    if(fabs(vec_magnitude(player->vel)) > PLAYER_SPEED){
                         player->vel = vec_normalize(player->vel) * PLAYER_SPEED;
                    }
#else
                    player->accel.x = calc_accel_component_move(player->horizontal_move, PLAYER_ACCEL);

                    player->prev_vel = player->vel;
                    player->pos_delta.x = calc_position_motion(player->vel.x, player->accel.x, dt);
                    player->vel.x = calc_velocity_motion(player->vel.x, player->accel.x, dt);

                    Vec_t pos_delta = player->pos_delta;

                    update_motion_free_form(&player->horizontal_move, motion_x_component(player),
                                            rotated_move_actions[DIRECTION_RIGHT], rotated_move_actions[DIRECTION_LEFT],
                                            dt, PLAYER_ACCEL, PLAYER_ACCEL_DISTANCE);
#endif

                    Coord_t skip_coord[DIRECTION_COUNT];
                    Coord_t player_previous_coord = pos_to_coord(player->pos);
                    Coord_t player_coord = pos_to_coord(player->pos + pos_delta);

                    find_portal_adjacents_to_skip_collision_check(player_coord, world.interactive_qt, skip_coord);
                    auto teleport_result = teleport_position_across_portal(player->pos, pos_delta, &world,
                                                                           player_previous_coord, player_coord);
                    auto teleport_clone_id = player->clone_id;
                    if(player_coord != player->clone_start){
                         // if we are going back to our clone portal, then all clones should go back

                         // find the index closest to our original clone portal
                         F32 shortest_distance = FLT_MAX;
                         auto clone_start_center = coord_to_pixel_at_center(player->clone_start);

                         for(S8 t = 0; t < teleport_result.count; t++){
                              F32 distance = pixel_distance_between(clone_start_center, teleport_result.results[t].pos.pixel);
                              if(distance < shortest_distance){
                                   shortest_distance = distance;
                                   teleport_clone_id = t;
                              }
                         }
                    }
                    if(teleport_result.count > teleport_clone_id){
                         player->pos = teleport_result.results[teleport_clone_id].pos;
                         pos_delta = teleport_result.results[teleport_clone_id].delta;
                    }

                    player->pushing_block = -1;
                    bool collide_with_interactive = false;
                    Vec_t player_delta_pos = move_player_position_through_world(player->pos, pos_delta,
                                                                                player->face, skip_coord,
                                                                                player, &world,
                                                                                &collide_with_interactive, &resetting);

                    player_coord = pos_to_coord(player->pos + player_delta_pos);

                    if(player->pushing_block >= 0){
                         Block_t* block_to_push = world.blocks.elements + player->pushing_block;
                         DirectionMask_t block_move_dir_mask = vec_direction_mask(block_to_push->vel);
                         if(direction_in_mask(direction_mask_opposite(block_move_dir_mask), player->pushing_block_dir))
                         {
                              // if the player is pushing against a block moving towards them, the block wins
                              player->push_time = 0;
                              player->pushing_block = -1;
                         }else{
                              F32 save_push_time = player->push_time;

                              // TODO: get back to this once we improve our demo tools
                              player->push_time += dt;
                              if(player->push_time > BLOCK_PUSH_TIME){

                                   // if this is the frame that causes the block to be pushed, make a commit
                                   if(save_push_time <= BLOCK_PUSH_TIME) undo_commit(&undo, &world.players, &world.tilemap, &world.blocks, &world.interactives);

                                   bool pushed = block_push(block_to_push, player->pushing_block_dir, &world, false);
                                   if(pushed && block_to_push->entangle_index >= 0 && block_to_push->entangle_index < world.blocks.count){
                                        // TODO: take into account block_to_push->face to rotate face
                                        Block_t* entangled_block = world.blocks.elements + block_to_push->entangle_index;
                                        U8 push_rotation = 0;
                                        if(block_to_push->rotation){
                                             push_rotation = block_to_push->rotation;
                                        }else if(entangled_block->rotation){
                                             push_rotation = entangled_block->rotation;
                                        }
                                        Direction_t rotated_dir = direction_rotate_clockwise(player->pushing_block_dir, push_rotation);
                                        block_push(entangled_block, rotated_dir, &world, false);
                                   }

                                   if(block_to_push->pos.z > 0) player->push_time = -0.5f; // TODO: wtf is this line?
                              }
                         }
                    }else{
                         player->push_time = 0;
                    }

                    if(teleport_result.count == 0){
                         teleport_result = teleport_position_across_portal(player->pos, player_delta_pos, &world,
                                                                           player_previous_coord, player_coord);
                         if(teleport_result.count > teleport_clone_id){
                              player->pos = teleport_result.results[teleport_clone_id].pos;
                              player_delta_pos = teleport_result.results[teleport_clone_id].delta;
                         }
                    }else{
                         auto new_teleport_result = teleport_position_across_portal(player->pos, player_delta_pos, &world,
                                                                                    player_previous_coord, player_coord);
                         if(new_teleport_result.count > teleport_clone_id){
                              player->pos = new_teleport_result.results[teleport_clone_id].pos;
                              player_delta_pos = new_teleport_result.results[teleport_clone_id].delta;
                         }
                    }

                    if(teleport_result.count > teleport_clone_id){
                         player->face = direction_rotate_clockwise(player->face, teleport_result.results[teleport_clone_id].rotations);
                         player->vel = vec_rotate_quadrants_clockwise(player->vel, teleport_result.results[teleport_clone_id].rotations);
                         player->accel = vec_rotate_quadrants_clockwise(player->accel, teleport_result.results[teleport_clone_id].rotations);

                         if(i != 0) player->rotation = (player->rotation + teleport_result.results[teleport_clone_id].rotations) % DIRECTION_COUNT;

                         // set rotations for each direction the player wants to move
                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              if(player_action.move[d]) player->move_rotation[d] = (player->move_rotation[d] + teleport_result.results[teleport_clone_id].rotations) % DIRECTION_COUNT;
                         }
                    }

                    auto* portal = player_is_teleporting(player, world.interactive_qt);

                    player->pos += player_delta_pos;

                    if(portal && player->clone_start.x == 0){
                         // at the first instant of the block teleporting, check if we should create an entangled_block

                         PortalExit_t portal_exits = find_portal_exits(portal->coord, &world.tilemap, world.interactive_qt);
                         S8 count = portal_exit_count(&portal_exits);
                         if(count >= 3){ // src portal, dst portal, clone portal
                              world.clone_instance++;

                              S8 clone_id = 0;
                              for (auto &direction : portal_exits.directions) {
                                   for(int p = 0; p < direction.count; p++){
                                        if(direction.coords[p] == portal->coord) continue;

                                        if(clone_id == 0){
                                             player->clone_id = clone_id;
                                             player->clone_instance = world.clone_instance;
                                        }else{
                                             S16 new_player_index = world.players.count;

                                             if(resize(&world.players, world.players.count + (S16)(1))){
                                                  // a resize will kill our player ptr, so we gotta update it
                                                  player = world.players.elements + i;
                                                  player->clone_start = portal->coord;

                                                  Player_t* new_player = world.players.elements + new_player_index;
                                                  *new_player = *player;
                                                  new_player->clone_id = clone_id;
                                             }
                                        }

                                        clone_id++;
                                   }
                              }
                         }
                    }else if(!portal && player->clone_start.x > 0){
                         auto clone_portal_center = coord_to_pixel_at_center(player->clone_start);
                         F64 player_distance_from_portal = pixel_distance_between(clone_portal_center, player->pos.pixel);
                         bool from_clone_start = (player_distance_from_portal < TILE_SIZE_IN_PIXELS);

                         if(from_clone_start){
                              // loop across all players after this one
                              for(S16 p = 0; p < world.players.count; p++){
                                   if(p == i) continue;
                                   Player_t* other_player = world.players.elements + p;
                                   if(other_player->clone_instance == player->clone_instance){
                                        // TODO: I think I may have a really subtle bug here where we actually move
                                        // TODO: the i'th player around because it was the last in the array
                                        remove(&world.players, p);

                                        // update ptr since we could have resized
                                        player = world.players.elements + i;

                                        update_player_count--;
                                   }
                              }
                         }else{
                              for(S16 p = 0; p < world.players.count; p++){
                                   if(p == i) continue;
                                   Player_t* other_player = world.players.elements + p;
                                   if(other_player->clone_instance == player->clone_instance){
                                        other_player->clone_id = 0;
                                        other_player->clone_instance = 0;
                                        other_player->clone_start = Coord_t{};
                                   }
                              }

                              // turn off the circuit
                              activate(&world, player->clone_start);
                              auto* src_portal = quad_tree_find_at(world.interactive_qt, player->clone_start.x, player->clone_start.y);
                              if(is_active_portal(src_portal)) src_portal->portal.on = false;
                         }

                         player->clone_id = 0;
                         player->clone_instance = 0;
                         player->clone_start = Coord_t{};
                    }

                    Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, player_coord.x, player_coord.y);
                    if(interactive && interactive->type == INTERACTIVE_TYPE_CLONE_KILLER){
                         if(i == 0){
                              resize(&world.players, 1);
                              update_player_count = 1;
                         }else{
                              resetting = true;
                         }
                    }
               }

               if(resetting){
                    reset_timer += dt;
                    if(reset_timer >= RESET_TIME){
                         resetting = false;

                         load_map_number_map(map_number, &world, &undo, &player_start, &player_action);
                    }
               }else{
                    reset_timer -= dt;
                    if(reset_timer <= 0) reset_timer = 0;
               }
          }

          if((suite && !show_suite) || demo.seek_frame >= 0) continue;

          glClear(GL_COLOR_BUFFER_BIT);

          Position_t screen_camera = camera - Vec_t{0.5f, 0.5f} + Vec_t{HALF_TILE_SIZE, HALF_TILE_SIZE};

          Coord_t min = pos_to_coord(screen_camera);
          Coord_t max = min + Coord_t{ROOM_TILE_SIZE, ROOM_TILE_SIZE};
          min = coord_clamp_zero_to_dim(min, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));
          max = coord_clamp_zero_to_dim(max, world.tilemap.width - (S16)(1), world.tilemap.height - (S16)(1));
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

                    Tile_t* tile = world.tilemap.tiles[y] + x;
                    Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, x, y);
                    if(is_active_portal(interactive)){
                         Coord_t coord {x, y};
                         PortalExit_t portal_exits = find_portal_exits(coord, &world.tilemap, world.interactive_qt);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                                   Tile_t* portal_tile = world.tilemap.tiles[portal_coord.y] + portal_coord.x;
                                   Interactive_t* portal_interactive = quad_tree_find_at(world.interactive_qt, portal_coord.x, portal_coord.y);
                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                   draw_flats(tile_pos, portal_tile, portal_interactive, theme_texture, portal_rotations);
                              }
                         }
                    }else{
                         draw_flats(tile_pos, tile, interactive, theme_texture, 0);
                    }
               }
          }

          bool* draw_players = (bool*)malloc((size_t)(world.players.count));

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    Coord_t coord {x, y};
                    Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, coord.x, coord.y);

                    if(is_active_portal(interactive)){
                         PortalExit_t portal_exits = find_portal_exits(coord, &world.tilemap, world.interactive_qt);

                         for(S8 d = 0; d < DIRECTION_COUNT; d++){
                              for(S8 i = 0; i < portal_exits.directions[d].count; i++){
                                   if(portal_exits.directions[d].coords[i] == coord) continue;
                                   Coord_t portal_coord = portal_exits.directions[d].coords[i] + direction_opposite((Direction_t)(d));
                                   Rect_t coord_rect = rect_surrounding_coord(portal_coord);

                                   S16 block_count = 0;
                                   Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];

                                   quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                                   Interactive_t* portal_interactive = quad_tree_find_at(world.interactive_qt, portal_coord.x, portal_coord.y);

                                   U8 portal_rotations = portal_rotations_between((Direction_t)(d), interactive->portal.face);
                                   Pixel_t portal_center_pixel = coord_to_pixel_at_center(portal_coord);
                                   for(S16 p = 0; p < world.players.count; p++){
                                        draw_players[p] = (pixel_distance_between(portal_center_pixel, world.players.elements[p].pos.pixel) <= 20);
                                   }

                                   draw_solids(tile_pos, portal_interactive, blocks, block_count, &world.players, draw_players,
                                               screen_camera, theme_texture, player_texture, portal_coord, coord,
                                               portal_rotations, &world.tilemap, world.interactive_qt);
                              }
                         }

                         draw_interactive(interactive, tile_pos, coord, &world.tilemap, world.interactive_qt);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 x = min.x; x <= max.x; x++){
                    Tile_t* tile = world.tilemap.tiles[y] + x;
                    if(tile && tile->id >= 16){
                         Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                         (F32)(y - min.y) * TILE_SIZE + camera_offset.y};
                         draw_tile_id(tile->id, tile_pos);
                    }
               }
          }

          for(S16 y = max.y; y >= min.y; y--){
               for(S16 p = 0; p < world.players.count; p++){
                    draw_players[p] = (pos_to_coord(world.players.elements[p].pos).y == y);
               }

               for(S16 x = min.x; x <= max.x; x++){
                    Coord_t coord {x, y};
                    Rect_t coord_rect = rect_surrounding_adjacent_coords(coord);
                    Vec_t tile_pos {(F32)(x - min.x) * TILE_SIZE + camera_offset.x,
                                    (F32)(y - min.y) * TILE_SIZE + camera_offset.y};

                    S16 block_count = 0;
                    Block_t* blocks[BLOCK_QUAD_TREE_MAX_QUERY];
                    quad_tree_find_in(world.block_qt, coord_rect, blocks, &block_count, BLOCK_QUAD_TREE_MAX_QUERY);

                    Interactive_t* interactive = quad_tree_find_at(world.interactive_qt, x, y);

                    draw_solids(tile_pos, interactive, blocks, block_count, &world.players, draw_players, screen_camera,
                                theme_texture, player_texture, coord, Coord_t{-1, -1}, 0, &world.tilemap, world.interactive_qt);

               }

               // draw arrows
               static Vec_t arrow_tip_offset[DIRECTION_COUNT] = {
                    {0.0f,               9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  16.0f * PIXEL_SIZE},
                    {16.0f * PIXEL_SIZE, 9.0f * PIXEL_SIZE},
                    {8.0f * PIXEL_SIZE,  0.0f * PIXEL_SIZE},
               };

               for(S16 a = 0; a < ARROW_ARRAY_MAX; a++){
                    Arrow_t* arrow = world.arrows.arrows + a;
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
                    Vec_t dim {TILE_SIZE, TILE_SIZE};
                    Vec_t tex_dim {ARROW_FRAME_WIDTH, ARROW_FRAME_HEIGHT};
                    draw_screen_texture(arrow_vec, tex_vec, dim, tex_dim);

                    arrow_vec.y += (arrow->pos.z * PIXEL_SIZE);

                    S8 y_frame = 0;
                    if(arrow->element) y_frame = (S8)(2 + ((arrow->element - 1) * 4));

                    tex_vec = arrow_frame(arrow->face, y_frame);
                    draw_screen_texture(arrow_vec, tex_vec, dim, tex_dim);

                    glEnd();

                    glBindTexture(GL_TEXTURE_2D, theme_texture);
                    glBegin(GL_QUADS);
                    glColor3f(1.0f, 1.0f, 1.0f);
               }
          }

          free(draw_players);

          glEnd();

          glBindTexture(GL_TEXTURE_2D, 0);
          glBegin(GL_LINES);

          for(S16 p = 0; p < world.players.count; p++){
               // player circle
               Position_t player_camera_offset = world.players.elements[p].pos - screen_camera;
               Vec_t pos_vec = pos_to_vec(player_camera_offset);

               if(world.players.elements[p].pushing_block >= 0){
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

               pos_vec = pos_to_vec(Position_t{} - screen_camera);
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
                    Tile_t* tile = world.tilemap.tiles[y] + x;

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
                              Block_t block = block_from_stamp(stamp);
                              draw_block(&block, vec);
                         } break;
                         case STAMP_TYPE_INTERACTIVE:
                         {
                              draw_interactive(&stamp->interactive, vec, Coord_t{-1, -1}, &world.tilemap, world.interactive_qt);
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
                         Block_t block = block_from_stamp(stamp);
                         draw_block(&block, stamp_pos);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         draw_interactive(&stamp->interactive, stamp_pos, Coord_t{-1, -1}, &world.tilemap, world.interactive_qt);
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
                                   Block_t block = block_from_stamp(stamp);
                                   draw_block(&block, stamp_vec);
                              } break;
                              case STAMP_TYPE_INTERACTIVE:
                              {
                                   draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &world.tilemap, world.interactive_qt);
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
                         Block_t block = block_from_stamp(stamp);
                         draw_block(&block, stamp_vec);
                    } break;
                    case STAMP_TYPE_INTERACTIVE:
                    {
                         draw_interactive(&stamp->interactive, stamp_vec, Coord_t{-1, -1}, &world.tilemap, world.interactive_qt);
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

          if(reset_timer >= 0.0f){
               glBegin(GL_QUADS);
               glColor4f(0.0f, 0.0f, 0.0f, reset_timer / RESET_TIME);
               glVertex2f(0, 0);
               glVertex2f(0, 1);
               glVertex2f(1, 1);
               glVertex2f(1, 0);
               glEnd();
          }

          if(world.players.count >= 1){
               Player_t* player = world.players.elements + 0;

               glBegin(GL_QUADS);

               if(player_is_teleporting(player, world.interactive_qt)){
                    glColor3f(0.0f, 1.0f, 0.0f);
               }else{
                    glColor3f(1.0f, 0.0f, 0.0f);
               }

               glVertex2f(0.02, 0.02);
               glVertex2f(0.02, 0.04);
               glVertex2f(0.04, 0.04);
               glVertex2f(0.04, 0.02);
               glEnd();
          }

          if(demo.mode == DEMO_MODE_PLAY){
               F32 demo_pct = (F32)(frame_count) / (F32)(demo.last_frame);
               Quad_t pct_bar_quad = {pct_bar_outline_quad.left, pct_bar_outline_quad.bottom, demo_pct, pct_bar_outline_quad.top};
               draw_quad_filled(&pct_bar_quad, 255.0f, 255.0f, 255.0f);
               draw_quad_wireframe(&pct_bar_outline_quad, 255.0f, 255.0f, 255.0f);
          }

          SDL_GL_SwapWindow(window);
     }

     switch(demo.mode){
     default:
          break;
     case DEMO_MODE_RECORD:
          player_action_perform(&player_action, &world.players, PLAYER_ACTION_TYPE_END_DEMO, demo.mode, demo.file, frame_count);
          // save map and player position
          save_map_to_file(demo.file, player_start, &world.tilemap, &world.blocks, &world.interactives);
          switch(demo.version){
          default:
               break;
          case 1:
               fwrite(&world.players.elements->pos.pixel, sizeof(world.players.elements->pos.pixel), 1, demo.file);
               break;
          case 2:
          {
               fwrite(&world.players.count, sizeof(world.players.count), 1, demo.file);
               for(S16 p = 0; p < world.players.count; p++){
                    Player_t* player = world.players.elements + p;
                    fwrite(&player->pos.pixel, sizeof(player->pos.pixel), 1, demo.file);
               }
          } break;
          }
          fclose(demo.file);
          break;
     case DEMO_MODE_PLAY:
          fclose(demo.file);
          break;
     }

     quad_tree_free(world.interactive_qt);
     quad_tree_free(world.block_qt);

     destroy(&world.blocks);
     destroy(&world.interactives);
     destroy(&undo);
     destroy(&world.tilemap);
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
