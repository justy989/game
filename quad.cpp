#include "quad.h"
#include "vec.h"

static bool vec_in_quad(const Vec_t* v, const Quad_t* q){
     return (v->x >= q->left && v->x <= q->right &&
             v->y >= q->bottom && v->y <= q->top);
}

bool quad_in_quad(const Quad_t* a, const Quad_t* b){
     Vec_t top_left {b->left, b->top};
     Vec_t top_right {b->right, b->top};
     Vec_t bottom_left {b->left, b->bottom};
     Vec_t bottom_right {b->right, b->bottom};

     if(vec_in_quad(&top_left, a)) return true;
     if(vec_in_quad(&top_right, a)) return true;
     if(vec_in_quad(&bottom_left, a)) return true;
     if(vec_in_quad(&bottom_right, a)) return true;

     return false;
}

QuadInQuadHighRangeResult_t quad_in_quad_high_range_exclusive(const Quad_t* a, const Quad_t* b){
     QuadInQuadHighRangeResult_t result {};

     if(a->left >= b->left && a->left < b->right &&
        a->bottom >= b->bottom && a->bottom < b->top){
          result.horizontal_overlap = b->right - a->left;
          result.vertical_overlap = b->top - a->bottom;
          result.inside = true;
          return result;
     }

     if(a->right > b->left && a->right < b->right &&
        a->bottom >= b->bottom && a->bottom < b->top){
          result.horizontal_overlap = a->right - b->left;
          result.vertical_overlap = b->top - a->bottom;
          result.inside = true;
          return result;
     }

     if(a->left >= b->left && a->left < b->right &&
        a->top > b->bottom && a->top < b->top){
          result.horizontal_overlap = b->right - a->left;
          result.vertical_overlap = a->top - b->bottom;
          result.inside = true;
          return result;
     }

     if(a->right > b->left && a->right < b->right &&
        a->top > b->bottom && a->top < b->top){
          result.horizontal_overlap = a->right - b->left;
          result.vertical_overlap = a->top - b->bottom;
          result.inside = true;
          return result;
     }

     return result;
}

bool operator==(const Quad_t& a, const Quad_t& b){
     return (a.left == b.left && a.right == b.right &&
             a.bottom == b.bottom && a.top == b.top);
}
