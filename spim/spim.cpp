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

#include "emscripten/bind.h"
#include "emscripten/val.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include "CPU/spim.h"
#include "CPU/string-stream.h"
#include "CPU/spim-utils.h"
#include "CPU/inst.h"
#include "CPU/image.h"
#include "CPU/parser.h"
#include "CPU/sym-tbl.h"
#include "CPU/scanner.h"
#include "CPU/parser_yacc.h"
#include "CPU/data.h"
#include "CPU/version.h"

using namespace emscripten;

bool bare_machine;        /* => simulate bare machine */
bool delayed_branches;        /* => simulate delayed branches */
bool delayed_loads;        /* => simulate delayed loads */
bool accept_pseudo_insts = true;    /* => parse pseudo instructions  */
bool quiet;            /* => no warning messages */
char *exception_file_name;
port message_out, console_out, console_in;
bool mapped_io;            /* => activate memory-mapped IO */
int spim_return_value;        /* Value returned when spim exits */

static str_stream ss;
void init() {
  error("Based on <a href='http://spimsimulator.sourceforge.net/'>SPIM</a> %s "
        "by <a href='https://people.epfl.ch/james.larus'>James Larus</a>.\n",
        SPIM_VERSION);
        
  for (int i = 0; i < NUM_CONTEXTS; ++i) {
    initialize_world(i, DEFAULT_EXCEPTION_HANDLER, false);
    initialize_run_stack(0, nullptr);
    read_assembly_file("./input.s");
  }

  ctx_switch(0);
  reg().PC = starting_address();
}


/*int step(int step_size, bool cont_bkpt) {
  mem_addr addr = PC == 0 ? starting_address() : PC;
  if (step_size == 0) step_size = DEFAULT_RUN_STEPS;

  bool continuable, bp_encountered;
  bp_encountered = run_program(addr, step_size, false, cont_bkpt, &continuable);

  if (!continuable) { // finished
    printf("\n"); // to flush output
    error("Execution finished\n");
    return 0;
  }

  if (bp_encountered) {
    error("Breakpoint encountered at 0x%08x\n", PC);
    return -1;
  }

  return 1;
}*/


std::string getUserText(int ctx) {
  ss_clear(&ss);
  format_insts(&ss, TEXT_BOT, memview(ctx).text_top);
  return std::string(ss_to_string(&ss));
}

std::string getKernelText(int ctx) {
  ss_clear(&ss);
  format_insts(&ss, K_TEXT_BOT, memview(ctx).k_text_top);
  return std::string(ss_to_string(&ss));
}

val getStack(int ctx) { return val(typed_memory_view(STACK_LIMIT / 16, (unsigned int *) memview(ctx).stack_seg)); }
val getUserData(int ctx) { return val(typed_memory_view(memview(ctx).data_top - DATA_BOT, (unsigned int *) memview(ctx).data_seg)); }
val getKernelData(int ctx) { return val(typed_memory_view(memview(ctx).k_data_top - K_DATA_BOT, (unsigned int *) memview(ctx).k_data_seg)); }
val getGeneralRegVals(int ctx) { return val(typed_memory_view(32, (unsigned int *) regview(ctx).R)); }
val getFloatRegVals(int ctx) { return val(typed_memory_view(32, (float *) regview(ctx).FPR)); }
val getDoubleRegVals(int ctx) { return val(typed_memory_view(16, (double *) regview(ctx).FPR)); }

val getSpecialRegVals(int ctx) {
  static unsigned int specialRegs[9];
  specialRegs[0] = regview(ctx).PC;
  specialRegs[1] = regview(ctx).CP0_EPC;
  specialRegs[2] = regview(ctx).CP0_Cause;
  specialRegs[3] = regview(ctx).CP0_BadVAddr;
  specialRegs[4] = regview(ctx).CP0_Status;
  specialRegs[5] = regview(ctx).HI;
  specialRegs[6] = regview(ctx).LO;
  specialRegs[7] = regview(ctx).FIR;
  specialRegs[8] = regview(ctx).FCSR;

  return val(typed_memory_view(9, specialRegs));
}

EMSCRIPTEN_BINDINGS(init) { function("init", &init); }
//EMSCRIPTEN_BINDINGS(step) { function("step", &step); }
EMSCRIPTEN_BINDINGS(getUserText) { function("getUserText", &getUserText); }
EMSCRIPTEN_BINDINGS(getKernelText) { function("getKernelText", &getKernelText); }
EMSCRIPTEN_BINDINGS(getStack) { function("getStack", &getStack); }
EMSCRIPTEN_BINDINGS(getUserData) { function("getUserData", &getUserData); }
EMSCRIPTEN_BINDINGS(getKernelData) { function("getKernelData", &getKernelData); }
EMSCRIPTEN_BINDINGS(getGeneralRegVals) { function("getGeneralRegVals", &getGeneralRegVals); }
EMSCRIPTEN_BINDINGS(getFloatRegVals) { function("getFloatRegVals", &getFloatRegVals); }
EMSCRIPTEN_BINDINGS(getDoubleRegVals) { function("getDoubleRegVals", &getDoubleRegVals); }
EMSCRIPTEN_BINDINGS(getSpecialRegVals) { function("getSpecialRegVals", &getSpecialRegVals); }
//EMSCRIPTEN_BINDINGS(delete_breakpoint) { function("deleteBreakpoint", &delete_breakpoint); }
//EMSCRIPTEN_BINDINGS(add_breakpoint) { function("addBreakpoint", &add_breakpoint); }

/* Print an error message. */

void error(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

/* Print the error message then exit. */

void fatal_error(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fmt = va_arg(args, char *);
  vfprintf(stderr, fmt, args);
  exit(-1);
}

/* Print an error message and return to top level. */

void run_error(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

/* IO facilities: */

void write_output(port fp, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
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