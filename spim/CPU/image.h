#ifndef IMAGE_H
#define IMAGE_H

#include "mem.h"
#include "reg.h"

#define NUM_CONTEXTS 1

typedef struct mipsimage {
    int ctx;
    mem_image_t mem;
    reg_image_t reg;
} mips_image_t;

void ctx_switch(int ctx);
void ctx_init(int ctx);
void ctx_increment();
size_t ctx_current();
mem_image_t &mem();
reg_image_t &reg();
const mem_image_t &memview(int ctx);
const reg_image_t &regview(int ctx);

#define DATA_PC (reg().in_kernel ? reg().next_k_data_pc : reg().next_data_pc)
#define INST_PC (reg().in_kernel ? reg().next_k_text_pc : reg().next_text_pc)

#endif