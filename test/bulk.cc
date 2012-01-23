#include "test.h"

TEST_START("bulk set test", "bulk-set")
  /* write some stuff */
  const int n = 127;
  int i = 0;
  char key[100];
  char* keys[n];

  sprintf(key, "key: x");
  for (i = 0; i < n; i++) {
    key[5] = i << 1;
    assert(bp_sets(&db, key, key) == BP_OK);
  }

  for (i = 0; i < n; i++) {
    keys[i] = (char*) malloc(100);
    assert(keys[i] != NULL);

    sprintf(keys[i], "key: x");
    keys[i][5] = (i << 1) + 1;
  }

  assert(bp_bulk_sets(&db, n, (const char**) keys, (const char**) keys) ==
         BP_OK);

  for (i = 1; i < n; i++) {
    free(keys[i]);
  }

  for (i = 0; i < (n << 1) + 1; i++) {
    key[5] = i;
    char* value;
    assert(bp_gets(&db, key, &value) == BP_OK);
    assert(strcmp(key, value) == 0);
    free(value);
  }

TEST_END("range get test", "range")
