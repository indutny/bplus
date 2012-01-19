#ifndef _BPLUS_H_
#define _BPLUS_H_

typedef struct bp_tree_s bp_tree_t;

#define BP_KEY_FIELDS \
    uint32_t length;\
    char* value;

#include "private/tree.h"
#include "private/errors.h"
#include <stdint.h> /* uintx_t */

typedef struct bp_key_s bp_key_t;
typedef struct bp_key_s bp_value_t;
typedef int (*bp_compare_cb)(const bp_key_t* a, const bp_key_t* b);

int bp_open(bp_tree_t* tree, const char* filename);
int bp_close(bp_tree_t* tree);

int bp_get(bp_tree_t* tree, const bp_key_t* key, bp_value_t* value);
int bp_gets(bp_tree_t* tree, const char* key, char** value);

int bp_set(bp_tree_t* tree, const bp_key_t* key, const bp_value_t* value);
int bp_sets(bp_tree_t* tree, const char* key, const char* value);

int bp_remove(bp_tree_t* tree, const bp_key_t* key);
int bp_removes(bp_tree_t* tree, const char* key);

void bp_set_compare_cb(bp_tree_t* tree, bp_compare_cb cb);

struct bp_tree_s {
  BP_TREE_PRIVATE
};

struct bp_key_s {
  BP_KEY_FIELDS
};

#endif /* _BPLUS_H_ */
