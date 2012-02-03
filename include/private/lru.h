#ifndef _PRIVATE_LRU_H_
#define _PRIVATE_LRU_H_

#include "private/threads.h"
#include <stdint.h> /* uint32_t */

typedef struct bp__lru_s bp__lru_t;
typedef struct bp__lru_item_s bp__lru_item_t;
typedef void (*bp__lru_destructor)(void*);

bp__lru_t* bp__lru_create(const uint32_t size, bp__lru_destructor destructor);
void bp__lru_destroy(bp__lru_t* lru);

void bp__lru_insert(bp__lru_t* lru, const uint64_t key, void* value);
void* bp__lru_get(bp__lru_t* lru, const uint64_t key);
void bp__lru_discard(bp__lru_t* lru);
void bp__lru_del(bp__lru_t* lru, const uint32_t index);


struct bp__lru_item_s {
  uint64_t key;
  uint64_t timestamp;
  void* value;
};

struct bp__lru_s {
  uint32_t length;
  uint32_t size;

  uint64_t mru;

  bp__mutex_t mutex;
  bp__lru_destructor destructor;

  bp__lru_item_t items[1];
};

#endif /* _PRIVATE_LRU_H_ */
