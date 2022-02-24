#ifndef IMAGE_H
#define IMAGE_H

#include "mem.h"
#include "reg.h"

#define NUM_CONTEXTS 2

typedef struct mipsimage {
    int ctx;
    mem_image_t mem;
    reg_image_t reg;
} mips_image_t;

static mips_image_t images[NUM_CONTEXTS]; 
static size_t curr_ctx = 0;
static size_t num_ctx = NUM_CONTEXTS;

inline void ctx_switch(int ctx) { ctx %= NUM_CONTEXTS; curr_ctx = ctx; }
inline void ctx_init(int ctx) { images[ctx].ctx = ctx; ctx_switch(ctx); }
inline void ctx_increment() { ctx_switch(curr_ctx+1); }
inline size_t ctx_current() { return curr_ctx; }
inline mem_image_t &mem() { return images[curr_ctx].mem; }
inline reg_image_t &reg() { return images[curr_ctx].reg; }
inline const mem_image_t &memview(int ctx) { return images[ctx].mem; }
inline const reg_image_t &regview(int ctx) { return images[ctx].reg; }

#define DATA_PC (reg().in_kernel ? reg().next_k_data_pc : reg().next_data_pc)
#define INST_PC (reg().in_kernel ? reg().next_k_text_pc : reg().next_text_pc)

#endif