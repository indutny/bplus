#ifndef _BPLUS_H_
#define _BPLUS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bp_tree_s bp_tree_t;

typedef struct bp_key_s bp_key_t;
typedef struct bp_key_s bp_value_t;

typedef int (*bp_compare_cb)(const bp_key_t* a, const bp_key_t* b);
typedef void (*bp_range_cb)(const bp_key_t* key, const bp_value_t* value);
typedef int (*bp_filter_cb)(const bp_key_t* key);

#define BP_PADDING 64

#define BP_KEY_FIELDS \
    uint64_t length;\
    char* value;

#include "private/tree.h"
#include "private/errors.h"
#include <assert.h>
#include <stdint.h> /* uintx_t */

/*
 * Open and close database
 */
int bp_open(bp_tree_t* tree, const char* filename);
int bp_close(bp_tree_t* tree);

/*
 * Get one value by key
 */
int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value);
int bp_gets(bp_tree_t* tree, const char* key, char** value);

/*
 * Get previous value (MVCC)
 */
int bp_get_previous(bp_tree_t* tree,
                    const bp_value_t* value,
                    bp_value_t* previous);

/*
 * Set one value by key
 */
int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value);
int bp_sets(bp_tree_t* tree, const char* key, const char* value);

/*
 * Set multiple values by key
 *
 */
int bp_bulk_set(bp_tree_t* tree,
                const uint64_t count,
                const bp_key_t** keys,
                const bp_value_t** values);
int bp_bulk_sets(bp_tree_t* tree,
                 const uint64_t count,
                 const char** keys,
                 const char** values);

/*
 * Remove one value by key
 */
int bp_remove(bp_tree_t* tree, const bp_key_t* key);
int bp_removes(bp_tree_t* tree, const char* key);

/*
 * Get all values in range
 * Note: value will be automatically freed after invokation of callback
 */
int bp_get_range(bp_tree_t* tree,
                 const bp_key_t* start,
                 const bp_key_t* end,
                 bp_range_cb cb);
int bp_get_ranges(bp_tree_t* tree,
                  const char* start,
                  const char* end,
                  bp_range_cb cb);

/*
 * Get values in range (with custom key-filter)
 * Note: value will be automatically freed after invokation of callback
 */
int bp_get_filtered_range(bp_tree_t* tree,
                          const bp_key_t* start,
                          const bp_key_t* end,
                          bp_filter_cb filter,
                          bp_range_cb cb);
int bp_get_filtered_ranges(bp_tree_t* tree,
                           const char* start,
                           const char* end,
                           bp_filter_cb filter,
                           bp_range_cb cb);

/*
 * Run compaction on database
 */
int bp_compact(bp_tree_t* tree);

/*
 * Set compare function to define order of keys in database
 */
void bp_set_compare_cb(bp_tree_t* tree, bp_compare_cb cb);

/*
 * Ensure that all data is written to disk
 */
int bp_fsync(bp_tree_t* tree);

struct bp_tree_s {
  BP_TREE_PRIVATE
};

struct bp_key_s {
  /*
   * uint64_t length;
   * char* value;
   */
  BP_KEY_FIELDS
  BP_KEY_PRIVATE
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _BPLUS_H_ */
