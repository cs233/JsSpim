#include "spim.h"
#include "spim-utils.h"
#include "image.h"

static mips_image_t images[NUM_CONTEXTS]; 
static size_t curr_ctx = 0;
static size_t num_ctx = NUM_CONTEXTS;

void ctx_switch(int ctx) { 
    ctx %= NUM_CONTEXTS; 
    curr_ctx = ctx; 
}

void ctx_init(int ctx) { 
    images[ctx].ctx = ctx;
    ctx_switch(ctx);
}

void ctx_increment() { 
    ctx_switch(curr_ctx+1);
}

size_t ctx_current() { 
    return curr_ctx; 
}

mem_image_t &mem() { 
    return images[curr_ctx].mem;
}

reg_image_t &reg() { 
    return images[curr_ctx].reg;
}

const mem_image_t &memview(int ctx) { 
    return images[ctx].mem;
}

const reg_image_t &regview(int ctx) { 
    return images[ctx].reg;
}

std::unordered_map<mem_addr, breakpoint> &breakpoints() {
    return images[curr_ctx].breakpoints;
}