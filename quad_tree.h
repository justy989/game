#pragma once

#include "rect.h"
#include "object_array.h"

#include <stdlib.h>
#include <assert.h>

#define QUAD_TREE_NODE_ENTRY_COUNT 4

template <typename T>
struct QuadTreeNode_t{
     T* entries[QUAD_TREE_NODE_ENTRY_COUNT];
     S8 entry_count;

     Rect_t bounds;

     QuadTreeNode_t* bottom_left;
     QuadTreeNode_t* bottom_right;
     QuadTreeNode_t* top_left;
     QuadTreeNode_t* top_right;
};

template <typename T>
bool quad_tree_insert(QuadTreeNode_t<T>* node, T* object);

template <typename T>
bool quad_tree_subdivide(QuadTreeNode_t<T>* node){
     if(node->bounds.left == node->bounds.right && node->bounds.bottom == node->bounds.top) return false;

     node->bottom_left = (QuadTreeNode_t<T>*)(calloc(1, sizeof(*node)));
     if(!node->bottom_left) return false;

     node->bottom_right = (QuadTreeNode_t<T>*)(calloc(1, sizeof(*node)));
     if(!node->bottom_right) return false;

     node->top_left = (QuadTreeNode_t<T>*)(calloc(1, sizeof(*node)));
     if(!node->top_left) return false;

     node->top_right = (QuadTreeNode_t<T>*)(calloc(1, sizeof(*node)));
     if(!node->top_right) return false;

     int half_width = (node->bounds.right - node->bounds.left) / 2;
     int half_height = (node->bounds.top - node->bounds.bottom) / 2;

     node->bottom_left->bounds.left = node->bounds.left;
     node->bottom_left->bounds.right = node->bounds.left + half_width;
     node->bottom_left->bounds.bottom = node->bounds.bottom;
     node->bottom_left->bounds.top = node->bounds.bottom + half_height;

     node->bottom_right->bounds.left = node->bottom_left->bounds.right + 1;
     node->bottom_right->bounds.right = node->bounds.right;
     node->bottom_right->bounds.bottom = node->bounds.bottom;
     node->bottom_right->bounds.top = node->bounds.bottom + half_height;

     node->top_left->bounds.left = node->bounds.left;
     node->top_left->bounds.right = node->bounds.left + half_width;
     node->top_left->bounds.bottom = node->bottom_left->bounds.top + 1;
     node->top_left->bounds.top = node->bounds.top;

     node->top_right->bounds.left = node->bottom_right->bounds.left;
     node->top_right->bounds.right = node->bottom_right->bounds.right;
     node->top_right->bounds.bottom = node->bottom_left->bounds.top + 1;
     node->top_right->bounds.top = node->bounds.top;

     for(S8 i = 0; i < node->entry_count; i++){
          if(quad_tree_insert(node->bottom_left, node->entries[i])) continue;
          if(quad_tree_insert(node->bottom_right, node->entries[i])) continue;
          if(quad_tree_insert(node->top_left, node->entries[i])) continue;
          if(quad_tree_insert(node->top_right, node->entries[i])) continue;
     }

     node->entry_count = 0;
     return true;
}

template <typename T>
bool quad_tree_insert(QuadTreeNode_t<T>* node, T* object){
     if(!xy_in_rect(node->bounds, get_object_x(object), get_object_y(object))) return false;

     if(node->entry_count == 0 && node->bottom_left){
          // pass if the node is empty and has been subdivided
     }else{
          if(node->entry_count < QUAD_TREE_NODE_ENTRY_COUNT){
               node->entries[node->entry_count] = object;
               node->entry_count++;
               return true;
          }
     }

     if(!node->bottom_left){
          if(!quad_tree_subdivide(node)) return false; // nomem
     }

     if(quad_tree_insert(node->bottom_left, object)) return true;
     if(quad_tree_insert(node->bottom_right, object)) return true;
     if(quad_tree_insert(node->top_left, object)) return true;
     if(quad_tree_insert(node->top_right, object)) return true;

     return true;
}

template <typename T>
T* quad_tree_find_at(QuadTreeNode_t<T>* node, S16 x, S16 y){
     if(!xy_in_rect(node->bounds, x, y)) return nullptr;

     for(S8 i = 0; i < node->entry_count; i++){
          if(get_object_x(node->entries[i]) == x &&
             get_object_y(node->entries[i]) == y) return node->entries[i];
     }

     if(node->bottom_left){
          auto* result = quad_tree_find_at(node->bottom_left, x, y);
          if(result) return result;
          result = quad_tree_find_at(node->bottom_right, x, y);
          if(result) return result;
          result = quad_tree_find_at(node->top_left, x, y);
          if(result) return result;
          result = quad_tree_find_at(node->top_right, x, y);
          if(result) return result;
     }

     return nullptr;
}

template <typename T>
void quad_tree_free(QuadTreeNode_t<T>* root){
     if(!root) return;
     if(root->top_left) quad_tree_free(root->top_left);
     if(root->top_right) quad_tree_free(root->top_right);
     if(root->bottom_left) quad_tree_free(root->bottom_left);
     if(root->bottom_right) quad_tree_free(root->bottom_right);
     free(root);
}

template <typename T>
QuadTreeNode_t<T>* quad_tree_build(ObjectArray_t<T>* array){
     if(array->count == 0) return nullptr;

     QuadTreeNode_t<T>* root = (QuadTreeNode_t<T>*)(calloc(1, sizeof(*root)));
     root->bounds.left = get_object_x(array->elements + 0);
     root->bounds.right = get_object_x(array->elements + 0);
     root->bounds.bottom = get_object_y(array->elements + 0);
     root->bounds.top = get_object_y(array->elements + 0);

     // find mins/maxs for dimensions
     for(int i = 0; i < array->count; i++){
          S16 x = get_object_x(array->elements + i);
          S16 y = get_object_y(array->elements + i);
          if(root->bounds.left > x) root->bounds.left = x;
          if(root->bounds.right < x) root->bounds.right = x;
          if(root->bounds.bottom > y) root->bounds.bottom = y;
          if(root->bounds.top < y) root->bounds.top = y;
     }

     // insert coords
     for(int i = 0; i < array->count; i++){
          if(!quad_tree_insert(root, array->elements + i)) break;
     }

     return root;
}

template <typename T>
void quad_tree_find_in_impl(QuadTreeNode_t<T>* node, Rect_t rect, T** results_array, S16* count, S16 max_array_count){
     if(!rect_in_rect(rect, node->bounds) && !rect_in_rect(node->bounds, rect)) return;

     for(S8 i = 0; i < node->entry_count; i++){
          if(*count >= max_array_count){
               assert(!"hit max array count when querying quad tree!");
               return;
          }

          S16 x = get_object_x(node->entries[i]);
          S16 y = get_object_y(node->entries[i]);
          if(xy_in_rect(rect, x, y)){
               results_array[*count] = node->entries[i];
               (*count)++;
          }
     }

     if(node->bottom_left){
          quad_tree_find_in_impl(node->bottom_left, rect, results_array, count, max_array_count);
          quad_tree_find_in_impl(node->bottom_right, rect, results_array, count, max_array_count);
          quad_tree_find_in_impl(node->top_left, rect, results_array, count, max_array_count);
          quad_tree_find_in_impl(node->top_right, rect, results_array, count, max_array_count);
     }
}

// NOTE: must free returned array !
template <typename T>
void quad_tree_find_in(QuadTreeNode_t<T>* node, Rect_t rect, T** results_array, S16* count, S16 max_array_count){
     *count = 0;
     quad_tree_find_in_impl(node, rect, results_array, count, max_array_count);
}
