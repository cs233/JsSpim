#pragma once

#include <memory>

/* Type declarations for portability.  They work for DEC's Alpha (64 bits)
   and 32 bit machines */

typedef int int32;
typedef unsigned int  uint32;
typedef struct {
    int i;
    void *p;
    std::shared_ptr<char> s;
} intptr_union;
