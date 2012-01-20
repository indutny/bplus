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

  /* Load head */
  ret = bp__writer_find((bp__writer_t*) tree,
                        sizeof(tree->head),
                        &tree->head,
                        bp__tree_read_head,
                        bp__tree_write_head);
  if (ret) return ret;

  return 0;
}


int bp_close(bp_tree_t* tree) {
  int ret;
  ret = bp__writer_destroy((bp__writer_t*) tree);

  if (ret) return ret;
  ret = bp__page_destroy(tree, tree->head_page);

  return 0;
}


int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value) {
  int ret;
  return bp__page_find(tree, tree->head_page, (bp__kv_t*) key, value);
}


int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value) {
  bp__kv_t kv;

  int ret;
  ret = bp__writer_write((bp__writer_t*) tree, value->length, value->value,
                         &kv.offset);
  if (ret) return ret;

  kv.length = key->length;
  kv.value = key->value;
  kv.config = value->length;

  ret = bp__page_insert(tree, tree->head_page, &kv);
  if (ret) return ret;
  bp__tree_write_head((bp__writer_t*) tree, &tree->head);
}


int bp_remove(bp_tree_t* tree, const bp_key_t* key) {
  return 0;
}


/* Wrappers to allow string to string set/get/remove */


int bp_gets(bp_tree_t* tree, const char* key, char** value) {
  bp_key_t bkey;
  bkey.value = (char*) key;
  bkey.length = strlen(key);

  bp_value_t bvalue;

  int ret;
  ret = bp_get(tree, &bkey, &bvalue);
  if (ret) return ret;

  *value = bvalue.value;

  return 0;
}


int bp_sets(bp_tree_t* tree, const char* key, const char* value) {
  bp_key_t bkey;
  bkey.value = (char*) key;
  bkey.length = strlen(key);

  bp_value_t bvalue;
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
  bp__tree_head_t* head = (bp__tree_head_t*) data;

  head->offset = ntohl(head->offset);
  head->config = ntohl(head->config);
  head->page_size = ntohl(head->page_size);
  head->hash[0] = ntohl(head->hash[0]);
  head->hash[1] = ntohl(head->hash[1]);
  head->hash[2] = ntohl(head->hash[2]);

  /* Check hash first */
  if (bp__compute_hash(head->offset) != head->hash[0]) return 1;
  if (bp__compute_hash(head->config) != head->hash[1]) return 1;
  if (bp__compute_hash(head->page_size) != head->hash[2]) return 1;

  bp_tree_t* tree = (bp_tree_t*) w;

  if (tree->head_page == NULL) {
    bp__page_create(tree, 1, head->offset, head->config, &tree->head_page);
  }
  int ret = bp__page_load(tree, tree->head_page);

  return ret;
}


int bp__tree_write_head(bp__writer_t* w, void* data) {
  bp_tree_t* t = (bp_tree_t*) w;
  bp__tree_head_t* head = data;

  int ret;
  if (t->head_page == NULL) {
    /* Create empty leaf page */
    ret = bp__page_create(t, 1, 0, 0, &t->head_page);
    if (ret) return ret;

    /* Write it and store offset to head */
    ret = bp__page_save(t, t->head_page);
    if (ret) return ret;
  }

  /* Update head's position */
  head->offset = t->head_page->offset;
  head->config = t->head_page->config;

  /* TODO: page size should be configurable */
  head->page_size = 64;
  head->hash[0] = bp__compute_hash(head->offset);
  head->hash[1] = bp__compute_hash(head->config);
  head->hash[2] = bp__compute_hash(head->page_size);

  /* Create temporary head with fields in network byte order */
  bp__tree_head_t nhead;
  nhead.offset = htonl(head->offset);
  nhead.config = htonl(head->config);
  nhead.page_size = htonl(head->page_size);
  nhead.hash[0] = htonl(head->hash[0]);
  nhead.hash[1] = htonl(head->hash[1]);
  nhead.hash[2] = htonl(head->hash[2]);

  uint32_t offset;
  return bp__writer_write(w, sizeof(nhead), &nhead, &offset);
}
