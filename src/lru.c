#include "private/lru.h"
#include "private/threads.h"

#include <stdlib.h> /* malloc */


bp__lru_t* bp__lru_create(const uint32_t size, bp__lru_destructor destructor) {
  bp__lru_t* result = malloc(sizeof(*result) +
                             (size - 1) * sizeof(result->items[0]));
  if (result == NULL) return NULL;

  /* LRU is empty initially */
  result->length = 0;
  result->size = size;

  result->destructor = destructor;
  result->mru = 0;

  bp__mutex_init(&result->mutex);

  return result;
}


void bp__lru_destroy(bp__lru_t* lru) {
  while (lru->length > 0) {
    bp__lru_del(lru, 0);
  }
  bp__mutex_destroy(&lru->mutex);

  free(lru);
}


void bp__lru_insert(bp__lru_t* lru, const uint64_t key, void* value) {
  uint32_t i;

  bp__mutex_lock(&lru->mutex);
  for (i = 0; i < lru->length; i++) {
    if (lru->items[i].key == key) break;
  }
  if (i == lru->length) {
    if (lru->length == lru->size) bp__lru_discard(lru);

    lru->items[lru->length].key = key;
    lru->items[lru->length].value = value;
    lru->items[lru->length].timestamp = lru->mru++;
    lru->length++;
  }
  bp__mutex_unlock(&lru->mutex);
}


void* bp__lru_get(bp__lru_t* lru, const uint64_t key) {
  uint32_t i;
  void* result = NULL;

  bp__mutex_lock(&lru->mutex);
  for (i = 0; i < lru->length; i++) {
    if (lru->items[i].key == key) break;
  }
  if (i != lru->length) {
    lru->items[i].timestamp = lru->mru++;
    result = lru->items[i].value;
  }
  bp__mutex_unlock(&lru->mutex);

  return result;
}


void bp__lru_del(bp__lru_t* lru, const uint32_t index) {
  uint32_t i;

  lru->destructor(lru->items[index].value);

  /* Shift left */
  for (i = index; i < lru->length - 1; i++) {
    lru->items[i] = lru->items[i + 1];
  }
  lru->length--;
}


void bp__lru_discard(bp__lru_t* lru) {
  uint32_t i, min_i = 0;
  uint64_t min = -1;
  for (i = 0; i < lru->length; i++) {
    if (lru->items[i].timestamp <= min) {
      min_i = i;
      min = lru->items[i].timestamp;
    }
  }

  if (i == lru->length) i = min_i;

  bp__lru_del(lru, i);
}
