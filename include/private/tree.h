#ifndef _PRIVATE_TREE_H_
#define _PRIVATE_TREE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "private/threads.h"
#include "private/writer.h"
#include "private/pages.h"

#define BP__HEAD_SIZE sizeof(uint64_t) * 4

#define BP_TREE_PRIVATE\
    BP_WRITER_PRIVATE\
    enum bp__tree_state state;\
    bp__mutex_t write_mutex;\
    bp__tree_head_t head;\
    bp_compare_cb compare_cb;

typedef struct bp__tree_head_s bp__tree_head_t;

int bp__tree_swap_head(bp_tree_t* tree, bp__page_t** clone);

int bp__tree_read_head(bp__writer_t* w, void* data);
int bp__tree_write_head(bp__writer_t* w, void* data);

int bp__default_compare_cb(const bp_key_t* a, const bp_key_t* b);
int bp__default_filter_cb(void* arg, const bp_key_t* key);


enum bp__tree_state {
  kNormal = 0,
  kCompacting = 1
};


struct bp__tree_head_s {
  uint64_t offset;
  uint64_t config;
  uint64_t page_size;
  uint64_t hash;

  bp__page_t* page;
  bp__page_t* new_page;

  /* mutex for changing head page */
  bp__mutex_t mutex;
};

#ifdef __cplusplus
}
#endif

#endif /* _PRIVATE_TREE_H_ */
