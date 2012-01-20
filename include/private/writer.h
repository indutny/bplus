#ifndef _PRIVATE_WRITER_H_
#define _PRIVATE_WRITER_H_

#include <stdint.h>
#include <snappy-c.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BP_WRITER_PRIVATE \
    int fd;\
    uint32_t filesize;\
    char padding[BP_PADDING];

typedef struct bp__writer_s bp__writer_t;
typedef int (*bp__writer_cb)(bp__writer_t* w, void* data);

enum comp_type {
  kNotCompressed = 0,
  kCompressed = 1
};

int bp__writer_create(bp__writer_t* w, const char* filename);
int bp__writer_destroy(bp__writer_t* w);

int bp__writer_read(bp__writer_t* w,
                    const uint32_t offset,
                    const enum comp_type comp,
                    uint32_t* size,
                    void** data);
int bp__writer_write(bp__writer_t* w,
                     const uint32_t size,
                     const enum comp_type comp,
                     const void* data,
                     uint32_t* offset,
                     uint32_t* csize);

int bp__writer_find(bp__writer_t* w,
                    const uint32_t size,
                    const enum comp_type comp,
                    void* data,
                    bp__writer_cb seek,
                    bp__writer_cb miss);

struct bp__writer_s {
  BP_WRITER_PRIVATE
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _PRIVATE_WRITER_H_ */
