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
