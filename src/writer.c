#include "bplus.h"
#include "private/writer.h"

#include <stdlib.h> /* malloc, free */

#include <snappy-c.h>

int bp__writer_create(bp__writer_t* w, const char* filename) {
  w->fd = fopen(filename, "a+");
  if (w->fd == NULL) return BP_EFILE;

  /* Determine filesize */
  if (fseeko(w->fd, 0, SEEK_END)) return BP_EFILE;
  w->filesize = ftello(w->fd);
  w->flushed = w->filesize;

  return BP_OK;
}


int bp__writer_destroy(bp__writer_t* w) {
  if (fclose(w->fd)) return BP_EFILE;
}


int bp__writer_read(bp__writer_t* w,
                    const uint32_t offset,
                    uint32_t* size,
                    void** data) {
  if (w->filesize < offset + *size) return BP_EFILEREAD_OOB;

  /* flush any pending data before reading */
  if (offset + *size > w->flushed) {
    if (fflush(w->fd)) return BP_EFILEFLUSH;
    w->flushed = w->filesize;
  }

  /* Ignore empty reads */
  if (*size == 0) return BP_OK;

  if (fseeko(w->fd, offset, SEEK_SET)) return BP_EFILE;

  size_t read;

  char* cdata = malloc(*size);
  if (cdata == NULL) return BP_EALLOC;

  read = fread(cdata, 1, *size, w->fd);
  if (read != *size) {
    free(cdata);
    return BP_EFILEREAD;
  }

  /* no compression for small chunks */
  if (*size <= sizeof(bp__tree_head_t)) {
    *data = cdata;
  } else {
    int ret = 0;

    char* uncompressed = NULL;
    size_t usize;

    if (snappy_uncompressed_length(cdata, *size, &usize) != SNAPPY_OK) {
      ret = BP_ESNAPPYD;
    } else {
      uncompressed = malloc(usize);
      if (uncompressed == NULL) {
        ret = BP_EALLOC;
      } else if (snappy_uncompress(cdata, *size, uncompressed, &usize) !=
                 SNAPPY_OK) {
        ret = BP_ESNAPPYD;
      } else {
        free(cdata);
        *data = uncompressed;
        *size = usize;
      }
    }

    if (ret) {
      free(cdata);
      free(uncompressed);
      return ret;
    }
  }

  return BP_OK;
}


int bp__writer_write(bp__writer_t* w,
                     const uint32_t size,
                     const void* data,
                     uint32_t* offset,
                     uint32_t* csize) {
  size_t written;

  /* Write padding */
  bp__tree_head_t head;
  uint32_t padding = sizeof(head) - (w->filesize % sizeof(head));

  if (padding != sizeof(head)) {
    written = fwrite(&head, 1, padding, w->fd);
    if (written != padding) return BP_EFILEWRITE;
    w->filesize += padding;
  }

  /* Ignore empty writes */
  if (size == 0) return BP_OK;

  /* head and smaller chunks shouldn't be compressed */
  if (size < sizeof(head)) {
    written = fwrite(data, 1, size, w->fd);
    *csize = size;
  } else {
    size_t max_csize = snappy_max_compressed_length(size);
    char* compressed = malloc(max_csize);
    if (compressed == NULL) return BP_EALLOC;

    size_t result_size = max_csize;
    int ret = snappy_compress(data, size, compressed, &result_size);
    if (ret != SNAPPY_OK) {
      free(compressed);
      return BP_ESNAPPYC;
    }

    *csize = (uint32_t) result_size;
    written = fwrite(compressed, 1, result_size, w->fd);
    free(compressed);
  }

  if (written != *csize) return BP_EFILEWRITE;

  /* change offset */
  *offset = w->filesize;
  w->filesize += written;

  return BP_OK;
}


int bp__writer_find(bp__writer_t* w,
                    const uint32_t size,
                    void* data,
                    bp__writer_cb seek,
                    bp__writer_cb miss) {
  uint32_t offset = w->filesize;
  int ret = 0;

  /* Write padding first */
  ret = bp__writer_write(w, 0, NULL, NULL, NULL);
  if (ret) return ret;

  uint32_t size_tmp = size;

  /* Start seeking from bottom of file */
  while (offset >= size) {
    ret = bp__writer_read(w, offset - size, &size_tmp, &data);
    if (ret) break;

    /* Break if matched */
    if (seek(w, data) == 0) break;

    offset -= size;
  }

  /* Not found - invoke miss */
  if (offset < size) {
    ret = miss(w, data);
  }

  return ret;
}
