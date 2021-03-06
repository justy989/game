#include "tags.h"
#include "log.h"

const char* tag_to_string(Tag_t tag){
    switch(tag){
    default:
        break;
    case TAB_PLAYER_STOPS_COASTING_BLOCK:
        return "PLAYER_STOPS_COASTING_BLOCK";
    case TAG_PLAYER_PUSHES_MORE_THAN_ONE_MASS:
        return "PLAYER_PUSHES_MORE_THAN_ONE_MASS";
    case TAG_BLOCK:
        return "BLOCK";
    case TAG_BLOCK_FALLS_IN_PIT:
        return "BLOCK_FALLS_IN_PIT";
    case TAG_BLOCK_BLOCKS_ICE_FROM_BEING_MELTED:
        return "BLOCK_BLOCKS_ICE_FROM_BEING_MELTED";
    case TAG_BLOCK_BLOCKS_ICE_FROM_BEING_SPREAD:
        return "BLOCK_BLOCKS_ICE_FROM_BEING_SPREAD";
    case TAB_BLOCK_MOMENTUM_COLLISION:
        return "BLOCK_MOMENTUM_COLLISION";
    case TAG_BLOCK_GETS_SPLIT:
        return "BLOCK_GETS_SPLIT";
    case TAG_BLOCK_GETS_DESTROYED:
        return "BLOCK_GETS_DESTROYED";
    case TAG_BLOCKS_STACKED:
        return "BLOCKS_STACKED";
    case TAG_BLOCK_EXTINGUISHED_BY_STOMP:
        return "BLOCK_EXTINGUISHED_BY_STOMP";
    case TAG_BLOCK_BOUNCES_BACK_FROM_MOMENTUM:
        return "BLOCK_BOUNCES_BACK_FROM_MOMENTUM";
    case TAG_BLOCK_SQUISHES_PLAYER:
        return "BLOCK_SQUISHES_PLAYER";
    case TAG_BLOCK_MULTIPLE_INTERVALS_TO_STOP:
        return "BLOCK_MULTIPLE_INTERVALS_TO_STOP";
    case TAG_SPLIT_BLOCK:
        return "SPLIT_BLOCK";
    case TAG_POPUP:
        return "POPUP";
    case TAB_POPUP_RAISES_BLOCK:
        return "POPUP_RAISES_BLOCK";
    case TAB_POPUP_RAISES_PLAYER:
        return "POPUP_RAISES_PLAYER";
    case TAG_LEVER:
        return "LEVER";
    case TAG_PIT:
        return "PIT";
    case TAG_PRESSURE_PLATE:
        return "PRESSURE_PLATE";
    case TAG_LIGHT_DETECTOR:
        return "LIGHT_DETECTOR";
    case TAG_ICE:
        return "ICE";
    case TAG_ICE_DETECTOR:
        return "ICE_DETECTOR";
    case TAG_ICE_BLOCK:
        return "ICE_BLOCK";
    case TAG_FIRE_BLOCK:
        return "FIRE_BLOCK";
    case TAG_FIRE_BLOCK_SLIDES_ON_ICE:
        return "FIRE_BLOCK_SLIDES_ON_ICE";
    case TAG_ICED_BLOCK:
        return "ICED_BLOCK";
    case TAG_MELT_ICE:
        return "MELT_ICE";
    case TAG_MELTED_POPUP:
        return "MELTED_POPUP";
    case TAG_MELTED_PRESSURE_PLATE:
        return "MELTED_PRESSURE_PLATE";
    case TAG_SPREAD_ICE:
        return "SPREAD_ICE";
    case TAG_ARROW:
        return "ARROW";
    case TAG_ARROW_CHANGES_BLOCK_ELEMENT:
        return "ARROW_CHANGES_BLOCK_ELEMENT";
    case TAB_ARROW_ACTIVATES_LEVER:
        return "ARROW_ACTIVATES_LEVER";
    case TAG_ARROW_STICKS_INTO_BLOCK:
        return "ARROW_STICKS_INTO_BLOCK";
    case TAG_ICED_POPUP:
        return "ICED_POPUP";
    case TAG_ICED_PRESSURE_PLATE:
        return "ICED_PRESSURE_PLATE";
    case TAG_ICED_PRESSURE_PLATE_NOT_ACTIVATED:
        return "ICED_PRESSURE_PLATE_NOT_ACTIVATED";
    case TAG_PORTAL:
        return "PORTAL";
    case TAG_PORTAL_ROT_90:
        return "PORTAL_ROT_90";
    case TAG_PORTAL_ROT_180:
        return "PORTAL_ROT_180";
    case TAG_PORTAL_ROT_270:
        return "PORTAL_ROT_270";
    case TAG_TELEPORT_PLAYER:
        return "TELEPORT_PLAYER";
    case TAG_TELEPORT_BLOCK:
        return "TELEPORT_BLOCK";
    case TAG_TELEPORT_ARROW:
        return "TELEPORT_ARROW";
    case TAG_BLOCK_GETS_ENTANGLED:
        return "BLOCK_GETS_ENTANGLED";
    case TAG_PLAYER_GETS_ENTANGLED:
        return "PLAYER_GETS_ENTANGLED";
    case TAG_ARROW_GETS_ENTANGLED:
        return "ARROW_GETS_ENTANGLED";
    case TAG_ENTANGLED_BLOCK:
        return "ENTANGLED_BLOCK";
    case TAG_ENTANGLED_BLOCK_ROT_90:
        return "ENTANGLED_BLOCK_ROT_90";
    case TAG_ENTANGLED_BLOCK_ROT_180:
        return "ENTANGLED_BLOCK_ROT_180";
    case TAG_ENTANGLED_BLOCK_ROT_270:
        return "ENTANGLED_BLOCK_ROT_270";
    case TAG_ENTANGLED_BLOCK_HELD_DOWN_UNMOVABLE:
        return "ENTANGLED_BLOCK_HELD_DOWN_UNMOVABLE";
    case TAG_ENTANGLED_BLOCK_FLOATS:
        return "ENTANGLED_BLOCK_FLOATS";
    case TAG_ENTANGLED_BLOCKS_OF_DIFFERENT_SPLITS:
        return "ENTANGLED_BLOCKS_OF_DIFFERENT_SPLITS";
    case TAG_ENTANGLED_CENTROID_COLLISION:
        return "ENTANGLED_CENTROID_COLLISION";
    case TAG_THREE_PLUS_BLOCKS_ENTANGLED:
        return "THREE_PLUS_BLOCKS_ENTANGLED";
    case TAG_THREE_PLUS_PLAYERS_ENTANGLED:
        return "THREE_PLUS_PLAYERS_ENTANGLED";
    case TAG_THREE_PLUS_ARROWS_ENTANGLED:
        return "THREE_PLUS_ARROWS_ENTANGLED";
    case TAG_COUNT:
        return "COUNT";
    }

    return "TAG_UNKNOWN";
}

bool global_tags[TAG_COUNT];

void add_global_tag(Tag_t tag){
     global_tags[tag] = true;
}

bool* get_global_tags(){ return global_tags; }

void clear_global_tags(){
     for(S32 c = 0; c < TAG_COUNT; c++){
          global_tags[c] = false;
     }
}

void log_global_tags(){
     for(S32 c = 0; c < TAG_COUNT; c++){
          if(global_tags[c]){
               LOG("%s\n", tag_to_string((Tag_t)(c)));
          }
     }
}
