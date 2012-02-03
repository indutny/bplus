#include "private/cache.h"
#include "private/threads.h"
#include "private/utils.h"

#include <stdlib.h> /* malloc */
#include <string.h> /* memset */


bp__cache_t* bp__cache_create(const uint32_t size,
                              void* arg,
                              bp__cache_clone clone,
                              bp__cache_destructor destructor) {
  bp__cache_t* result = malloc(sizeof(*result));
  if (result == NULL) return NULL;

  result->size = 1 << size;
  result->mask = result->size - 1;

  result->space = malloc(sizeof(*result->space) * result->size);
  if (result->space == NULL) {
    free(result);
    return NULL;
  }

  result->arg = arg;
  result->destructor = destructor;
  result->clone = clone;

  /* cache is empty initially */
  memset(result->space, 0, sizeof(*result->space) * result->size);

  bp__mutex_init(&result->lock);

  return result;
}


void bp__cache_destroy(bp__cache_t* cache) {
  uint32_t i;
  for (i = 0; i < cache->size; i++) {
    if (cache->space[i].value != NULL) {
      cache->destructor(cache->arg, cache->space[i].value);
      cache->space[i].value = NULL;
    }
  }

  bp__mutex_destroy(&cache->lock);
  free(cache->space);
  free(cache);
}


void bp__cache_set(bp__cache_t* cache, const uint64_t key, void* value) {
  uint32_t index = bp__compute_hash(key) & cache->mask;
  void* clone;
  if (cache->space[index].value != NULL) return;

  bp__mutex_lock(&cache->lock);

  if (cache->space[index].value == NULL) {
    /* cache->destructor(cache->arg, cache->space[index].value); */
    cache->clone(cache->arg, value, &clone);
    cache->space[index].key = key;
    cache->space[index].value = clone;
  }

  bp__mutex_unlock(&cache->lock);
}


void* bp__cache_get(bp__cache_t* cache, const uint64_t key) {
  uint32_t index = bp__compute_hash(key) & cache->mask;
  void* result = NULL;

  if (cache->space[index].value != NULL &&
      cache->space[index].key == key) {
    cache->clone(cache->arg, cache->space[index].value, &result);
  }

  return result;
}
