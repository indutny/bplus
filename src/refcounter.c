#include "bplus.h"
#include "private/refcounter.h"
#include "private/threads.h"

int bp__ref_init(bp__ref_t* handle) {
  int ret;

  ret = bp__mutex_init(&handle->ref_change_lock);
  if (ret != BP_OK) return ret;
  ret = bp__mutex_init(&handle->ref_close_lock);
  if (ret != BP_OK) bp__mutex_destroy(&handle->ref_change_lock);

  handle->ref_state = kOpen;
  handle->ref = 0;

  return ret;
}


void bp__ref_destroy(bp__ref_t* handle) {
  bp__mutex_destroy(&handle->ref_change_lock);
  bp__mutex_destroy(&handle->ref_close_lock);
}


void bp__ref(bp__ref_t* handle) {
  bp__mutex_lock(&handle->ref_change_lock);

  /* wait until handle will become open */
  if (handle->ref_state != kOpen) {
    bp__mutex_lock(&handle->ref_close_lock);
    bp__mutex_unlock(&handle->ref_close_lock);
  }

  if (handle->ref == 0) {
    bp__mutex_lock(&handle->ref_close_lock);
  }
  handle->ref++;
  bp__mutex_unlock(&handle->ref_change_lock);
}


void bp__unref(bp__ref_t* handle) {
  bp__mutex_lock(&handle->ref_change_lock);
  handle->ref--;
  if (handle->ref == 0) {
    bp__mutex_unlock(&handle->ref_close_lock);
  }
  bp__mutex_unlock(&handle->ref_change_lock);
}


void bp__ref_close(bp__ref_t* handle) {
  bp__mutex_lock(&handle->ref_change_lock);
  if (handle->ref_state == kOpen) {
    handle->ref_state = kHalfClosed;
  }
  bp__mutex_lock(&handle->ref_close_lock);
  if (handle->ref_state == kHalfClosed) {
    handle->ref_state = kClosed;
  }
  bp__mutex_unlock(&handle->ref_change_lock);
}


void bp__ref_open(bp__ref_t* handle) {
  bp__mutex_lock(&handle->ref_change_lock);
  if (handle->ref_state == kClosed) {
    handle->ref_state = kOpen;
  }
  bp__mutex_unlock(&handle->ref_close_lock);
  bp__mutex_unlock(&handle->ref_change_lock);
}
