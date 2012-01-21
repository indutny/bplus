#include "test.h"

TEST_START("API test", "api")

  const int n = 1000;
  char key[100];
  char val[100];
  char expected[100];
  int i;

  for (i = 0; i < n; i++) {
    sprintf(key, "some key %d", i);
    sprintf(val, "some long long long long long value %d", i);
    assert(bp_sets(&db, key, val) == 0);
  }

  assert(bp_compact(&db) == 0);

  for (i = 0; i < n; i++) {
    char* result = NULL;

    sprintf(key, "some key %d", i);
    sprintf(expected, "some long long long long long value %d", i);

    int ret = bp_gets(&db, key, &result);
    assert(ret == 0);
    assert(strncmp(result, expected, strlen(expected)) == 0);

    free(result);
  }

  for (i = 0; i < n; i++) {
    sprintf(key, "some key %d", i);
    assert(bp_removes(&db, key) == 0);
  }

  assert(bp_compact(&db) == 0);

TEST_END("API test")
