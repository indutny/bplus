#include "private/cache.h"
#include "private/threads.h"
#include "private/utils.h"

#include <stdlib.h> /* malloc */
#include <string.h> /* memset */


bp__cache_t* bp__cache_create(const uint32_t size, bp__cache_destructor destructor) {
  bp__cache_t* result = malloc(sizeof(*result));
  if (result == NULL) return NULL;

  result->space = malloc(sizeof(*result->space) * size);
  if (result->space == NULL) {
    free(result);
    return NULL;
  }

  result->size = size;
  result->destructor = destructor;

  /* cache is empty initially */
  memset(result->space, 0, sizeof(*result->space) * size);

  bp__mutex_init(&result->mutex);

  return result;
}


void bp__cache_destroy(bp__cache_t* cache) {
  uint32_t i;
  for (i = 0; i < cache->size; i++) {
    if (cache->space[i].value != NULL) {
      cache->destructor(cache->space[i].value);
      cache->space[i].value = NULL;
    }
  }

  bp__mutex_destroy(&cache->mutex);
  free(cache->space);
  free(cache);
}


void bp__cache_set(bp__cache_t* cache, const uint64_t key, void* value) {
  uint32_t index = bp__compute_hash(key) % cache->size;
  bp__mutex_lock(&cache->mutex);

  if (cache->space[index].value != NULL) {
    cache->destructor(cache->space[index].value);
  }
  cache->space[index].key = key;
  cache->space[index].value = value;

  bp__mutex_unlock(&cache->mutex);
}


void* bp__cache_get(bp__cache_t* cache, const uint64_t key) {
  uint32_t index = bp__compute_hash(key) % cache->size;
  void* result = NULL;

  bp__mutex_lock(&cache->mutex);
  if (cache->space[index].value != NULL &&
      cache->space[index].key == key) {
    result = cache->space[index].value;
  }
  bp__mutex_unlock(&cache->mutex);

  return result;
}
