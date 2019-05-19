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

bool quad_in_quad_high_range_exclusive(const Quad_t* a, const Quad_t* b){
     if(a->left >= b->left && a->left < b->right &&
        a->bottom >= b->bottom && a->bottom < b->top){
          return true;
     }

     if(a->right > b->left && a->right < b->right &&
        a->bottom >= b->bottom && a->bottom < b->top){
          return true;
     }

     if(a->left >= b->left && a->left < b->right &&
        a->top > b->bottom && a->top < b->top){
          return true;
     }

     if(a->right > b->left && a->right < b->right &&
        a->top > b->bottom && a->top < b->top){
          return true;
     }

     return false;
}

bool operator==(const Quad_t& a, const Quad_t& b){
     return (a.left == b.left && a.right == b.right &&
             a.bottom == b.bottom && a.top == b.top);
}
