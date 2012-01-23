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
    assert(bp_sets(&db, key, val) == BP_OK);
  }

  assert(bp_compact(&db) == BP_OK);

  for (i = 0; i < n; i++) {
    char* result = NULL;

    sprintf(key, "some key %d", i);
    sprintf(expected, "some long long long long long value %d", i);

    assert(bp_gets(&db, key, &result) == BP_OK);
    assert(strcmp(result, expected) == 0);

    free(result);
  }

  /* overwrite every key */
  for (i = 0; i < n; i++) {
    sprintf(key, "some key %d", i);
    sprintf(val, "some another value %d", i);
    assert(bp_sets(&db, key, val) == BP_OK);
  }

  for (i = 0; i < n; i++) {
    bp_key_t kkey;
    bp_value_t result;
    bp_value_t previous;

    sprintf(key, "some key %d", i);

    kkey.length = strlen(key) + 1;
    kkey.value = key;

    /* new values should be in place */
    sprintf(expected, "some another value %d", i);
    assert(bp_get(&db, &kkey, &result) == BP_OK);
    assert(strcmp(result.value, expected) == 0);

    /* previous values should be available before compaction */
    sprintf(expected, "some long long long long long value %d", i);
    assert(bp_get_previous(&db, &result, &previous) == BP_OK);
    assert(strcmp(previous.value, expected) == 0);

    free(result.value);

    /* previous of previous ain't exist */
    assert(bp_get_previous(&db, &previous, &result) == BP_ENOTFOUND);

    free(previous.value);
  }

  assert(bp_compact(&db) == BP_OK);

  for (i = 0; i < n; i++) {
    sprintf(key, "some key %d", i);
    assert(bp_removes(&db, key) == BP_OK);
  }

  assert(bp_compact(&db) == BP_OK);

TEST_END("API test", "api")
