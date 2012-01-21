#ifndef _PRIVATE_COMPRESSOR_H_
#define _PRIVATE_COMPRESSOR_H_

#include <unistd.h> /* size_t */

size_t bp__max_compressed_size(size_t size);
int bp__compress(const char* input,
                 size_t input_length,
                 char* compressed,
                 size_t* compressed_length);

int bp__uncompressed_length(const char* compressed,
                            size_t compressed_length,
                            size_t* result);
int bp__uncompress(const char* compressed,
                   size_t compressed_length,
                   char* uncompressed,
                   size_t* uncompressed_length);

#endif /* _PRIVATE_COMPRESSOR_H_ */
