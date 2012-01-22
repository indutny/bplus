#include "bplus.h"
#include "private/values.h"

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */


int bp__kv_copy(const bp__kv_t* source, bp__kv_t* target, int alloc) {
  /* copy key fields */
  if (alloc) {
    target->value = malloc(source->length);

    if (target->value == NULL) return BP_EALLOC;
    memcpy(target->value, source->value, source->length);
    target->allocated = 1;
  } else {
    target->value = source->value;
    target->allocated = source->allocated;
  }

  target->length = source->length;

  /* copy rest */
  target->offset = source->offset;
  target->config = source->config;

  return BP_OK;
}
