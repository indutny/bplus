#include "test.h"

TEST_START("one thread benchmark", "one-thread-bench")

  const int num = 100000;
  int i;

  char keys[num][20];

  /* init keys */
  for (int i = 0; i < num; i++) {
    sprintf(keys[i], "string: %d", i);
  }

  BENCH_START(write, num)
  for (int i = 0; i < num; i++) {
    bp_sets(&db, keys[i], keys[i]);
  }
  BENCH_END(write, num)

  BENCH_START(read, num)
  for (int i = 0; i < num; i++) {
    char* value;
    bp_gets(&db, keys[i], &value);
    free(value);
  }
  BENCH_END(read, num)

  BENCH_START(compact, 0)
  bp_compact(&db);
  BENCH_END(compact, 0)

  BENCH_START(read_after_compact, num)
  for (int i = 0; i < num; i++) {
    char* value;
    bp_gets(&db, keys[i], &value);
    free(value);
  }
  BENCH_END(read_after_compact, num)

  BENCH_START(remove, num)
  for (int i = 0; i < num; i++) {
    bp_removes(&db, keys[i]);
  }
  BENCH_END(remove, num)

TEST_END("one thread benchmark", "one-thread-bench")
