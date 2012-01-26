#ifndef _PRIVATE_REFCOUNTER_H_
#define _PRIVATE_REFCOUNTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "private/threads.h"

typedef struct bp__ref_s bp__ref_t;
enum bp__ref_state {
  kOpen,
  kHalfClosed,
  kClosed
};

#define BP__REF_PRIVATE\
    int ref;\
    bp__mutex_t ref_change_lock;\
    bp__mutex_t ref_close_lock;\
    enum bp__ref_state ref_state;

int bp__ref_init(bp__ref_t* handle);
void bp__ref_destroy(bp__ref_t* handle);

void bp__ref(bp__ref_t* handle);
void bp__unref(bp__ref_t* handle);

void bp__ref_close(bp__ref_t* handle);
void bp__ref_open(bp__ref_t* handle);

struct bp__ref_s {
  BP__REF_PRIVATE
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PRIVATE_REFCOUNTER_H_ */
