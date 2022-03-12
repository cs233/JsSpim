#ifndef IMAGE_H
#define IMAGE_H

#include <unordered_map>

#include "mem_image.h"
#include "reg_image.h"

#define NUM_CONTEXTS 2

typedef struct {
  mem_addr addr;
  instruction *inst;
} breakpoint;

typedef struct mipsimage {
    int ctx;
    mem_image_t mem;
    reg_image_t reg;

    std::unordered_map<mem_addr, breakpoint> breakpoints;
} mips_image_t;

void ctx_switch(int ctx);
void ctx_init(int ctx);
void ctx_increment();
size_t ctx_current();
mem_image_t &mem();
reg_image_t &reg();
const mem_image_t &memview(int ctx);
const reg_image_t &regview(int ctx);
std::unordered_map<mem_addr, breakpoint> &breakpoints();

#define DATA_PC (reg().in_kernel ? reg().next_k_data_pc : reg().next_data_pc)
#define INST_PC (reg().in_kernel ? reg().next_k_text_pc : reg().next_text_pc)

#define INTERRUPTS_ON(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_IE)
#define IN_INTERRUPT_HANDLER(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_EXL)

#endif