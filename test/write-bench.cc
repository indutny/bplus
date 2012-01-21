#include "test.h"

TEST_START("write benchmark", "write-bench")

  const int num = 100000;
  int i;

  char keys[num][20];

  /* init keys */
  for (int i = 0; i < num; i++) {
    sprintf(keys[i], "string: %d", i);
  }

  BENCH_START(write)
  for (int i = 0; i < num; i++) {
    bp_sets(&db, keys[i], keys[i]);
  }
  BENCH_END(write)

TEST_END("write benchmark", "write-bench")
