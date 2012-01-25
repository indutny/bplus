#include "private/threads.h"
#include "private/errors.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>


int bp__mutex_init(bp__mutex_t* mutex) {
  return pthread_mutex_init(mutex, NULL) == 0 ? BP_OK : BP_EMUTEX;
}


void bp__mutex_destroy(bp__mutex_t* mutex) {
  if (pthread_mutex_destroy(mutex)) abort();
}


void bp__mutex_lock(bp__mutex_t* mutex) {
  if (pthread_mutex_lock(mutex)) abort();
}


void bp__mutex_unlock(bp__mutex_t* mutex) {
  if (pthread_mutex_unlock(mutex)) abort();
}
