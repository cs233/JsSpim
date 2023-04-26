#pragma once
#ifndef IMAGE_PRINT_STREAM_H
#define IMAGE_PRINT_STREAM_H

#include <streambuf>
#include <cstdlib>
#include <iostream>
#include <vector>

/**
 * This is a custom stream to allow each context to have their own stdout and stderr.
 *
 * If compiled for WASM, the stream will flush to an async function where then the main UI thread
 * will append it to the frontend div and append it to an array for the ctx's stdout or stderr.
 *
 * If compiled for unix, it will simply forward it to std::cout/std::cerr.
 */
class MIPSImagePrintStream : public std::streambuf {
    public:
        explicit MIPSImagePrintStream(unsigned int ctx, std::ostream &sink, std::size_t buffer_size = 256); 
        MIPSImagePrintStream(const MIPSImagePrintStream&) = delete;
        MIPSImagePrintStream(MIPSImagePrintStream&&);
        MIPSImagePrintStream& operator=(const MIPSImagePrintStream&) = delete;
        MIPSImagePrintStream& operator=(MIPSImagePrintStream&&);
        ~MIPSImagePrintStream();

    private:
        inline void move_buffer_pointers(MIPSImagePrintStream &&other);
        std::streamsize xsputn(const char *s, std::streamsize n);
        int_type overflow(int_type ch);
        int sync();

        unsigned int ctx;
        // Note that this cannot be a reference type as std::ostream doesn't implement a public
        // copy/move constructor. As such, when it comes to the assignment overloads, if sink was a
        // `std::ostream &`, we cannot simply assign `this->sink = other.sink`. A pointer achieves
        // the desired result without having using ostream's protected move assignment.
        // Note: The use of a pointer rather than a reference makes no difference if the original
        // ostream goes out of scope and gets destroyed.
        std::ostream *sink;
        std::vector<char> buf;
};

#endif
