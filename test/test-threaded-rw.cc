#include "test.h"

const int items = 1000;
const int times = 30;

void* test_reader(void* db_) {
  bp_tree_t* db = (bp_tree_t*) db_;

  char key[20];
  char* value;

  for (int j = 0; j < times; j++) {
    for (int i = 0; i < items; i++) {
      sprintf(key, "%d", i);
      if (bp_gets(db, key, &value) == BP_OK) {
        assert(strcmp(key, value) == 0);
        free(value);
      }
    }
  }

  return NULL;
}

void* test_writer(void* db_) {
  bp_tree_t* db = (bp_tree_t*) db_;

  char key[20];
  int ret;

  for (int i = 0; i < items; i++) {
    sprintf(key, "%d", i);
    ret = bp_sets(db, key, key);
    assert(ret == BP_OK);
  }

  return NULL;
}

TEST_START("threaded read/write test", "threaded-rw")

const int n = 4;
pthread_t readers[n];
pthread_t writers[n];

for (int i = 0; i < n; i++) {
  assert(pthread_create(&readers[i], NULL, test_reader, (void*) &db) == 0);
  assert(pthread_create(&writers[i], NULL, test_writer, (void*) &db) == 0);
}

for (int i = 0; i < n; i++) {
  assert(pthread_join(readers[i], NULL) == 0);
  assert(pthread_join(writers[i], NULL) == 0);
}

TEST_END("threaded read/write test", "threaded-rw")
