/* SPIM S20 MIPS simulator.
   Terminal interface for SPIM simulator.

   Copyright (c) 1990-2015, James R. Larus.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of the James R. Larus nor the names of its contributors may be
   used to endorse or promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "spim.h"

#ifdef WASM
#include "emscripten/bind.h"
#include "emscripten/val.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "CPU/spim.h"
#include "CPU/string-stream.h"
#include "CPU/spim-utils.h"
#include "CPU/inst.h"
#include "CPU/image.h"
#include "CPU/mem.h"
#include "CPU/parser.h"
#include "CPU/sym-tbl.h"
#include "CPU/scanner.h"
#include "CPU/data.h"
#include "CPU/version.h"

#ifdef WASM
using namespace emscripten;
#endif

bool bare_machine;        /* => simulate bare machine */
bool delayed_branches;        /* => simulate delayed branches */
bool delayed_loads;        /* => simulate delayed loads */
bool accept_pseudo_insts = true;    /* => parse pseudo instructions  */
bool quiet;            /* => no warning messages */
char *exception_file_name;
port message_out, console_out, console_in;
bool mapped_io;            /* => activate memory-mapped IO */
int spim_return_value;        /* Value returned when spim exits */


std::vector<MIPSImage> make_ctxs() {
  return std::vector<MIPSImage>();
}

static std::vector<MIPSImage> ctxs = make_ctxs();
static std::timed_mutex simulator_mtx;
static unsigned long intermidate_cycle_delay_usec = 0;

static str_stream ss;
void init() {
  // error("Based on <a href='http://spimsimulator.sourceforge.net/'>SPIM</a> %s "
  //       "by <a href='https://people.epfl.ch/james.larus'>James Larus</a>.\n",
  //       SPIM_VERSION);

  std::vector<MIPSImage> initial_ctxs;
        
  for (int i = 0; i < NUM_CONTEXTS; ++i) {
    MIPSImage new_image(i);
    initialize_world(new_image, DEFAULT_EXCEPTION_HANDLER, false);
    initialize_run_stack(new_image, 0, nullptr);
    char file_name[64];
    sprintf(file_name, "./input_%d.s", i);
    read_assembly_file(new_image, file_name); // check if the file
    new_image.reg_image().PC = starting_address(new_image);
    initial_ctxs.push_back(new_image);
  }

  ctxs = initial_ctxs;
}

int start_program() {
    step(0, false);
    return 0;
}

int run_entire_program() {
    std::thread t(start_program);
    t.detach();
    return 0;
}

int step(int step_size, bool cont_bkpt) {
  if (step_size == 0) step_size = DEFAULT_RUN_STEPS;

  bool continuable, bp_encountered;

  bp_encountered = run_spim_program(ctxs, step_size, false, cont_bkpt, &continuable, simulator_mtx, intermidate_cycle_delay_usec);

  if (!continuable) { // finished
    printf("\n"); // to flush output
    for (auto &img : ctxs) {
      error(img, "Execution finished\n");
    }
    return 0;
  }

  if (bp_encountered) {
    return -1;
  }

  return 1;
}

bool delete_ctx_breakpoint(mem_addr addr, int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return delete_breakpoint(img, addr);
}

bool add_ctx_breakpoint(mem_addr addr, int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return add_breakpoint(img, addr);
}

#ifdef WASM

EMSCRIPTEN_BINDINGS(run_entire_program) { function("run_entire_program", &run_entire_program); }

void unlockSimulator() {
    simulator_mtx.unlock();
}

bool lockSimulator(int timeout_usec) {
   return simulator_mtx.try_lock_for(std::chrono::microseconds(timeout_usec)); // to prevent main thread from holding and waiting
}

EMSCRIPTEN_BINDINGS(unlockSimulator) { function("unlockSimulator", &unlockSimulator); }
EMSCRIPTEN_BINDINGS(lockSimulator) { function("lockSimulator", &lockSimulator); }

void setDelay(unsigned long delay_usec) {
    intermidate_cycle_delay_usec = delay_usec;
}

EMSCRIPTEN_BINDINGS(setDelay) { function("setDelay", &setDelay); }

std::string getUserText(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  ss_clear(&ss);
  format_insts(img, &ss, TEXT_BOT, img.memview_image().text_top);
  return std::string(ss_to_string(img, &ss));
}

std::string getKernelText(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  ss_clear(&ss);
  format_insts(img, &ss, K_TEXT_BOT, img.memview_image().k_text_top);
  return std::string(ss_to_string(img, &ss));
}

val getStack(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(STACK_LIMIT / 16, (unsigned int *) img.memview_image().stack_seg));
}
val getUserData(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(img.memview_image().data_top - DATA_BOT, (unsigned int *) img.memview_image().data_seg));
}
val getKernelData(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(img.memview_image().k_data_top - K_DATA_BOT, (unsigned int *) img.memview_image().k_data_seg));
}
val getGeneralRegVals(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(32, (unsigned int *) img.regview_image().R));
}
val getFloatRegVals(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(32, (float *) img.regview_image().FPR));
  }
val getDoubleRegVals(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  return val(typed_memory_view(16, (double *) img.regview_image().FPR));
}

val getSpecialRegVals(int ctx) {
  MIPSImage &img = ctxs[ctx]; // will exception if ctx out of bounds
  static unsigned int specialRegs[9];
  specialRegs[0] = img.regview_image().PC;
  specialRegs[1] = img.regview_image().CP0_EPC;
  specialRegs[2] = img.regview_image().CP0_Cause;
  specialRegs[3] = img.regview_image().CP0_BadVAddr;
  specialRegs[4] = img.regview_image().CP0_Status;
  specialRegs[5] = img.regview_image().HI;
  specialRegs[6] = img.regview_image().LO;
  specialRegs[7] = img.regview_image().FIR;
  specialRegs[8] = img.regview_image().FCSR;

  return val(typed_memory_view(9, specialRegs));
}

EMSCRIPTEN_BINDINGS(init) { function("init", &init); }
EMSCRIPTEN_BINDINGS(step) { function("step", &step); }
EMSCRIPTEN_BINDINGS(getUserText) { function("getUserText", &getUserText); }
EMSCRIPTEN_BINDINGS(getKernelText) { function("getKernelText", &getKernelText); }
EMSCRIPTEN_BINDINGS(getStack) { function("getStack", &getStack); }
EMSCRIPTEN_BINDINGS(getUserData) { function("getUserData", &getUserData); }
EMSCRIPTEN_BINDINGS(getKernelData) { function("getKernelData", &getKernelData); }
EMSCRIPTEN_BINDINGS(getGeneralRegVals) { function("getGeneralRegVals", &getGeneralRegVals); }
EMSCRIPTEN_BINDINGS(getFloatRegVals) { function("getFloatRegVals", &getFloatRegVals); }
EMSCRIPTEN_BINDINGS(getDoubleRegVals) { function("getDoubleRegVals", &getDoubleRegVals); }
EMSCRIPTEN_BINDINGS(getSpecialRegVals) { function("getSpecialRegVals", &getSpecialRegVals); }
EMSCRIPTEN_BINDINGS(delete_breakpoint) { function("deleteBreakpoint", &delete_ctx_breakpoint); }
EMSCRIPTEN_BINDINGS(add_breakpoint) { function("addBreakpoint", &add_ctx_breakpoint); }
#endif

/* Print an error message. */

void error(MIPSImage const &img, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "(%u)\t", img.get_ctx());
  vfprintf(stderr, fmt, args);
  fflush(stderr);
  va_end(args);
}

/* Print the error message then exit. */

void fatal_error(MIPSImage const &img, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fmt = va_arg(args, char *);
  fprintf(stderr, "(%u)\t", img.get_ctx());
  vfprintf(stderr, fmt, args);
  fflush(stderr);
  exit(-1);
}

/* Print an error message and return to top level. */

void run_error(MIPSImage const &img, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "(%u)\t", img.get_ctx());
  vfprintf(stderr, fmt, args);
  fflush(stderr);
  va_end(args);
}

/* IO facilities: */

void write_output(MIPSImage const &img, port fp, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "(%u)\t", img.get_ctx());
  vfprintf(stdout, fmt, args);
  fflush(stdout);
  va_end(args);
}

/* Simulate the semantics of fgets (not gets) on Unix file. */

void read_input(char *str, int str_size) {
  char *ptr;
  ptr = str;
  while (1 < str_size) /* Reserve space for null */
  {
    char buf[1];
    if (read((int) console_in.i, buf, 1) <= 0) /* Not in raw mode! */
      break;

    *ptr++ = buf[0];
    str_size -= 1;

    if (buf[0] == '\n') break;
  }

  if (0 < str_size) *ptr = '\0'; /* Null terminate input */
}

int console_input_available() {
  return 0;
}

char get_console_char() {
  char buf;
  read(0, &buf, 1);
  return (buf);
}

void put_console_char(char c) {
  putc(c, stdout);
  fflush(stdout);
}

