#include "spim.h"
#include "spim-utils.h"
#include "image.h"

static size_t curr_ctx = 0;
static size_t num_ctx = NUM_CONTEXTS;

MIPSImage::MIPSImage(int ctx) : ctx(ctx) {}

int MIPSImage::get_ctx() const {
    return ctx;
}

mem_image_t &MIPSImage::mem_image() {
    return mem_img;
}

reg_image_t &MIPSImage::reg_image() {
    return reg_img;
}

label **MIPSImage::get_label_hash_table() {
    return label_hash_table;
}

label *MIPSImage::get_local_labels() {
    return local_labels;
}

void MIPSImage::set_local_labels(label *lbl) {
    local_labels = lbl;
}

const mem_image_t &MIPSImage::memview_image() const {
    return mem_img;
}

const reg_image_t &MIPSImage::regview_image() const {
    return reg_img;
}

std::unordered_map<mem_addr, breakpoint> &MIPSImage::breakpoints() {
    return bkpt_map;
}

