#ifndef _PRIVATE_PAGES_H_
#define _PRIVATE_PAGES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "private/tree.h"

#define BP__KV_HEADER_SIZE 12
#define BP__KV_SIZE(kv) BP__KV_HEADER_SIZE + kv.length

typedef struct bp__page_s bp__page_t;
typedef struct bp__kv_s bp__kv_t;
typedef struct bp__page_search_res_s bp__page_search_res_t;

enum page_type {
  kPage = 0,
  kLeaf = 1
};

int bp__page_create(bp_tree_t* t,
                    const enum page_type type,
                    const uint32_t offset,
                    const uint32_t config,
                    bp__page_t** page);
int bp__page_destroy(bp_tree_t* t, bp__page_t* page);

int bp__page_load(bp_tree_t* t, bp__page_t* page);
int bp__page_save(bp_tree_t* t, bp__page_t* page);

int bp__page_search(bp_tree_t* t,
                    bp__page_t* page,
                    const bp__kv_t* kv,
                    bp__page_search_res_t* result);
int bp__page_get(bp_tree_t* t,
                 bp__page_t* page,
                 const bp__kv_t* kv,
                 bp_value_t* value);
int bp__page_insert(bp_tree_t* t, bp__page_t* page, const bp__kv_t* kv);
int bp__page_remove(bp_tree_t* t, bp__page_t* page, const bp__kv_t* kv);

int bp__page_remove_idx(bp_tree_t* t, bp__page_t* page, const uint32_t index);
int bp__page_split(bp_tree_t* t,
                   bp__page_t* parent,
                   const uint32_t index,
                   bp__page_t* child);

void bp__page_shiftr(bp_tree_t* t, bp__page_t* page, const uint32_t index);
void bp__page_shiftl(bp_tree_t* t, bp__page_t* page, const uint32_t index);

struct bp__kv_s {
  BP_KEY_FIELDS

  uint32_t offset;
  uint32_t config;

  uint8_t allocated;
};

struct bp__page_s {
  enum page_type type;

  uint32_t length;
  uint32_t byte_size;

  uint32_t offset;
  uint32_t config;

  void* buff_;

  bp__kv_t keys[1];
};

struct bp__page_search_res_s {
  bp__page_t* child;

  uint32_t index;
  int cmp;
};


int bp__kv_copy(const bp__kv_t* source, bp__kv_t* target, int alloc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _PRIVATE_PAGES_H_ */
