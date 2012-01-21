#include "test.h"

TEST_START("API test", "api")

  const int n = 10000;
  int i;

  for (i = 0; i < n; i++) {
    char key[100];
    char val[100];

    sprintf(key, "some key %d", i);
    sprintf(val, "some long long long long long value %d", i);
    assert(bp_sets(&db, key, val) == 0);
  }

  assert(bp_compact(&db) == 0);

  for (i = 0; i < n; i++) {
    char key[100];
    char* value = NULL;
    char expected[100];

    sprintf(key, "some key %d", i);
    sprintf(expected, "some long long long long long value %d", i);

    int ret = bp_gets(&db, key, &value);
    assert(ret == 0);
    assert(strncmp(value, expected, strlen(expected)) == 0);

    free(value);
  }

  for (i = 0; i < n; i++) {
    char key[100];
    char* value;

    sprintf(key, "some key %d", i);
    assert(bp_removes(&db, key) == 0);
  }

  assert(bp_compact(&db) == 0);

TEST_END("API test")
