#pragma once

#include "direction.h"
#include "coord.h"
#include "axis_line.h"

struct Lift_t{
     U8 ticks; // start at 1
     bool up;
     F32 timer;
};

void lift_update(Lift_t* lift, float tick_delay, float dt, S8 min_tick, S8 max_tick);

#define POPUP_TICK_DELAY 0.1f
#define POPUP_MAX_LIFT_TICKS (HEIGHT_INTERVAL + 1)

enum InteractiveType_t{
     INTERACTIVE_TYPE_NONE,
     INTERACTIVE_TYPE_PRESSURE_PLATE,
     INTERACTIVE_TYPE_LIGHT_DETECTOR,
     INTERACTIVE_TYPE_ICE_DETECTOR,
     INTERACTIVE_TYPE_POPUP,
     INTERACTIVE_TYPE_LEVER,
     INTERACTIVE_TYPE_DOOR,
     INTERACTIVE_TYPE_PORTAL,
     INTERACTIVE_TYPE_BOMB,
     INTERACTIVE_TYPE_BOW,
     INTERACTIVE_TYPE_STAIRS,
     INTERACTIVE_TYPE_CHECKPOINT,
     INTERACTIVE_TYPE_WIRE_CROSS,
     INTERACTIVE_TYPE_CLONE_KILLER,
     INTERACTIVE_TYPE_PIT,
};

struct PressurePlate_t{
     bool down;
     bool iced_under;
};

struct Detector_t{
     bool on;
};

struct Popup_t{
     Lift_t lift;
     bool iced;
};

struct Stairs_t{
     bool up;
     Direction_t face;
     U8 exit_index;
};

struct Lever_t{
     Direction_t activated_from;
     S8 ticks;
     F32 timer;
};

#define DOOR_MAX_HEIGHT 7

struct Door_t{
     Lift_t lift;
     Direction_t face;
};

struct Portal_t{
     Direction_t face;
     bool on;
     bool has_block_inside;
     bool wants_to_turn_off;
};

struct WireCross_t{
     DirectionMask_t mask;
     bool on; // can hold a different state than the tile wire state flag
};

struct Pit_t{
     S8 id; // just a visual display thing
     bool iced;
};

struct Interactive_t{
     InteractiveType_t type;
     Coord_t coord;

     union{
          PressurePlate_t pressure_plate;
          Detector_t detector;
          Popup_t popup;
          Stairs_t stairs;
          Lever_t lever;
          Door_t door;
          Portal_t portal;
          WireCross_t wire_cross;
          Pit_t pit;
          bool checkpoint;
     };
};

bool interactive_is_solid(const Interactive_t* interactive);
S16 get_object_x(const Interactive_t* interactive);
S16 get_object_y(const Interactive_t* interactive);
bool interactive_equal(const Interactive_t* a, const Interactive_t* b);

bool is_active_portal(const Interactive_t* interactive);
AxisLine_t get_portal_line(const Interactive_t* interactive);
