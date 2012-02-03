#ifndef _PRIVATE_CACHE_H_
#define _PRIVATE_CACHE_H_

#include "private/threads.h"
#include <stdint.h> /* uint32_t */

typedef struct bp__cache_s bp__cache_t;
typedef struct bp__cache_item_s bp__cache_item_t;
typedef void (*bp__cache_clone)(void* arg, void* original, void** clone);
typedef void (*bp__cache_destructor)(void* arg, void* value);

bp__cache_t* bp__cache_create(const uint32_t size,
                              void *arg,
                              bp__cache_clone clone,
                              bp__cache_destructor destructor);
void bp__cache_destroy(bp__cache_t* cache);

void bp__cache_set(bp__cache_t* cache, const uint64_t key, void* value);
void* bp__cache_get(bp__cache_t* cache, const uint64_t key);

struct bp__cache_item_s {
  uint64_t key;
  void* value;
};

struct bp__cache_s {
  uint32_t size;
  uint32_t mask;

  bp__mutex_t lock;

  void* arg;
  bp__cache_clone clone;
  bp__cache_destructor destructor;

  bp__cache_item_t* space;
};

#endif /* _PRIVATE_CACHE_H_ */
