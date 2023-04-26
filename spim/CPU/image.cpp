#include "image.h"
#include "inst.h"
#include "spim.h"
#include "spim-utils.h"
#include "sym-tbl.h"

#include <iostream>

MIPSImage::MIPSImage(int ctx) :
    ctx(ctx),
    std_out(ctx, std::cout),
    std_err(ctx, std::cerr)
{
    label_hash_table = (label **) zmalloc(*this, LABEL_HASH_TABLE_SIZE * sizeof(label *));
}

MIPSImage::~MIPSImage() {
    free_internals();
}

MIPSImage::MIPSImage(MIPSImage &&other) :
    ctx(other.ctx),
    mem_img(std::move(other.mem_img)),
    reg_img(std::move(other.reg_img)),
    bkpt_map(std::move(other.bkpt_map)),
    local_labels(other.local_labels),
    label_hash_table(other.label_hash_table),
    labels_to_free(std::move(other.labels_to_free)),
    std_out(std::move(other.std_out)),
    std_err(std::move(other.std_err))
{
    other.mem_img = {};
    other.reg_img = {};
    other.bkpt_map.clear();
    other.local_labels = NULL;
    other.label_hash_table = NULL;
    other.labels_to_free.clear();
}

MIPSImage &MIPSImage::operator=(MIPSImage &&other) {
    free_internals();
    ctx = other.ctx;
    mem_img = std::move(other.mem_img);
    reg_img = std::move(other.reg_img);
    bkpt_map = std::move(other.bkpt_map);
    local_labels = other.local_labels;
    label_hash_table = other.label_hash_table;
    labels_to_free = std::move(other.labels_to_free);
    std_out = std::move(other.std_out);
    std_err = std::move(other.std_err);

    other.mem_img = {};
    other.reg_img = {};
    other.local_labels = NULL;
    other.label_hash_table = NULL;
    other.labels_to_free.clear();

    return *this;
}

void MIPSImage::free_internals() {
    initialize_symbol_table(*this);
    for (auto it = labels_to_free.begin(); it != labels_to_free.end(); it ++) {
        free((*it)->name);
        free(*it);
    }
    if (label_hash_table)
        free(label_hash_table);
}

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

void MIPSImage::push_label_to_free_vector(label *label) {
    labels_to_free.push_back(label);
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

std::streambuf *MIPSImage::get_std_out_buf() {
    return &std_out;
}

std::streambuf *MIPSImage::get_std_err_buf() {
    return &std_err;
}
