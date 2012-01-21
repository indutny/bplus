#include <bplus.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

int compare_cb(const bp_key_t* a, const bp_key_t* b) {
  uint32_t i, len = a->length < b->length ? a->length : b->length;

  for (i = 0; i < len; i++) {
    if (a->value[i] != b->value[i]) {
      return a->value[i] > b->value[i] ? 1 : -1;
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

  const int n = 10000;
  int i;

  for (i = 0; i < n; i++) {
    char key[100];
    char val[100];

    sprintf(key, "some key %d", i);
    sprintf(val, "some long long long long long value %d", i);
    assert(bp_sets(&tree, key, val) == 0);
  }

  assert(bp_compact(&tree) == 0);

  for (i = 0; i < n; i++) {
    char key[100];
    char* value = NULL;
    char expected[100];

    sprintf(key, "some key %d", i);
    sprintf(expected, "some long long long long long value %d", i);

    int ret = bp_gets(&tree, key, &value);
    assert(ret == 0);
    assert(strncmp(value, expected, strlen(expected)) == 0);

    free(value);
  }

  for (i = 0; i < n; i++) {
    char key[100];
    char* value;

    sprintf(key, "some key %d", i);
    assert(bp_removes(&tree, key) == 0);
  }

  assert(bp_compact(&tree) == 0);

  r = bp_close(&tree);
  assert(r == 0);

  return 0;
}
