#include <stdlib.h> /* malloc */
#include <stdio.h> /* fopen, fclose, fread, fwrite, ... */
#include <string.h> /* strlen */
#include <arpa/inet.h> /* nothl, htonl */

#include "bplus.h"
#include "private/utils.h"

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

  return BP_OK;
}


int bp_close(bp_tree_t* tree) {
  int ret;
  ret = bp__writer_destroy((bp__writer_t*) tree);

  if (ret) return ret;
  ret = bp__page_destroy(tree, tree->head_page);

  return BP_OK;
}


int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value) {
  return bp__page_get(tree, tree->head_page, (bp__kv_t*) key, value);
}


int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value) {
  int ret;
  bp__kv_t kv;

  /* store value in db file to get offset and config */
  ret = bp__writer_write((bp__writer_t*) tree,
                         kCompressed,
                         value->length,
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


void bp_set_compare_cb(bp_tree_t* tree, bp_compare_cb cb) {
  tree->compare_cb = cb;
}


int bp__tree_read_head(bp__writer_t* w, void* data) {
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t* head = (bp__tree_head_t*) data;

  t->head.offset = ntohl(head->offset);
  t->head.config = ntohl(head->config);
  t->head.page_size = ntohl(head->page_size);
  t->head.hash = ntohl(head->hash);

  /* we've copied all data - free it */
  free(data);

  /* Check hash first */
  if (bp__compute_hash(t->head.offset) != t->head.hash) return 1;

  if (t->head_page == NULL) {
    bp__page_create(t, 1, t->head.offset, t->head.config, &t->head_page);
  }
  return bp__page_load(t, t->head_page);
}


int bp__tree_write_head(bp__writer_t* w, void* data) {
  int ret;
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t nhead;
  uint32_t offset;
  uint32_t csize;

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

  t->head.hash = bp__compute_hash(t->head.offset);

  /* Create temporary head with fields in network byte order */
  nhead.offset = htonl(t->head.offset);
  nhead.config = htonl(t->head.config);
  nhead.page_size = htonl(t->head.page_size);
  nhead.hash = htonl(t->head.hash);

  ret = bp__writer_write(w,
                         kNotCompressed,
                         sizeof(nhead),
                         &nhead,
                         &offset,
                         &csize);

  return ret;
}
