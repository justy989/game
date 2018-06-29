#pragma once

#include "types.h"

enum Element_t : U8{
     ELEMENT_NONE,
     ELEMENT_FIRE,
     ELEMENT_ICE,
     ELEMENT_ONLY_ICED,
     ELEMENT_COUNT
};

Element_t transition_element(Element_t a, Element_t b);
Element_t next_element(Element_t e);
const char* element_to_string(Element_t e);
