#include "image_print_stream.h"

#include <string.h>
#include <cassert>
#include <functional>

#ifdef WASM
#include "emscripten.h"
#endif

MIPSImagePrintStream::MIPSImagePrintStream(unsigned int ctx, std::ostream &sink, std::size_t buffer_size) :
    ctx(ctx),
    sink(sink),
    buf(buffer_size + 1)
{
    char *base = &buf.front();
    setp(base, base + buf.size() - 1);
}

MIPSImagePrintStream::~MIPSImagePrintStream() {
    sync(); // Flush the buffer on exit
}

std::streamsize MIPSImagePrintStream::xsputn(const char *s, std::streamsize n) {
    std::streamsize char_pos = 0;
    while (char_pos < n) {
        if (overflow(MIPSImagePrintStream::traits_type::to_int_type(*s)) == MIPSImagePrintStream::traits_type::eof()) {
            break;
        }
        char_pos++;
        s++;
    }
    return char_pos;
}

MIPSImagePrintStream::int_type MIPSImagePrintStream::overflow(MIPSImagePrintStream::int_type ch) {
    if (ch != traits_type::eof()) {
        assert(std::less_equal<char *>()(pptr(), epptr()));
        *pptr() = ch;
        pbump(1);

        // If ch is newline ('\n') or buffer is now full, flush
        if ((ch != '\n' && pptr() < epptr()) || !sync()) {
            return ch;
        }
    }

    return traits_type::eof();
}

int MIPSImagePrintStream::sync() {
    std::ptrdiff_t n = pptr() - pbase();
    pbump(-n);
#ifdef WASM
    char *s = new char[n + 1];
    strncpy(s, pbase(), n);
    s[n] = 0;
    if (&sink == &std::cout) {
        MAIN_THREAD_ASYNC_EM_ASM({
            writeStdOut($0, UTF8ToString($1));
        }, ctx, s);
        return 0;
    }
    if (&sink == &std::cerr) {
        MAIN_THREAD_ASYNC_EM_ASM({
            writeStdErr($0, UTF8ToString($1));
        }, ctx, s);
        return 0;
    }
    delete[] s;
    return -1;
#else
    return !((bool) sink.write(pbase(), n));
#endif
}
