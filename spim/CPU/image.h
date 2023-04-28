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
    label *local_labels = NULL; // No allocs occur here
    label **label_hash_table = NULL; // Points to an array of size LABEL_HASH_TABLE_SIZE
    std::vector<label *> labels_to_free;

    MIPSImagePrintStream std_out;
    MIPSImagePrintStream std_err;

    void free_internals();
  public:
    MIPSImage(int ctx);
    ~MIPSImage();
    MIPSImage(const MIPSImage&) = delete;
    MIPSImage(MIPSImage&&);

    MIPSImage &operator=(MIPSImage &) = delete;
    MIPSImage &operator=(MIPSImage &&);

    int get_ctx() const;

    mem_image_t &mem_image();
    reg_image_t &reg_image();
    label **get_label_hash_table();
    label *get_local_labels();
    void push_label_to_free_vector(label *);
    void set_local_labels(label *);
    const mem_image_t &memview_image() const;
    const reg_image_t &regview_image() const;
    std::unordered_map<mem_addr, breakpoint> &breakpoints();

    /**
     * @brief Override this method to implement custom memory read word behavior
     * @param addr The address to read from
     * @returns The value at the address, or std::nullopt if the read was unsuccessful
     */
    virtual std::optional<mem_word> custom_memory_read_word(mem_addr) { return std::nullopt; }

    /**
     * @brief Override this method to implement custom memory write word behavior
     * @param addr The address to write to
     * @param value The value to write
     * @returns true if the write was successful, false otherwise
     */
    virtual bool custom_memory_write_word(mem_addr, mem_word) { return false; }

    /**
     * @brief Override this method to implement custom memory read half behavior
     * @param addr The address to read from
     * @returns The value at the address, or std::nullopt if the read was unsuccessful
     */
    virtual std::optional<mem_word> custom_memory_read_half(mem_addr) { return std::nullopt; }

    /**
     * @brief Override this method to implement custom memory write half behavior
     * @param addr The address to write to
     * @param value The value to write
     * @returns true if the write was successful, false otherwise
     */
    virtual bool custom_memory_write_half(mem_addr, mem_word) { return false; }

    /**
     * @brief Override this method to implement custom memory read byte behavior
     * @param addr The address to read from
     * @returns The value at the address, or std::nullopt if the read was unsuccessful
     */
    virtual std::optional<mem_word> custom_memory_read_byte(mem_addr) { return std::nullopt; }

    /**
     * @brief Override this method to implement custom memory write byte behavior
     * @param addr The address to write to
     * @param value The value to write
     * @returns true if the write was successful, false otherwise
     */
    virtual bool custom_memory_write_byte(mem_addr, mem_word) { return false; }


    std::streambuf *get_std_out_buf();
    std::streambuf *get_std_err_buf();
};

#define DATA_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_data_pc : img.reg_image().next_data_pc)
#define INST_PC(img) (img.reg_image().in_kernel ? img.reg_image().next_k_text_pc : img.reg_image().next_text_pc)

#define INTERRUPTS_ON(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_IE)
#define IN_INTERRUPT_HANDLER(REGIMAGE) (CP0_Status(REGIMAGE) & CP0_Status_EXL)

#endif
