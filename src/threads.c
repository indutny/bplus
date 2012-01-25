#include "private/threads.h"
#include "private/errors.h"
#include <pthread.h>

#include <stdlib.h>

#ifdef NDEBUG
#include <stdio.h>
#define ENSURE(what, expr)\
    int ret = expr;\
    if (ret != 0) {\
      fprintf(stdout, what " failed with code : %d\n", ret);\
      abort();\
    }
#else
#define ENSURE(what, expr)\
    if (expr) abort();
#endif

int bp__mutex_init(bp__mutex_t* mutex) {
  return pthread_mutex_init(mutex, NULL) == 0 ? BP_OK : BP_EMUTEX;
}


void bp__mutex_destroy(bp__mutex_t* mutex) {
  ENSURE("mutex destroy", pthread_mutex_destroy(mutex));
}


void bp__mutex_lock(bp__mutex_t* mutex) {
  ENSURE("mutex lock", pthread_mutex_lock(mutex));
}


void bp__mutex_unlock(bp__mutex_t* mutex) {
  ENSURE("mutex unlock", pthread_mutex_unlock(mutex));
}
