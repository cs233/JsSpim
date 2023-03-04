#pragma once
/* Type declarations for portability.  They work for DEC's Alpha (64 bits)
   and 32 bit machines */

typedef int int32;
typedef unsigned int  uint32;
typedef union {int i; void* p;} intptr_union;
