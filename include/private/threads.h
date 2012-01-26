#ifndef _PRIVATE_THREADS_H_
#define _PRIVATE_THREADS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

typedef pthread_mutex_t bp__mutex_t;


int bp__mutex_init(bp__mutex_t* mutex);
void bp__mutex_destroy(bp__mutex_t* mutex);
void bp__mutex_lock(bp__mutex_t* mutex);
void bp__mutex_unlock(bp__mutex_t* mutex);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _PRIVATE_THREADS_H_ */
