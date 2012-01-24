#include <stdlib.h> /* malloc */
#include <string.h> /* strlen */

#include "bplus.h"
#include "private/utils.h"


int bp__default_compare_cb(const bp_key_t* a, const bp_key_t* b);
int bp__default_filter_cb(const bp_key_t* key);


int bp_open(bp_tree_t* tree, const char* filename) {
  int ret;
  ret = bp__writer_create((bp__writer_t*) tree, filename);
  if (ret != BP_OK) return ret;

  tree->head.page = NULL;
  tree->head.new_page = NULL;

  /*
   * Load head.
   * Writer will not compress data chunk smaller than head,
   * that's why we're passing head size as compressed size here
   */
  ret = bp__writer_find((bp__writer_t*) tree,
                        kNotCompressed,
                        BP__HEAD_SIZE,
                        &tree->head,
                        bp__tree_read_head,
                        bp__tree_write_head);
  if (ret != BP_OK) return ret;

  /* set default compare function */
  bp_set_compare_cb(tree, bp__default_compare_cb);

  return BP_OK;
}


int bp_close(bp_tree_t* tree) {
  int ret;
  ret = bp__writer_destroy((bp__writer_t*) tree);

  if (ret != BP_OK) return ret;
  if (tree->head.page != NULL) {
    bp__page_destroy(tree, tree->head.page);
    tree->head.page = NULL;
  }

  return BP_OK;
}


int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value) {
  return bp__page_get(tree, tree->head.page, key, value);
}


int bp_get_previous(bp_tree_t* tree,
                    const bp_value_t* value,
                    bp_value_t* previous) {
  if (value->_prev_offset == 0 && value->_prev_length == 0) {
    return BP_ENOTFOUND;
  }
  return bp__value_load(tree,
                        value->_prev_offset,
                        value->_prev_length,
                        previous);
}


int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value) {
  int ret;
  bp__page_t* clone;

  ret = bp__page_clone(tree, tree->head.page, &clone);
  if (ret != BP_OK) return ret;

  tree->head.new_page = NULL;
  ret = bp__page_insert(tree, clone, key, value);
  if (ret == BP_OK) ret = bp__tree_swap_head(tree, &clone);

  bp__page_destroy(tree, clone);
  return ret;
}


int bp_bulk_set(bp_tree_t* tree,
                const uint64_t count,
                const bp_key_t** keys,
                const bp_value_t** values) {
  int ret;
  bp__page_t* clone;
  bp_key_t* keys_iter = (bp_key_t*) *keys;
  bp_value_t* values_iter = (bp_value_t*) *values;
  uint64_t left = count;

  ret = bp__page_clone(tree, tree->head.page, &clone);
  if (ret != BP_OK) return ret;

  tree->head.new_page = NULL;
  ret = bp__page_bulk_insert(tree,
                             clone,
                             NULL,
                             &left,
                             &keys_iter,
                             &values_iter);
  if (ret == BP_OK) ret = bp__tree_swap_head(tree, &clone);

  bp__page_destroy(tree, clone);
  return ret;
}


int bp_remove(bp_tree_t* tree, const bp_key_t* key) {
  int ret;
  bp__page_t* clone;

  ret = bp__page_clone(tree, tree->head.page, &clone);
  if (ret != BP_OK) return ret;

  tree->head.new_page = NULL;
  ret = bp__page_remove(tree, clone, key);
  if (ret == BP_OK) ret = bp__tree_swap_head(tree, &clone);

  bp__page_destroy(tree, clone);
  return ret;
}


int bp_compact(bp_tree_t* tree) {
  int ret;
  char* compacted_name;
  bp_tree_t compacted;
  bp__page_t* head_page;
  bp__page_t* head_copy;

  /* get name of compacted database (prefixed with .compact) */
  ret = bp__writer_compact_name((bp__writer_t*) tree, &compacted_name);
  if (ret != BP_OK) return ret;

  /* open it */
  ret = bp_open(&compacted, compacted_name);
  free(compacted_name);
  if (ret != BP_OK) return ret;

  /* for multi-threaded env */
  head_page = tree->head.page;

  /* clone head for thread safety */
  ret = bp__page_load(tree,
                      head_page->offset,
                      head_page->config,
                      &head_copy);
  if (ret != BP_OK) return ret;

  /* copy all pages starting from root */
  ret = bp__page_copy(tree, &compacted, head_copy);
  if (ret != BP_OK) return ret;

  /* compacted tree already has a head page, free it first */
  free(compacted.head.page);
  compacted.head.page = head_copy;

  ret = bp__tree_write_head((bp__writer_t*) &compacted, &compacted.head);
  if (ret != BP_OK) return ret;

  return bp__writer_compact_finalize((bp__writer_t*) tree,
                                     (bp__writer_t*) &compacted);
}


int bp_get_filtered_range(bp_tree_t* tree,
                          const bp_key_t* start,
                          const bp_key_t* end,
                          bp_filter_cb filter,
                          bp_range_cb cb,
                          void* arg) {
  return bp__page_get_range(tree,
                            tree->head.page,
                            start,
                            end,
                            filter,
                            cb,
                            arg);
}


int bp_get_range(bp_tree_t* tree,
                 const bp_key_t* start,
                 const bp_key_t* end,
                 bp_range_cb cb,
                 void* arg) {
  return bp_get_filtered_range(tree,
                               start,
                               end,
                               bp__default_filter_cb,
                               cb,
                               arg);
}


/* Wrappers to allow string to string set/get/remove */


int bp_gets(bp_tree_t* tree, const char* key, char** value) {
  int ret;
  bp_key_t bkey;
  bp_value_t bvalue;

  BP__STOVAL(key, bkey);

  ret = bp_get(tree, &bkey, &bvalue);
  if (ret != BP_OK) return ret;

  *value = bvalue.value;

  return BP_OK;
}


int bp_sets(bp_tree_t* tree, const char* key, const char* value) {
  bp_key_t bkey;
  bp_value_t bvalue;

  BP__STOVAL(key, bkey);
  BP__STOVAL(value, bvalue);

  return bp_set(tree, &bkey, &bvalue);
}


int bp_bulk_sets(bp_tree_t* tree,
                 const uint64_t count,
                 const char** keys,
                 const char** values) {
  int ret;
  bp_key_t* bkeys;
  bp_value_t* bvalues;
  uint64_t i;

  /* allocated memory for keys/values */
  bkeys = malloc(sizeof(*bkeys) * count);
  if (bkeys == NULL) return BP_EALLOC;

  bvalues = malloc(sizeof(*bvalues) * count);
  if (bvalues == NULL) {
    free(bkeys);
    return BP_EALLOC;
  }

  /* copy keys/values to allocated memory */
  for (i = 0; i < count; i++) {
    BP__STOVAL(keys[i], bkeys[i]);
    BP__STOVAL(values[i], bvalues[i]);
  }

  ret = bp_bulk_set(tree,
                    count,
                    (const bp_key_t**) &bkeys,
                    (const bp_value_t**) &bvalues);

  free(bkeys);
  free(bvalues);

  return ret;
}


int bp_removes(bp_tree_t* tree, const char* key) {
  bp_key_t bkey;

  BP__STOVAL(key, bkey);

  return bp_remove(tree, &bkey);
}


int bp_get_filtered_ranges(bp_tree_t* tree,
                           const char* start,
                           const char* end,
                           bp_filter_cb filter,
                           bp_range_cb cb,
                           void* arg) {
  bp_key_t bstart;
  bp_key_t bend;

  BP__STOVAL(start, bstart);
  BP__STOVAL(end, bend);

  return bp_get_filtered_range(tree, &bstart, &bend, filter, cb, arg);
}


int bp_get_ranges(bp_tree_t* tree,
                  const char* start,
                  const char* end,
                  bp_range_cb cb,
                  void* arg) {
  return bp_get_filtered_ranges(tree,
                                start,
                                end,
                                bp__default_filter_cb,
                                cb,
                                arg);
}


/* various functions */


void bp_set_compare_cb(bp_tree_t* tree, bp_compare_cb cb) {
  tree->compare_cb = cb;
}


int bp_fsync(bp_tree_t* tree) {
  return bp__writer_fsync((bp__writer_t*) tree);
}


/* internal utils */


int bp__tree_swap_head(bp_tree_t* tree, bp__page_t** clone) {
  int ret;
  bp__page_t* tmp;

  /*
   * if tree was splitted -
   * clone was already destroyed, so don't care about it much
   */
  if (tree->head.new_page != NULL) {
    *clone = tree->head.new_page;
  }

  /* swap clone and original head */
  tmp = tree->head.page;
  tree->head.page = *clone;
  *clone = tmp;

  /* put new head position on disk */
  ret = bp__tree_write_head((bp__writer_t*) tree, &tree->head);

  /* revert changes if failed */
  if (ret != BP_OK) {
    tmp = tree->head.page;
    tree->head.page = *clone;
    *clone = tmp;
  }

  return ret;
}


int bp__tree_read_head(bp__writer_t* w, void* data) {
  int ret;
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t* head = (bp__tree_head_t*) data;

  t->head.offset = ntohll(head->offset);
  t->head.config = ntohll(head->config);
  t->head.page_size = ntohll(head->page_size);
  t->head.hash = ntohll(head->hash);

  /* we've copied all data - free it */
  free(data);

  /* Check hash first */
  if (bp__compute_hashl(t->head.offset) != t->head.hash) return 1;

  ret = bp__page_load(t, t->head.offset, t->head.config, &t->head.page);
  if (ret != BP_OK) return ret;

  t->head.page->is_head = 1;

  return ret;
}


int bp__tree_write_head(bp__writer_t* w, void* data) {
  int ret;
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t nhead;
  uint64_t offset;
  uint64_t size;

  if (t->head.page == NULL) {
    /* TODO: page size should be configurable */
    t->head.page_size = 64;

    /* Create empty leaf page */
    ret = bp__page_create(t, kLeaf, 0, 0, &t->head.page);
    if (ret != BP_OK) return ret;

    t->head.page->is_head = 1;
  }

  /* Update head's position */
  t->head.offset = t->head.page->offset;
  t->head.config = t->head.page->config;

  t->head.hash = bp__compute_hashl(t->head.offset);

  /* Create temporary head with fields in network byte order */
  nhead.offset = htonll(t->head.offset);
  nhead.config = htonll(t->head.config);
  nhead.page_size = htonll(t->head.page_size);
  nhead.hash = htonll(t->head.hash);

  size = BP__HEAD_SIZE;
  ret = bp__writer_write(w,
                         kNotCompressed,
                         &nhead,
                         &offset,
                         &size);

  return ret;
}


int bp__default_compare_cb(const bp_key_t* a, const bp_key_t* b) {
  uint32_t i, len = a->length < b->length ? a->length : b->length;

  for (i = 0; i < len; i++) {
    if (a->value[i] != b->value[i]) {
      return (uint8_t) a->value[i] > (uint8_t) b->value[i] ? 1 : -1;
    }
  }

  return a->length - b->length;
}


int bp__default_filter_cb(const bp_key_t* key) {
  /* default filter accepts all keys */
  return 1;
}
