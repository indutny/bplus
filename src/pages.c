#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */

#include "bplus.h"
#include "private/pages.h"

int bp__page_create(bp_tree_t* t,
                    const int is_leaf,
                    const uint32_t offset,
                    const uint32_t config,
                    bp__page_t** page) {
  /* Allocate space for page + keys */
  bp__page_t* p = malloc(sizeof(*p) +
                         sizeof(p->keys[0]) * (t->head.page_size - 1));
  if (p == NULL) return BP_EALLOC;

  p->is_leaf = is_leaf;
  p->length = 0;
  p->byte_size = 0;

  /* this two fields will be changed on page_write */
  p->offset = offset;
  p->config = config;

  p->buff_ = NULL;

  *page = p;
  return 0;
}


int bp__page_destroy(bp_tree_t* t, bp__page_t* page) {
  /* Free all keys */
  int i = 0;
  for (i = 0; i < t->head_page->length; i++) {
    if (t->head_page->keys[i].value != NULL &&
        t->head_page->keys[i].allocated) {
      free(t->head_page->keys[i].value);
    }
  }

  if (page->buff_ != NULL) {
    free(page->buff_);
  }

  /* Free page itself */
  free(page);
  return 0;
}


int bp__page_load(bp_tree_t* t, bp__page_t* page) {
  bp__writer_t* w = (bp__writer_t*) t;

  int ret;

  /* Read page size and leaf flag */
  uint32_t size = page->config & 0x7fffffff;
  page->is_leaf = page->config >> 31;

  /* Read page data */
  char* buff = malloc(size);
  if (buff == NULL) return BP_EALLOC;

  ret = bp__writer_read(w, page->offset, size, buff);
  if (ret) return ret;

  /* Parse data */
  uint32_t i = 0, o = 0;
  while (o < size) {
    page->keys[i].length = ntohl(*((uint32_t*) (buff + o)));
    page->keys[i].offset = ntohl(*((uint32_t*) (buff + o + 4)));
    page->keys[i].config = ntohl(*((uint32_t*) (buff + o + 8)));
    page->keys[i].value = buff + o + 12;
    page->keys[i].allocated = 0;

    o += 12 + page->keys[i].length;
    i++;
  }
  page->length = i;

  if (page->buff_ != NULL) {
    free(page->buff_);
  }
  page->buff_ = buff;

  return 0;
}


int bp__page_save(bp_tree_t* t, bp__page_t* page) {
  bp__writer_t* w = (bp__writer_t*) t;

  /* Allocate space for serialization (header + keys); */
  char* buff = malloc(page->byte_size);

  uint32_t i, o = 0;
  for (i = 0; i < page->length; i++) {
    *((uint32_t*) (buff + o)) = htonl(page->keys[i].length);
    *((uint32_t*) (buff + o + 4)) = htonl(page->keys[i].offset);
    *((uint32_t*) (buff + o + 8)) = htonl(page->keys[i].config);

    memcpy(buff + o + BP__KV_HEADER_SIZE,
           page->keys[i].value,
           page->keys[i].length);

    o += BP__KV_HEADER_SIZE + page->keys[i].length;
  }

  int ret = 0;
  ret = bp__writer_write(w,  page->byte_size, buff, &page->offset);
  free(buff);

  if (ret) return ret;

  page->config = page->byte_size | (page->is_leaf << 31);

  return 0;
}


int bp__page_insert(bp_tree_t* t, bp__page_t* page, bp__kv_t* kv) {
  uint32_t i;
  int cmp = 1;
  for (i = 0; i < page->length; i++) {
    cmp = t->compare_cb((bp_key_t*) &page->keys[i], (bp_key_t*) kv);

    if (cmp >= 0) {
      break;
    }
  }

  int ret;

  if (page->is_leaf) {
    /* TODO: Save reference to previous value */
    if (cmp == 0) bp__page_remove_idx(t, page, i);

    /* Shift all keys right */
    uint32_t j;
    if (page->length >= 1) {
      for (j = page->length - 1; j >= i; j--) {
        bp__kv_copy(&page->keys[j], &page->keys[j + 1], 0);

        if (j == 0) break;
      }
    }

    /* Insert key in the middle */
    bp__kv_copy(kv, &page->keys[i], 1);
    page->byte_size += BP__KV_HEADER_SIZE + kv->length;
    page->length++;
  } else {
    /* Insert kv in child page */
    bp__page_t* child;
    ret = bp__page_create(t, 0, page->keys[i].offset, page->keys[i].config,
                          &child);
    if (ret) return ret;

    ret = bp__page_load(t, child);
    if (ret) return ret;

    ret = bp__page_insert(t, child, kv);

    if (ret && ret != BP_ESPLITPAGE) {
      return ret;
    }

    /* kv was inserted but page is full now */
    if (ret == BP_ESPLITPAGE) {
      ret = bp__page_split(t, page, child);
    } else {
      /* we don't need child now */
      ret = bp__page_destroy(t, child);
    }
    if (ret) return ret;
  }

  /* Notify caller that it should split page */
  if (page->length == t->head.page_size) return BP_ESPLITPAGE;

  ret = bp__page_save(t, page);
  if (ret) return ret;

  return 0;
}


int bp__page_remove_idx(bp_tree_t* t, bp__page_t* page, uint32_t index) {
  /* Free memory allocated for kv and reduce byte_size of page */
  page->byte_size -= page->keys[index].length + BP__KV_HEADER_SIZE;
  if (page->keys[index].allocated) {
    free(page->keys[index].value);
  }

  /* Shift all keys left */
  uint32_t i;
  for (i = index + 1; i < page->length; i++) {
    bp__kv_copy(&page->keys[i], &page->keys[i - 1], 0);
  }

  page->length--;

  return 0;
}


int bp__page_remove(bp_tree_t* t, bp__page_t* page, bp__kv_t* kv) {
  uint32_t i;
  for (i = 0; i < page->length; i++) {
    int cmp = t->compare_cb((bp_key_t*) &page->keys[i], (bp_key_t*) kv);

    if (cmp >= 0) {
      if (cmp == 0) {
        return bp__page_remove_idx(t, page, i);
      }
      break;
    }
  }

  return 0;
}


int bp__page_split(bp_tree_t* t, bp__page_t* parent, bp__page_t* child) {
  bp__page_t* left;
  bp__page_t* right;

  bp__page_create(t, child->is_leaf, 0, 0, &left);
  bp__page_create(t, child->is_leaf, 0, 0, &right);

  uint32_t middle = t->head.page_size >> 1;
  bp__kv_t middle_key;
  bp__kv_copy(&child->keys[middle], &middle_key, 1);

  uint32_t i;
  for (i = 0; i < middle; i++) {
    bp__kv_copy(&child->keys[i], &left->keys[i], 0);
  }

  for (; i < t->head.page_size; i++) {
    bp__kv_copy(&child->keys[i], &right->keys[i - middle], 0);
  }

  int ret;
  ret = bp__page_save(t, left);
  if (ret) return ret;
  ret = bp__page_save(t, right);
  if (ret) return ret;
  ret = bp__page_destroy(t, left);
  if (ret) return ret;
  ret = bp__page_destroy(t, right);
  if (ret) return ret;

  ret = bp__page_insert(t, parent, &middle_key);
  /* ignore split page here */
  if (ret == BP_ESPLITPAGE) {
    ret = 0;
  }

  return ret;
}


int bp__kv_copy(bp__kv_t* source, bp__kv_t* target, int alloc) {
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

  return 0;
}
