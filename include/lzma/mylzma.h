#ifndef MYLZMA_H
#define MYLZMA_H
#include <string>

int LZMA_decode(const std::string &filename, char **out_buffer, size_t *decompressed_size);

#endif