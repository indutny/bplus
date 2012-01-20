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
  if (is_leaf) {
    p->length = 0;
    p->byte_size = 0;
  } else {
    /* non-leaf pages always have left element */
    p->length = 1;
    p->keys[0].value = NULL;
    p->keys[0].length = 0;
    p->keys[0].offset = 0;
    p->keys[0].config = 0;
    p->byte_size = BP__KV_SIZE(p->keys[0]);
  }

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
  for (i = 0; i < page->length; i++) {
    if (page->keys[i].value != NULL &&
        page->keys[i].allocated) {
      free(page->keys[i].value);
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
    page->keys[i].length = ntohl(*(uint32_t*) (buff + o));
    page->keys[i].offset = ntohl(*(uint32_t*) (buff + o + 4));
    page->keys[i].config = ntohl(*(uint32_t*) (buff + o + 8));
    page->keys[i].value = buff + o + 12;
    page->keys[i].allocated = 0;

    o += 12 + page->keys[i].length;
    i++;
  }
  page->length = i;
  page->byte_size = size;

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
  if (buff == NULL) return BP_EALLOC;

  uint32_t i, o = 0;
  for (i = 0; i < page->length; i++) {
    *(uint32_t*) (buff + o) = htonl(page->keys[i].length);
    *(uint32_t*) (buff + o + 4) = htonl(page->keys[i].offset);
    *(uint32_t*) (buff + o + 8) = htonl(page->keys[i].config);

    memcpy(buff + o + 12, page->keys[i].value, page->keys[i].length);

    o += BP__KV_SIZE(page->keys[i]);
  }
  assert(o == page->byte_size);

  int ret = 0;
  ret = bp__writer_write(w, page->byte_size, buff, &page->offset);
  free(buff);

  if (ret) return ret;

  page->config = page->byte_size | (page->is_leaf << 31);

  return 0;
}


int bp__page_find(bp_tree_t* t,
                  bp__page_t* page,
                  const bp__kv_t* kv,
                  bp_value_t* value) {
  uint32_t i = page->is_leaf ? 0 : 1;
  int cmp = -1;
  while (i < page->length) {
    /* left key is always lower in non-leaf nodes */
    cmp = t->compare_cb((bp_key_t*) &page->keys[i], (bp_key_t*) kv);

    if (cmp >= 0) break;
    i++;
  }

  if (page->is_leaf) {
    if (cmp != 0) return BP_ENOTFOUND;

    value->length = page->keys[i].config;
    value->value = malloc(value->length);
    if (value->value == NULL) return BP_EALLOC;

    return bp__writer_read((bp__writer_t*) t,
                           page->keys[i].offset,
                           page->keys[i].config,
                           value->value);
  } else {
    assert(i > 0);
    i--;

    bp__page_t* child;
    int ret;
    ret = bp__page_create(t, 0, page->keys[i].offset, page->keys[i].config,
                          &child);
    if (ret) return ret;

    ret = bp__page_load(t, child);
    if (ret) return ret;

    return bp__page_find(t, child, kv, value);
  }
}


int bp__page_insert(bp_tree_t* t, bp__page_t* page, const bp__kv_t* kv) {
  uint32_t i = page->is_leaf ? 0 : 1;
  int cmp = -1;
  while (i < page->length) {
    /* left key is always lower in non-leaf nodes */
    cmp = t->compare_cb((bp_key_t*) &page->keys[i], (bp_key_t*) kv);

    if (cmp >= 0) break;
    i++;
  }

  int ret;

  if (page->is_leaf) {
    /* TODO: Save reference to previous value */
    if (cmp == 0) bp__page_remove_idx(t, page, i);

    /* Shift all keys right */
    bp__page_shiftr(t, page, i);

    /* Insert key in the middle */
    bp__kv_copy(kv, &page->keys[i], 1);
    page->byte_size += BP__KV_SIZE((*kv));
    page->length++;
  } else {
    /* non-leaf pages have left key that is always less than any other */
    assert(i > 0);
    i--;

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
      ret = bp__page_split(t, page, i, child);
    } else {
      /* Update offsets in page */
      page->keys[i].offset = child->offset;
      page->keys[i].config = child->config;

      /* we don't need child now */
      ret = bp__page_destroy(t, child);
    }
    if (ret) return ret;
  }

  if (page->length == t->head.page_size) {
    if (page == t->head_page) {
      /* split root */
      bp__page_t* new_root;
      bp__page_create(t, 0, 0, 0, &new_root);

      ret = bp__page_split(t, new_root, 0, page);
      if (ret) return ret;

      t->head_page = new_root;
      page = new_root;
    } else {
      /* Notify caller that it should split page */
      return BP_ESPLITPAGE;
    }
  }

  ret = bp__page_save(t, page);
  if (ret) return ret;

  return 0;
}


int bp__page_remove_idx(bp_tree_t* t, bp__page_t* page, const uint32_t index) {
  assert(index < page->length);

  /* Free memory allocated for kv and reduce byte_size of page */
  page->byte_size -= BP__KV_SIZE(page->keys[index]);
  if (page->keys[index].allocated) {
    free(page->keys[index].value);
  }

  /* Shift all keys left */
  bp__page_shiftl(t, page, index);

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


int bp__page_split(bp_tree_t* t,
                   bp__page_t* parent,
                   const uint32_t index,
                   bp__page_t* child) {
  bp__page_t* left;
  bp__page_t* right;

  bp__page_create(t, child->is_leaf, 0, 0, &left);
  bp__page_create(t, child->is_leaf, 0, 0, &right);

  uint32_t middle = t->head.page_size >> 1;
  bp__kv_t middle_key;
  bp__kv_copy(&child->keys[middle], &middle_key, 1);

  uint32_t i;
  for (i = 0; i < middle; i++) {
    bp__kv_copy(&child->keys[i], &left->keys[left->length++], 1);
    left->byte_size += BP__KV_SIZE(child->keys[i]);
  }

  for (; i < t->head.page_size; i++) {
    bp__kv_copy(&child->keys[i], &right->keys[right->length++], 1);
    right->byte_size += BP__KV_SIZE(child->keys[i]);
  }

  /* save left and right parts to get offsets */
  int ret;
  ret = bp__page_save(t, left);

  if (ret) return ret;
  ret = bp__page_save(t, right);
  if (ret) return ret;

  /* store offsets with middle key */
  middle_key.offset = right->offset;
  middle_key.config = right->config;

  /* insert middle key into parent page */
  bp__page_shiftr(t, parent, index + 1);
  bp__kv_copy(&middle_key, &parent->keys[index + 1], 0);
  parent->byte_size += BP__KV_SIZE(middle_key);
  parent->length++;

  /* change left element */
  parent->keys[index].offset = left->offset;
  parent->keys[index].config = left->config;

  /* cleanup */
  ret = bp__page_destroy(t, left);
  if (ret) return ret;
  ret = bp__page_destroy(t, right);
  if (ret) return ret;

  /* caller should not care of child */
  ret = bp__page_destroy(t, child);
  if (ret) return ret;

  return ret;
}


void bp__page_shiftr(bp_tree_t* t, bp__page_t* p, const uint32_t index) {
  uint32_t i;

  if (p->length != 0) {
    for (i = p->length - 1; i >= index; i--) {
      bp__kv_copy(&p->keys[i], &p->keys[i + 1], 0);

      if (i == 0) break;
    }
  }
}


void bp__page_shiftl(bp_tree_t* t, bp__page_t* p, const uint32_t index) {
  uint32_t i;
  for (i = index + 1; i < p->length; i++) {
    bp__kv_copy(&p->keys[i], &p->keys[i - 1], 0);
  }
}


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

  return 0;
}
