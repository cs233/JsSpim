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

void ctx_switch(int ctx) { ctx %= NUM_CONTEXTS; curr_ctx = ctx; }
void ctx_init(int ctx) { images[ctx].ctx = ctx; ctx_switch(ctx); }
void ctx_increment() { ctx_switch(curr_ctx+1); }
mem_image_t &mem() { return images[curr_ctx].mem; }
reg_image_t &reg() { return images[curr_ctx].reg; }
const mem_image_t &memview(int ctx) { return images[ctx].mem; }
const reg_image_t &regview(int ctx) { return images[ctx].reg; }

#endif