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
 *
 * It's unclear if we currently want to add std::ostream to the constructor, but that's an idea that
 * can be implemented if needed.
 */
class MIPSImagePrintStream : public std::streambuf {
    public:
        explicit MIPSImagePrintStream(unsigned int ctx, std::ostream &sink, std::size_t buffer_size = 256); 
        MIPSImagePrintStream(const MIPSImagePrintStream&) = delete;
        MIPSImagePrintStream(MIPSImagePrintStream&&);
        MIPSImagePrintStream& operator=(const MIPSImagePrintStream&) = delete;
        ~MIPSImagePrintStream();

    private:
        std::streamsize xsputn(const char *s, std::streamsize n);
        int_type overflow(int_type ch);
        int sync();

        unsigned int ctx;
        std::ostream &sink;
        std::vector<char> buf;
};

#endif
