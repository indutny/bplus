#include <bplus.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int compare_cb(const bp_key_t* a, const bp_key_t* b) {
  uint32_t i, len = a->length < b->length ? a->length : b->length;

  for (i = 0; i < len; i++) {
    if (a->value[i] != b->value[i]) {
      return a->value[i] > b->value[i] > 1 ? 1 : -1;
    }
  }

  return a->length - b->length;
}


int main(void) {
  bp_tree_t tree;

  int r;
  r = bp_open(&tree, "/tmp/1.bp");
  assert(r == 0);

  bp_set_compare_cb(&tree, compare_cb);

  bp_sets(&tree, "some key", "some value");
  bp_sets(&tree, "some key2", "some value");

  r = bp_close(&tree);
  assert(r == 0);
}
