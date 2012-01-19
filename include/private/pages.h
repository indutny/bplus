#ifndef _PRIVATE_PAGES_H_
#define _PRIVATE_PAGES_H_

#include "private/tree.h"

#define BP__KV_HEADER_SIZE 12

typedef struct bp_tree_s bp_tree_t;

typedef struct bp__page_s bp__page_t;
typedef struct bp__kv_s bp__kv_t;

int bp__page_create(bp_tree_t* t,
                    const int is_leaf,
                    const uint32_t offset,
                    const uint32_t config,
                    bp__page_t** page);
int bp__page_destroy(bp_tree_t* t, bp__page_t* page);

int bp__page_load(bp_tree_t* t, bp__page_t* page);
int bp__page_save(bp_tree_t* t, bp__page_t* page);

int bp__page_insert(bp_tree_t* t, bp__page_t* page, bp__kv_t* kv);
int bp__page_remove(bp_tree_t* t, bp__page_t* page, uint32_t index);
int bp__page_remove(bp_tree_t* t, bp__page_t* page, bp__kv_t* kv);
int bp__page_split(bp_tree_t* t, bp__page_t* parent, bp__page_t* child);

struct bp__kv_s {
  BP_KEY_FIELDS

  uint32_t offset;
  uint32_t config;

  uint8_t allocated;
};

struct bp__page_s {
  uint8_t is_leaf;
  uint32_t length;
  uint32_t byte_size;

  uint32_t offset;
  uint32_t config;

  void* buff_;

  bp__kv_t keys[];
};


int bp__kv_copy(bp__kv_t* source, bp__kv_t* target, int alloc);


#endif /* _PRIVATE_PAGES_H_ */
