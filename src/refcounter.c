#include "bplus.h"
#include "private/refcounter.h"
#include "private/threads.h"

#define SCOPED_LOCK(lock, A)\
    bp__mutex_lock(&lock);\
    A\
    bp__mutex_unlock(&lock);


int bp__ref_init(bp__ref_t* handle) {
  int ret;

  ret = bp__mutex_init(&handle->ref_lock);
  if (ret != BP_OK) return ret;
  ret = bp__mutex_init(&handle->ref_state_lock);
  if (ret != BP_OK) bp__mutex_destroy(&handle->ref_lock);

  handle->ref_state = kOpen;
  handle->ref = 0;

  return ret;
}


void bp__ref_destroy(bp__ref_t* handle) {
  bp__mutex_destroy(&handle->ref_lock);
  bp__mutex_destroy(&handle->ref_state_lock);
}


void bp__ref(bp__ref_t* handle) {
  SCOPED_LOCK(handle->ref_lock, {
    /*
     * pause close requests until handle will have ref == 0
     */
    if (handle->ref == 0) {
      bp__mutex_lock(&handle->ref_state_lock);
    }
    handle->ref++;
  })
}


void bp__unref(bp__ref_t* handle) {
  SCOPED_LOCK(handle->ref_lock, {
    handle->ref--;
    if (handle->ref == 0) {
      bp__mutex_unlock(&handle->ref_state_lock);
    }
  })
}


void bp__ref_close(bp__ref_t* handle) {
  handle->ref_state = kHalfClosed;
  bp__mutex_lock(&handle->ref_state_lock);
  handle->ref_state = kClosed;
}


void bp__ref_open(bp__ref_t* handle) {
  handle->ref_state = kOpen;
  bp__mutex_unlock(&handle->ref_state_lock);
}
