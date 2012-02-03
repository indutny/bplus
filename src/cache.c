#include "private/cache.h"
#include "private/threads.h"
#include "private/utils.h"

#include <stdio.h> /* malloc */
#include <stdlib.h> /* malloc */
#include <string.h> /* memset */


const uint32_t max_offset = 8;

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

  result->lru = 0;

  result->arg = arg;
  result->destructor = destructor;
  result->clone = clone;

  /* cache is empty initially */
  memset(result->space, 0, sizeof(*result->space) * result->size);

  bp__rwlock_init(&result->lock);

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

  bp__rwlock_destroy(&cache->lock);
  free(cache->space);
  free(cache);
}


void bp__cache_set(bp__cache_t* cache, const uint64_t key, void* value) {
  uint32_t index = bp__compute_hash(key) & cache->mask;
  uint32_t offset = 0;
  uint64_t min = -1;
  uint32_t min_index = 0;
  void* clone;

  while (offset < max_offset && cache->space[index].value != NULL) {
    if (min > cache->space[index].lru) {
      min = cache->space[index].lru;
      min_index = index;
    }
    offset++;
    index = (index + 1) & cache->mask;
  }

  if (offset == max_offset && cache->space[min_index].value != NULL) {
    bp__rwlock_wrlock(&cache->lock);
    cache->destructor(cache->arg, cache->space[min_index].value);
    cache->space[min_index].value = NULL;
    index = min_index;
    bp__rwlock_unlock(&cache->lock);
  }

  cache->clone(cache->arg, value, &clone);

  cache->space[index].lru = cache->lru++;
  cache->space[index].key = key;
  cache->space[index].value = clone;
}


void* bp__cache_get(bp__cache_t* cache, const uint64_t key) {
  uint32_t index = bp__compute_hash(key) & cache->mask;
  uint32_t offset = 0;
  void* result = NULL;

  while (offset < max_offset &&
         (cache->space[index].value == NULL ||
         cache->space[index].key != key)) {
    offset++;
    index = (index + 1) & cache->mask;
  }

  if (offset != max_offset) {
    bp__rwlock_rdlock(&cache->lock);
    if (cache->space[index].value != NULL) {
      cache->clone(cache->arg, cache->space[index].value, &result);
      cache->space[index].lru = ++cache->lru;
    }
    bp__rwlock_unlock(&cache->lock);
  }

  return result;
}
