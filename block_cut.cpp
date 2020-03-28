#include "block_cut.h"

BlockCut_t block_cut_rotate_clockwise(BlockCut_t cut){
    switch(cut){
    default:
    case BLOCK_CUT_WHOLE:
        break;
    case BLOCK_CUT_LEFT_HALF:
        return BLOCK_CUT_TOP_HALF;
    case BLOCK_CUT_RIGHT_HALF:
        return BLOCK_CUT_BOTTOM_HALF;
    case BLOCK_CUT_TOP_HALF:
        return BLOCK_CUT_RIGHT_HALF;
    case BLOCK_CUT_BOTTOM_HALF:
        return BLOCK_CUT_LEFT_HALF;
    case BLOCK_CUT_TOP_LEFT_QUARTER:
        return BLOCK_CUT_TOP_RIGHT_QUARTER;
    case BLOCK_CUT_TOP_RIGHT_QUARTER:
        return BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
    case BLOCK_CUT_BOTTOM_LEFT_QUARTER:
        return BLOCK_CUT_TOP_LEFT_QUARTER;
    case BLOCK_CUT_BOTTOM_RIGHT_QUARTER:
        return BLOCK_CUT_BOTTOM_LEFT_QUARTER;
    }

    return cut;
}

BlockCut_t block_cut_rotate_counter_clockwise(BlockCut_t cut){
    switch(cut){
    default:
    case BLOCK_CUT_WHOLE:
        break;
    case BLOCK_CUT_LEFT_HALF:
        return BLOCK_CUT_BOTTOM_HALF;
    case BLOCK_CUT_RIGHT_HALF:
        return BLOCK_CUT_TOP_HALF;
    case BLOCK_CUT_TOP_HALF:
        return BLOCK_CUT_LEFT_HALF;
    case BLOCK_CUT_BOTTOM_HALF:
        return BLOCK_CUT_RIGHT_HALF;
    case BLOCK_CUT_TOP_LEFT_QUARTER:
        return BLOCK_CUT_BOTTOM_LEFT_QUARTER;
    case BLOCK_CUT_TOP_RIGHT_QUARTER:
        return BLOCK_CUT_TOP_LEFT_QUARTER;
    case BLOCK_CUT_BOTTOM_LEFT_QUARTER:
        return BLOCK_CUT_BOTTOM_RIGHT_QUARTER;
    case BLOCK_CUT_BOTTOM_RIGHT_QUARTER:
        return BLOCK_CUT_TOP_RIGHT_QUARTER;
    }

    return cut;
}

BlockCut_t block_cut_rotate_clockwise(BlockCut_t cut, S8 times){
    for(S8 i = 0; i < times; i++){
        cut = block_cut_rotate_clockwise(cut);
    }
    return cut;
}

BlockCut_t block_cut_rotate_counter_clockwise(BlockCut_t cut, S8 times){
    for(S8 i = 0; i < times; i++){
        cut = block_cut_rotate_counter_clockwise(cut);
    }
    return cut;
}

const char* block_cut_to_string(BlockCut_t cut){
    switch(cut){
    default:
    case BLOCK_CUT_WHOLE:
        return "BLOCK_CUT_WHOLE";
    case BLOCK_CUT_LEFT_HALF:
        return "BLOCK_CUT_LEFT_HALF";
    case BLOCK_CUT_RIGHT_HALF:
        return "BLOCK_CUT_RIGHT_HALF";
    case BLOCK_CUT_TOP_HALF:
        return "BLOCK_CUT_TOP_HALF";
    case BLOCK_CUT_BOTTOM_HALF:
        return "BLOCK_CUT_BOTTOM_HALF";
    case BLOCK_CUT_TOP_LEFT_QUARTER:
        return "BLOCK_CUT_TOP_LEFT_QUARTER";
    case BLOCK_CUT_TOP_RIGHT_QUARTER:
        return "BLOCK_CUT_TOP_RIGHT_QUARTER";
    case BLOCK_CUT_BOTTOM_LEFT_QUARTER:
        return "BLOCK_CUT_BOTTOM_LEFT_QUARTER";
    case BLOCK_CUT_BOTTOM_RIGHT_QUARTER:
        return "BLOCK_CUT_BOTTOM_RIGHT_QUARTER";
    }

    return "BLOCK_CUT_UNKNOWN";
}
