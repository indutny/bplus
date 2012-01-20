#ifndef _PRIVATE_TREE_H_
#define _PRIVATE_TREE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "private/writer.h"
#include "private/pages.h"

#define BP_TREE_PRIVATE \
    BP_WRITER_PRIVATE \
    bp__tree_head_t head;\
    bp__page_t* head_page;\
    bp_compare_cb compare_cb;

typedef struct bp__tree_head_s bp__tree_head_t;

int bp__tree_read_head(bp__writer_t* w, void* data);
int bp__tree_write_head(bp__writer_t* w, void* data);


struct bp__tree_head_s {
  uint32_t offset;
  uint32_t config;
  uint32_t csize;
  uint32_t page_size;
  uint32_t hash;

  uint8_t padding[32 - 16];
};

#ifdef __cplusplus
}
#endif

#endif /* _PRIVATE_TREE_H_ */
