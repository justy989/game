#pragma once

#include "types.h"

enum BlockCut_t{
     BLOCK_CUT_WHOLE,
     BLOCK_CUT_LEFT_HALF,
     BLOCK_CUT_RIGHT_HALF,
     BLOCK_CUT_TOP_HALF,
     BLOCK_CUT_BOTTOM_HALF,
     BLOCK_CUT_TOP_LEFT_QUARTER,
     BLOCK_CUT_TOP_RIGHT_QUARTER,
     BLOCK_CUT_BOTTOM_LEFT_QUARTER,
     BLOCK_CUT_BOTTOM_RIGHT_QUARTER,
};

BlockCut_t block_cut_rotate_clockwise(BlockCut_t cut);
BlockCut_t block_cut_rotate_counter_clockwise(BlockCut_t cut);

BlockCut_t block_cut_rotate_clockwise(BlockCut_t cut, S8 times);
BlockCut_t block_cut_rotate_counter_clockwise(BlockCut_t cut, S8 times);

const char* block_cut_to_string(BlockCut_t cut);
