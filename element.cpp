#include "element.h"
#include "defines.h"

Element_t transition_element(Element_t a, Element_t b){
     Element_t e = ELEMENT_NONE;

     switch(a){
     default:
          break;
     case ELEMENT_NONE:
          switch(b){
          default:
          case ELEMENT_NONE:
               break;
          case ELEMENT_FIRE:
               e = ELEMENT_FIRE;
               break;
          case ELEMENT_ICE:
               e = ELEMENT_ICE;
               break;
          }
          break;
     case ELEMENT_FIRE:
          switch(b){
          default:
          case ELEMENT_NONE:
          case ELEMENT_FIRE:
               e = ELEMENT_FIRE;
               break;
          case ELEMENT_ICE:
               e = ELEMENT_NONE;
               break;
          }
          break;
     case ELEMENT_ICE:
          switch(b){
          default:
          case ELEMENT_NONE:
          case ELEMENT_ICE:
               e = ELEMENT_ICE;
               break;
          case ELEMENT_FIRE:
               e = ELEMENT_NONE;
               break;
          }
          break;
     case ELEMENT_ONLY_ICED:
          switch(b){
          default:
          case ELEMENT_NONE:
               break;
          case ELEMENT_ICE:
               e = ELEMENT_ICE;
               break;
          case ELEMENT_FIRE:
               e = ELEMENT_FIRE;
               break;
          }
          break;
     }

     return e;
}

const char* element_to_string(Element_t e){
     switch(e){
     default:
          break;
     CASE_ENUM_RET_STR(ELEMENT_NONE);
     CASE_ENUM_RET_STR(ELEMENT_FIRE);
     CASE_ENUM_RET_STR(ELEMENT_ICE);
     CASE_ENUM_RET_STR(ELEMENT_COUNT);
     }

     return "ELEMENT_UNKNOWN";
}
