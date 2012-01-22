#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */

#include "bplus.h"
#include "private/pages.h"
#include "private/utils.h"

int bp__page_create(bp_tree_t* t,
                    const enum page_type type,
                    const uint64_t offset,
                    const uint64_t config,
                    bp__page_t** page) {
  /* Allocate space for page + keys */
  bp__page_t* p;

  p = malloc(sizeof(*p) + sizeof(p->keys[0]) * (t->head.page_size - 1));
  if (p == NULL) return BP_EALLOC;

  p->type = type;
  if (type == kLeaf) {
    p->length = 0;
    p->byte_size = 0;
  } else {
    /* non-leaf pages always have left element */
    p->length = 1;
    p->keys[0].value = NULL;
    p->keys[0].length = 0;
    p->keys[0].offset = 0;
    p->keys[0].config = 0;
    p->keys[0].allocated = 0;
    p->byte_size = BP__KV_SIZE(p->keys[0]);
  }

  /* this two fields will be changed on page_write */
  p->offset = offset;
  p->config = config;

  p->buff_ = NULL;

  *page = p;
  return BP_OK;
}


void bp__page_destroy(bp_tree_t* t, bp__page_t* page) {
  /* Free all keys */
  uint64_t i = 0;
  for (i = 0; i < page->length; i++) {
    if (page->keys[i].value != NULL &&
        page->keys[i].allocated) {
      free(page->keys[i].value);
      page->keys[i].value = NULL;
    }
  }

  if (page->buff_ != NULL) {
    free(page->buff_);
    page->buff_ = NULL;
  }

  /* Free page itself */
  free(page);
}


int bp__page_read(bp_tree_t* t, bp__page_t* page) {
  int ret;
  uint64_t size, o;
  uint64_t i;
  bp__writer_t* w = (bp__writer_t*) t;

  char* buff = NULL;

  /* Read page size and leaf flag */
  size = page->config >> 1;
  page->type = page->config & 1 ? kLeaf : kPage;

  /* Read page data */
  ret = bp__writer_read(w, kCompressed, page->offset, &size, (void**) &buff);
  if (ret != BP_OK) return ret;

  /* Parse data */
  i = 0;
  o = 0;
  while (o < size) {
    page->keys[i].length = ntohll(*(uint64_t*) (buff + o));
    page->keys[i].offset = ntohll(*(uint64_t*) (buff + o + 8));
    page->keys[i].config = ntohll(*(uint64_t*) (buff + o + 16));
    page->keys[i].value = buff + o + 24;
    page->keys[i].allocated = 0;

    o += BP__KV_SIZE(page->keys[i]);
    i++;
  }
  page->length = i;
  page->byte_size = size;

  if (page->buff_ != NULL) {
    free(page->buff_);
  }
  page->buff_ = buff;

  return BP_OK;
}


int bp__page_load(bp_tree_t* t,
                  const uint64_t offset,
                  const uint64_t config,
                  bp__page_t** page) {
  int ret;

  bp__page_t* new_page;
  ret = bp__page_create(t, 0, offset, config, &new_page);
  if (ret != BP_OK) return ret;

  ret = bp__page_read(t, new_page);
  if (ret != BP_OK) {
    bp__page_destroy(t, new_page);
    return ret;
  }

  /* bp__page_load should be atomic */
  *page = new_page;

  return BP_OK;
}


int bp__page_save(bp_tree_t* t, bp__page_t* page) {
  int ret;
  bp__writer_t* w = (bp__writer_t*) t;
  uint64_t i;
  uint64_t o;
  char* buff;

  assert(page->type == kLeaf || page->length != 0);

  /* Allocate space for serialization (header + keys); */
  buff = malloc(page->byte_size);
  if (buff == NULL) return BP_EALLOC;

  o = 0;
  for (i = 0; i < page->length; i++) {
    assert(o + BP__KV_SIZE(page->keys[i]) <= page->byte_size);

    *(uint64_t*) (buff + o) = htonll(page->keys[i].length);
    *(uint64_t*) (buff + o + 8) = htonll(page->keys[i].offset);
    *(uint64_t*) (buff + o + 16) = htonll(page->keys[i].config);

    memcpy(buff + o + 24, page->keys[i].value, page->keys[i].length);

    o += BP__KV_SIZE(page->keys[i]);
  }
  assert(o == page->byte_size);

  page->config = page->byte_size;
  ret = bp__writer_write(w,
                         kCompressed,
                         buff,
                         &page->offset,
                         &page->config);
  page->config = (page->config << 1) | (page->type == kLeaf);

  free(buff);
  return ret;
}


int bp__page_load_value(bp_tree_t* t,
                        bp__page_t* page,
                        const uint64_t index,
                        bp_value_t* value,
                        bp__kv_t* previous) {
  return bp__value_load(t,
                        page->keys[index].offset,
                        page->keys[index].config,
                        value,
                        previous);
}


int bp__page_save_value(bp_tree_t* t,
                        bp__page_t* page,
                        const uint64_t index,
                        const int cmp,
                        const bp__kv_t* key,
                        const bp_value_t* value) {
  int ret;
  bp__kv_t previous, tmp;

  /* remove item with same key from page */
  if (cmp == 0) {
    previous.offset = page->keys[index].offset;
    previous.config = page->keys[index].config;
    bp__page_remove_idx(t, page, index);
  }

  /* store key */
  tmp.value = key->value;
  tmp.length = key->length;

  /* store value */
  ret = bp__value_save(t,
                       value,
                       cmp == 0 ? &previous : NULL,
                       &tmp.offset,
                       &tmp.config);
  if (ret != BP_OK) return ret;

  /* Shift all keys right */
  bp__page_shiftr(t, page, index);

  /* Insert key in the middle */
  ret = bp__kv_copy(&tmp, &page->keys[index], 1);
  if (ret != BP_OK) {
    /* shift keys back */
    bp__page_shiftl(t, page, index);
    return ret;
  }

  page->byte_size += BP__KV_SIZE(tmp);
  page->length++;

  return BP_OK;
}


int bp__page_search(bp_tree_t* t,
                    bp__page_t* page,
                    const bp__kv_t* kv,
                    const enum search_type type,
                    bp__page_search_res_t* result) {
  int ret;
  uint64_t i = page->type == kPage;
  int cmp = -1;
  bp__page_t* child;

  while (i < page->length) {
    /* left key is always lower in non-leaf nodes */
    cmp = t->compare_cb((bp_key_t*) &page->keys[i], (bp_key_t*) kv);

    if (cmp >= 0) break;
    i++;
  }

  result->cmp = cmp;

  if (page->type == kLeaf) {
    result->index = i;
    result->child = NULL;

    return BP_OK;
  } else {
    assert(i > 0);
    if (cmp != 0) i--;

    if (type == kLoad) {
      ret = bp__page_load(t,
                          page->keys[i].offset,
                          page->keys[i].config,
                          &child);
      if (ret != BP_OK) return ret;

      result->child = child;
    } else {
      result->child = NULL;
    }

    result->index = i;

    return BP_OK;
  }
}


int bp__page_get(bp_tree_t* t,
                 bp__page_t* page,
                 const bp__kv_t* kv,
                 bp_value_t* value) {
  int ret;
  bp__page_search_res_t res;
  ret = bp__page_search(t, page, kv, kLoad, &res);
  if (ret != BP_OK) return ret;

  if (res.child == NULL) {
    if (res.cmp != 0) return BP_ENOTFOUND;

    return bp__page_load_value(t, page, res.index, value, NULL);
  } else {
    ret = bp__page_get(t, res.child, kv, value);
    bp__page_destroy(t, res.child);
    res.child = NULL;
    return ret;
  }
}


int bp__page_get_range(bp_tree_t* t,
                       bp__page_t* page,
                       const bp__kv_t* start,
                       const bp__kv_t* end,
                       bp_filter_cb filter,
                       bp_range_cb cb) {
  int ret;
  uint64_t i;
  bp__page_search_res_t start_res, end_res;

  /* find start and end indexes */
  ret = bp__page_search(t, page, start, kNotLoad, &start_res);
  if (ret != BP_OK) return ret;
  ret = bp__page_search(t, page, end, kNotLoad, &end_res);
  if (ret != BP_OK) return ret;

  if (page->type == kLeaf) {
    /* on leaf pages end-key should always be greater or equal than first key */
    if (end_res.cmp > 0 && end_res.index == 0) return BP_OK;

    if (end_res.cmp < 0) end_res.index--;
  }

  /* go through each page item */
  for (i = start_res.index; i <= end_res.index; i++) {
    /* run filter */
    if (!filter((bp_key_t*) &page->keys[i])) continue;

    if (page->type == kPage) {
      /* load child page and apply range get to it */
      bp__page_t* child;

      ret = bp__page_load(t,
                          page->keys[i].offset,
                          page->keys[i].config,
                          &child);
      if (ret != BP_OK) return ret;

      ret = bp__page_get_range(t, child, start, end, filter, cb);

      /* destroy child regardless of error */
      bp__page_destroy(t, child);

      if (ret != BP_OK) return ret;
    } else {
      /* load value and pass it to callback */
      bp_value_t value;
      ret = bp__page_load_value(t, page, i, &value, NULL);
      if (ret != BP_OK) return ret;

      cb((bp_key_t*) &page->keys[i], &value);

      free(value.value);
    }
  }

  return BP_OK;
}


int bp__page_insert(bp_tree_t* t,
                    bp__page_t* page,
                    const bp__kv_t* kv,
                    const bp_value_t* value) {
  int ret;
  bp__page_search_res_t res;
  ret = bp__page_search(t, page, kv, kLoad, &res);
  if (ret != BP_OK) return ret;

  if (res.child == NULL) {
    /* store value in db file to get offset and config */
    ret = bp__page_save_value(t, page, res.index, res.cmp, kv, value);
    if (ret != BP_OK) return ret;
  } else {
    /* Insert kv in child page */
    ret = bp__page_insert(t, res.child, kv, value);

    if (ret != BP_OK && ret != BP_ESPLITPAGE) {
      return ret;
    }

    /* kv was inserted but page is full now */
    if (ret == BP_ESPLITPAGE) {
      ret = bp__page_split(t, page, res.index, res.child);
      if (ret != BP_OK) return ret;
    } else {
      /* Update offsets in page */
      page->keys[res.index].offset = res.child->offset;
      page->keys[res.index].config = res.child->config;

      /* we don't need child now */
      bp__page_destroy(t, res.child);
      res.child = NULL;
    }
  }

  if (page->length == t->head.page_size) {
    if (page == t->head.page) {
      /* split root */
      bp__page_t* new_root = NULL;
      bp__page_create(t, 0, 0, 0, &new_root);

      ret = bp__page_split(t, new_root, 0, page);
      if (ret != BP_OK) return ret;

      t->head.page = new_root;
      page = new_root;
    } else {
      /* Notify caller that it should split page */
      return BP_ESPLITPAGE;
    }
  }

  assert(page->length < t->head.page_size);

  ret = bp__page_save(t, page);
  if (ret != BP_OK) return ret;

  return BP_OK;
}


int bp__page_remove(bp_tree_t* t, bp__page_t* page, const bp__kv_t* kv) {
  int ret;
  bp__page_search_res_t res;
  ret = bp__page_search(t, page, kv, kLoad, &res);
  if (ret != BP_OK) return ret;

  if (res.child == NULL) {
    if (res.cmp != 0) return BP_ENOTFOUND;
    bp__page_remove_idx(t, page, res.index);

    if (page->length == 0 && t->head.page != page) return BP_EEMPTYPAGE;
  } else {
    /* Insert kv in child page */
    ret = bp__page_remove(t, res.child, kv);

    if (ret != BP_OK && ret != BP_EEMPTYPAGE) {
      return ret;
    }

    /* kv was inserted but page is full now */
    if (ret == BP_EEMPTYPAGE) {
      bp__page_remove_idx(t, page, res.index);

      /* we don't need child now */
      bp__page_destroy(t, res.child);
      res.child = NULL;

      /* only one item left - lift kv from last child to current page */
      if (page->length == 1) {
        page->offset = page->keys[0].offset;
        page->config = page->keys[0].config;

        /* remove child to free memory */
        bp__page_remove_idx(t, page, 0);

        /* and load child as current page */
        ret = bp__page_read(t, page);
        if (ret != BP_OK) return ret;
      }
    } else {
      /* Update offsets in page */
      page->keys[res.index].offset = res.child->offset;
      page->keys[res.index].config = res.child->config;

      /* we don't need child now */
      bp__page_destroy(t, res.child);
      res.child = NULL;
    }
  }

  return bp__page_save(t, page);
}


int bp__page_copy(bp_tree_t* source, bp_tree_t* target, bp__page_t* page) {
  int ret;
  uint64_t i;
  for (i = 0; i < page->length; i++) {
    if (page->type == kPage) {
      /* copy child page */
      bp__page_t* child;
      ret = bp__page_load(source,
                          page->keys[i].offset,
                          page->keys[i].config,
                          &child);
      if (ret != BP_OK) return ret;

      ret = bp__page_copy(source, target, child);
      if (ret != BP_OK) return ret;

      /* update child position */
      page->keys[i].offset = child->offset;
      page->keys[i].config = child->config;

      bp__page_destroy(source, child);
    } else {
      /* copy value */
      bp_value_t value;

      ret = bp__page_load_value(source, page, i, &value, NULL);
      if (ret != BP_OK) return ret;

      page->keys[i].config = value.length;
      ret = bp__value_save(target,
                           &value,
                           NULL,
                           &page->keys[i].offset,
                           &page->keys[i].config);

      /* value is not needed anymore */
      free(value.value);
      if (ret != BP_OK) return ret;
    }
  }

  return bp__page_save(target, page);
}


int bp__page_remove_idx(bp_tree_t* t, bp__page_t* page, const uint64_t index) {
  assert(index < page->length);

  /* Free memory allocated for kv and reduce byte_size of page */
  page->byte_size -= BP__KV_SIZE(page->keys[index]);
  if (page->keys[index].value != NULL && page->keys[index].allocated) {
    free(page->keys[index].value);
    page->keys[index].value = NULL;
  }

  /* Shift all keys left */
  bp__page_shiftl(t, page, index);

  page->length--;

  return BP_OK;
}


int bp__page_split(bp_tree_t* t,
                   bp__page_t* parent,
                   const uint64_t index,
                   bp__page_t* child) {
  int ret;
  uint64_t i, middle;
  bp__page_t* left = NULL;
  bp__page_t* right = NULL;
  bp__kv_t middle_key;

  bp__page_create(t, child->type, 0, 0, &left);
  bp__page_create(t, child->type, 0, 0, &right);

  middle = t->head.page_size >> 1;
  ret = bp__kv_copy(&child->keys[middle], &middle_key, 1);
  if (ret != BP_OK) goto fatal;

  /* non-leaf nodes has byte_size > 0 nullify it first */
  left->byte_size = 0;
  left->length = 0;
  for (i = 0; i < middle; i++) {
    ret = bp__kv_copy(&child->keys[i], &left->keys[left->length++], 1);
    if (ret != BP_OK) goto fatal;
    left->byte_size += BP__KV_SIZE(child->keys[i]);
  }

  right->byte_size = 0;
  right->length = 0;
  for (; i < t->head.page_size; i++) {
    ret = bp__kv_copy(&child->keys[i], &right->keys[right->length++], 1);
    if (ret != BP_OK) goto fatal;
    right->byte_size += BP__KV_SIZE(child->keys[i]);
  }

  /* save left and right parts to get offsets */
  ret = bp__page_save(t, left);
  if (ret != BP_OK) return ret;

  ret = bp__page_save(t, right);
  if (ret != BP_OK) return ret;

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
  bp__page_destroy(t, left);
  bp__page_destroy(t, right);

  /* caller should not care of child */
  bp__page_destroy(t, child);

  return BP_OK;
fatal:
  /* cleanup */
  bp__page_destroy(t, left);
  bp__page_destroy(t, right);

  /* caller should not care of child */
  bp__page_destroy(t, child);
  return ret;
}


void bp__page_shiftr(bp_tree_t* t, bp__page_t* p, const uint64_t index) {
  uint64_t i;

  if (p->length != 0) {
    for (i = p->length - 1; i >= index; i--) {
      bp__kv_copy(&p->keys[i], &p->keys[i + 1], 0);

      if (i == 0) break;
    }
  }
}


void bp__page_shiftl(bp_tree_t* t, bp__page_t* p, const uint64_t index) {
  uint64_t i;
  for (i = index + 1; i < p->length; i++) {
    bp__kv_copy(&p->keys[i], &p->keys[i - 1], 0);
  }
}
