#include "bplus.h"
#include "private/writer.h"

#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>


int bp__writer_create(bp__writer_t* w, const char* filename) {
  w->fd = fopen(filename, "a+");
  if (w->fd == NULL) return BP_EFILE;

  /* Determine filesize */
  if (fseeko(w->fd, 0, SEEK_END)) return BP_EFILE;
  w->filesize = ftello(w->fd);
  w->flushed = 1;

  return 0;
}


int bp__writer_destroy(bp__writer_t* w) {
  if (fclose(w->fd)) return BP_EFILE;
}


int bp__writer_read(bp__writer_t* w,
                    const uint32_t offset,
                    const uint32_t size,
                    void* data) {
  if (w->filesize < offset + size) return BP_EFILEREAD_OOB;

  /* flush any pending data before reading */
  if (!w->flushed) {
    if (fflush(w->fd)) return BP_EFILEFLUSH;
    w->flushed = 1;
  }

  /* Ignore empty reads */
  if (size == 0) return 0;

  if (fseeko(w->fd, offset, SEEK_SET)) return BP_EFILE;

  uint32_t read;
  read = fread(data, 1, size, w->fd);
  if (read != size) return BP_EFILEREAD;

  return 0;
}


int bp__writer_write(bp__writer_t* w,
                     const uint32_t size,
                     const void* data,
                     uint32_t* offset) {
  uint32_t written;

  /* Write padding */
  bp__tree_head_t head;
  uint32_t padding = sizeof(head) - (w->filesize % sizeof(head));

  if (padding != sizeof(head)) {
    written = fwrite(&head, 1, padding, w->fd);
    if (written != padding) return BP_EFILEWRITE;
    w->filesize += padding;
    w->flushed = 0;
  }

  /* Ignore empty writes */
  if (size != 0) {
    /* Write data */
    written = fwrite(data, 1, size, w->fd);
    if (written != size) return BP_EFILEWRITE;

    /* change offset */
    *offset = w->filesize;
    w->filesize += written;
    w->flushed = 0;
  }

  return 0;
}


int bp__writer_find(bp__writer_t* w,
                    const uint32_t size,
                    void* data,
                    bp__writer_cb seek,
                    bp__writer_cb miss) {
  uint32_t offset = w->filesize;
  int ret = 0;

  /* Write padding first */
  ret = bp__writer_write(w, 0, NULL, NULL);
  if (ret) return ret;

  /* Start seeking from bottom of file */
  while (offset >= size) {
    ret = bp__writer_read(w, offset - size, size, data);
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
