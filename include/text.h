#ifndef TEXT_H
#define TEXT_H

#include <cstdio>

#define PRINT(fmt, ...) do { fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)

#endif
