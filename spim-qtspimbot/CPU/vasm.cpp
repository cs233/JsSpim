#include <cstdio>

#include "spim.h"
#include "string-stream.h"
#include "inst.h"
#include "reg.h"
#include "mem.h"
#include "data.h"
#include "vasm.h"

namespace
{
/**
 * @brief Forward iterator for encoded instructions
 */
class inst_iterator
{
public:
    inst_iterator()
    {
        // empty
    }

    inst_iterator(mem_addr addr) : addr(addr)
    {
        // empty
    }

    mem_word operator*() const
    {
        instruction * inst = read_mem_inst(0, addr);
        return inst_encode(inst, true);
    }

    bool operator==(const inst_iterator & other) const
    {
        return addr == other.addr;
    }

    bool operator!=(const inst_iterator & other) const
    {
        return !(*this == other);
    }

    inst_iterator & operator++()
    {
        addr += 4;
        return *this;
    }

    inst_iterator operator++(int)
    {
        inst_iterator temp(*this);
        ++*this;
        return temp;
    }

    mem_word word_addr()
    {
        return (addr - TEXT_BOT) >> 2;
    }

private:
    mem_addr addr;
};

/**
 * @brief Forward iterator for data words
 */
class data_iterator
{
public:
    data_iterator()
    {
        // empty
    }

    data_iterator(mem_addr addr) : addr(addr)
    {
        // empty
    }

    mem_word operator*() const
    {
        return read_mem_word(0, addr);
    }

    bool operator==(const data_iterator & other) const
    {
        return addr == other.addr;
    }

    bool operator!=(const data_iterator & other) const
    {
        return !(*this == other);
    }

    data_iterator & operator++()
    {
        addr += 4;
        return *this;
    }

    data_iterator operator++(int)
    {
        data_iterator temp(*this);
        ++*this;
        return temp;
    }

    mem_addr word_addr() const
    {
        return (addr - DATA_BOT) >> 2;
    }

private:
    mem_addr addr;
};

template <class T>
void vasm(const std::string & file_path, T begin, T end)
{
    FILE * output_file = fopen(file_path.c_str(), "w");
    if (output_file == NULL)
    {
        perror("Could not open file for writing");
        return;
    }

    if (begin == end)
    {
        // emulate spim.vasm behavior on empty segments
        fclose(output_file);
        return;
    }

    fprintf(output_file, "\n@%08x\n", begin.word_addr());
    for (T it = begin; it != end;)
    {
        for (int i = 0; i < 4 && it != end; ++i, ++it)
        {
            fprintf(output_file, " %08x", *it);
        }
        fputc('\n', output_file);
    }

    fclose(output_file);
}
}

void vasm(const std::string & files_prefix)
{
    std::string text_path = files_prefix + ".text.dat";
    inst_iterator text_begin(TEXT_BOT);
    inst_iterator text_end(current_text_pc());
    vasm(text_path, text_begin, text_end);

    std::string data_path = files_prefix + ".data.dat";
    data_iterator data_begin(DATA_BOT + 64*K);
    data_iterator data_end(current_data_pc());
    vasm(data_path, data_begin, data_end);
}
