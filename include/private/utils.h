#ifndef _PRIVATE_UTILS_H_
#define _PRIVATE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Thomas Wang, Integer Hash Functions. */
/* http://www.concentric.net/~Ttwang/tech/inthash.htm */
uint32_t bp__compute_hash(uint32_t key) {
  uint32_t hash = key;
  hash = ~hash + (hash << 15);  /* hash = (hash << 15) - hash - 1; */
  hash = hash ^ (hash >> 12);
  hash = hash + (hash << 2);
  hash = hash ^ (hash >> 4);
  hash = hash * 2057;  /* hash = (hash + (hash << 3)) + (hash << 11); */
  hash = hash ^ (hash >> 16);
  return hash;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _PRIVATE_UTILS_H_ */
