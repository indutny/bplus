#include <stdlib.h> /* malloc */
#include <stdio.h> /* fopen, fclose, fread, fwrite, ... */
#include <string.h> /* strlen */
#include <arpa/inet.h> /* nothl, htonl */

#include "bplus.h"
#include "private/utils.h"


int bp__default_compare_cb(const bp_key_t* a, const bp_key_t* b);
int bp__default_filter_cb(const bp_key_t* key);


int bp_open(bp_tree_t* tree, const char* filename) {
  int ret;
  ret = bp__writer_create((bp__writer_t*) tree, filename);
  if (ret) return ret;

  tree->head_page = NULL;

  /*
   * Load head.
   * Writer will not compress data chunk smaller than head,
   * that's why we're passing head size as compressed size here
   */
  ret = bp__writer_find((bp__writer_t*) tree,
                        kNotCompressed,
                        sizeof(tree->head),
                        &tree->head,
                        bp__tree_read_head,
                        bp__tree_write_head);
  if (ret) return ret;

  /* set default compare function */
  bp_set_compare_cb(tree, bp__default_compare_cb);

  return BP_OK;
}


int bp_close(bp_tree_t* tree) {
  int ret;
  ret = bp__writer_destroy((bp__writer_t*) tree);

  if (ret) return ret;
  if (tree->head_page != NULL) {
    bp__page_destroy(tree, tree->head_page);
    tree->head_page = NULL;
  }

  return BP_OK;
}


int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value) {
  return bp__page_get(tree, tree->head_page, (bp__kv_t*) key, value);
}


int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value) {
  int ret;
  bp__kv_t kv;

  /* store value in db file to get offset and config */
  kv.config = value->length;
  ret = bp__writer_write((bp__writer_t*) tree,
                         kCompressed,
                         value->value,
                         &kv.offset,
                         &kv.config);
  if (ret) return ret;

  kv.length = key->length;
  kv.value = key->value;

  ret = bp__page_insert(tree, tree->head_page, &kv);
  if (ret) return ret;

  return bp__tree_write_head((bp__writer_t*) tree, &tree->head);
}


int bp_remove(bp_tree_t* tree, const bp_key_t* key) {
  int ret;
  ret = bp__page_remove(tree, tree->head_page, (bp__kv_t*) key);
  if (ret) return ret;

  return bp__tree_write_head((bp__writer_t*) tree, &tree->head);
}


int bp_compact(bp_tree_t* tree) {
  int ret;
  char* compacted_name;
  bp_tree_t compacted;
  bp__page_t* head_copy;

  /* get name of compacted database (prefixed with .compact) */
  ret = bp__writer_compact_name((bp__writer_t*) tree, &compacted_name);
  if (ret) return ret;

  /* open it */
  ret = bp_open(&compacted, compacted_name);
  free(compacted_name);
  if (ret) return ret;

  /* clone head for thread safety */
  ret = bp__page_create(tree,
                        0,
                        tree->head_page->offset,
                        tree->head_page->config,
                        &head_copy);
  if (ret) return ret;
  ret = bp__page_load(tree, head_copy);
  if (ret) return ret;

  /* copy all pages starting from root */
  ret = bp__page_copy(tree, &compacted, head_copy);
  if (ret) return ret;

  /* compacted tree already has a head page, free it first */
  free(compacted.head_page);
  compacted.head_page = head_copy;

  ret = bp__tree_write_head((bp__writer_t*) &compacted, &compacted.head);
  if (ret) return ret;

  return bp__writer_compact_finalize((bp__writer_t*) tree,
                                     (bp__writer_t*) &compacted);
}


/* Wrappers to allow string to string set/get/remove */


int bp_gets(bp_tree_t* tree, const char* key, char** value) {
  int ret;
  bp_key_t bkey;
  bp_value_t bvalue;

  bkey.value = (char*) key;
  bkey.length = strlen(key);

  ret = bp_get(tree, &bkey, &bvalue);
  if (ret) return ret;

  *value = bvalue.value;

  return BP_OK;
}


int bp_sets(bp_tree_t* tree, const char* key, const char* value) {
  bp_key_t bkey;
  bp_value_t bvalue;

  bkey.value = (char*) key;
  bkey.length = strlen(key);

  bvalue.value = (char*) value;
  bvalue.length = strlen(value);

  return bp_set(tree, &bkey, &bvalue);
}


int bp_removes(bp_tree_t* tree, const char* key) {
  bp_key_t bkey;
  bkey.value = (char*) key;
  bkey.length = strlen(key);

  return bp_remove(tree, &bkey);
}


int bp_get_filtered_range(bp_tree_t* tree,
                          const bp_key_t* start,
                          const bp_key_t* end,
                          bp_filter_cb filter,
                          bp_range_cb cb) {
  return bp__page_get_range(tree,
                            tree->head_page,
                            (bp__kv_t*) start,
                            (bp__kv_t*) end,
                            filter,
                            cb);
}


int bp_get_filtered_ranges(bp_tree_t* tree,
                           const char* start,
                           const char* end,
                           bp_filter_cb filter,
                           bp_range_cb cb) {
  bp_key_t bstart;
  bp_key_t bend;

  bstart.value = (char*) start;
  bstart.length = strlen(start);

  bend.value = (char*) end;
  bend.length = strlen(end);

  return bp_get_filtered_range(tree, &bstart, &bend, filter, cb);
}


int bp_get_range(bp_tree_t* tree,
                 const bp_key_t* start,
                 const bp_key_t* end,
                 bp_range_cb cb) {
  return bp_get_filtered_range(tree, start, end, bp__default_filter_cb, cb);
}


int bp_get_ranges(bp_tree_t* tree,
                  const char* start,
                  const char* end,
                  bp_range_cb cb) {
  return bp_get_filtered_ranges(tree, start, end, bp__default_filter_cb, cb);
}


void bp_set_compare_cb(bp_tree_t* tree, bp_compare_cb cb) {
  tree->compare_cb = cb;
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

  ret = bp__page_create(t, 1, t->head.offset, t->head.config, &t->head_page);
  if (ret) return ret;

  return bp__page_load(t, t->head_page);
}


int bp__tree_write_head(bp__writer_t* w, void* data) {
  int ret;
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t nhead;
  uint64_t offset;
  uint64_t size;

  if (t->head_page == NULL) {
    /* TODO: page size should be configurable */
    t->head.page_size = 64;

    /* Create empty leaf page */
    ret = bp__page_create(t, kLeaf, 0, 0, &t->head_page);
    if (ret) return ret;
  }

  /* Update head's position */
  t->head.offset = t->head_page->offset;
  t->head.config = t->head_page->config;

  t->head.hash = bp__compute_hashl(t->head.offset);

  /* Create temporary head with fields in network byte order */
  nhead.offset = htonll(t->head.offset);
  nhead.config = htonll(t->head.config);
  nhead.page_size = htonll(t->head.page_size);
  nhead.hash = htonll(t->head.hash);

  size = sizeof(nhead);
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
      return a->value[i] > b->value[i] ? 1 : -1;
    }
  }

  return a->length - b->length;
}


int bp__default_filter_cb(const bp_key_t* key) {
  /* default filter accepts all keys */
  return 1;
}
