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

class MIPSImage {
  private:
    int ctx;
    mem_image_t mem_img;
    reg_image_t reg_img;

    std::unordered_map<mem_addr, breakpoint> bkpt_map;

  public:
    MIPSImage(int ctx);

    int get_ctx() const;

    mem_image_t &mem_image();
    reg_image_t &reg_image();
    const mem_image_t &memview_image() const;
    const reg_image_t &regview_image() const;
    std::unordered_map<mem_addr, breakpoint> &breakpoints();

};

#define DATA_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_data_pc : img.reg_image().next_data_pc)
#define INST_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_text_pc : img.reg_image().next_text_pc)

#define INTERRUPTS_ON(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_IE)
#define IN_INTERRUPT_HANDLER(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_EXL)

#endif