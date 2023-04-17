#ifndef IMAGE_H
#define IMAGE_H

#include <unordered_map>
#include <optional>

#include "image_print_stream.h"
#include "mem_image.h"
#include "reg_image.h"
#include "label.h"

#define LABEL_HASH_TABLE_SIZE 8191

#define NUM_CONTEXTS 2

typedef struct breakpoint {
  mem_addr addr;
  instruction *inst;

  breakpoint(mem_addr addr) {
    this->addr = addr;
  }
} breakpoint;

class MIPSImage {
  private:
    int ctx;
    mem_image_t mem_img;
    reg_image_t reg_img;

    std::unordered_map<mem_addr, breakpoint> bkpt_map;
    // std::unordered_map<mem_addr, label> labels;
    label *local_labels = NULL;
    label *label_hash_table[LABEL_HASH_TABLE_SIZE] = {0};

    MIPSImagePrintStream std_out;
    MIPSImagePrintStream std_err;
  public:
    MIPSImage(int ctx);

    int get_ctx() const;

    mem_image_t &mem_image();
    reg_image_t &reg_image();
    label **get_label_hash_table();
    label *get_local_labels();
    void set_local_labels(label *);
    const mem_image_t &memview_image() const;
    const reg_image_t &regview_image() const;
    std::unordered_map<mem_addr, breakpoint> &breakpoints();

    /**
     * @brief Override this method to implement custom memory read behavior
     * @param addr The address to read from
     * @returns The value at the address, or std::nullopt if the read was unsuccessful
     */
    virtual std::optional<mem_word> custom_memory_read(mem_addr) { return std::nullopt; }

    /**
     * @brief Override this method to implement custom memory write behavior
     * @param addr The address to write to
     * @param value The value to write
     * @returns true if the write was successful, false otherwise
     */
    virtual bool custom_memory_write(mem_addr, mem_word) { return false; }

    std::streambuf *get_std_out_buf();
    std::streambuf *get_std_err_buf();
};

#define DATA_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_data_pc : img.reg_image().next_data_pc)
#define INST_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_text_pc : img.reg_image().next_text_pc)

#define INTERRUPTS_ON(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_IE)
#define IN_INTERRUPT_HANDLER(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_EXL)

#endif
