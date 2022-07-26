/* SPIM S20 MIPS simulator.
   Interface to misc. routines for SPIM.

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

#ifndef SPIM_UTILS_H
#define SPIM_UTILS_H

#include <vector>
#include "image.h"
#include "inst.h"


/* Triple containing a string and two integers.	 Used in tables
   mapping from a name to values. */

typedef struct
{
  char *name;
  int value1;
  int value2;
} name_val_val;

/* Record of where a breakpoint was placed and the instruction previously
   in memory. */

typedef struct bkptrec
{
  mem_addr addr;
  instruction *inst;
  struct bkptrec *next;
} bkpt;


extern bkpt *bkpts;



/* Exported functions: */

bool add_breakpoint (MIPSImage &img, mem_addr addr);
bool delete_breakpoint (MIPSImage &img, mem_addr addr);
void format_data_segs (MIPSImage &img, str_stream *ss);
void format_insts (MIPSImage &img, str_stream *ss, mem_addr from, mem_addr to);
void format_mem (MIPSImage &img, str_stream *ss, mem_addr from, mem_addr to);
void format_registers (MIPSImage &img, str_stream *ss, int print_gpr_hex, int print_fpr_hex);
void initialize_registers (MIPSImage &img);
void initialize_stack (MIPSImage &img, const char *command_line);
void initialize_run_stack (MIPSImage &img, int argc, char **argv);
void initialize_world (MIPSImage &img, char *exception_file_name, bool print_message);
void list_breakpoints (MIPSImage &img);
name_val_val *map_int_to_name_val_val (name_val_val tbl[], int tbl_len, int num);
name_val_val *map_string_to_name_val_val (name_val_val tbl[], int tbl_len, char *id);
bool read_assembly_file (MIPSImage &img, char *fpath);
bool run_spim_program(std::vector<MIPSImage> &ctxs, int steps, bool display, bool cont_bkpt, bool* continuable);
bool run_spimbot_program (int steps, bool display, bool cont_bkpt, bool* continuable);
mem_addr starting_address (MIPSImage &img);
char *str_copy (MIPSImage &img, char *str);
void write_startup_message (MIPSImage &img);
void *xmalloc (MIPSImage&, int);
void *zmalloc (MIPSImage&, int);

#endif
