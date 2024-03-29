/* SPIM S20 MIPS simulator.
   Data structures for symbolic addresses.

   Copyright (c) 1990-2010, James R. Larus.
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

#include "image.h"
#include "label.h"

#define SYMBOL_IS_DEFINED(SYM) ((SYM)->addr != 0)



/* Exported functions: */

mem_addr find_symbol_address (MIPSImage &img, char *symbol);
void flush_local_labels (MIPSImage &img, int issue_undef_warnings);
void initialize_symbol_table (MIPSImage &img);
label *label_is_defined (MIPSImage &img, char *name);
label *lookup_label (MIPSImage &img, char *name);
label *make_label_global (MIPSImage &img, char *name);
void print_symbols (MIPSImage &img);
void print_undefined_symbols (MIPSImage &img);
label *record_label (MIPSImage &img, char *name, mem_addr address, int resolve_uses);
void record_data_uses_symbol (MIPSImage &img, mem_addr location, label *sym);
void record_inst_uses_symbol (MIPSImage &img, instruction *inst, label *sym);
char *undefined_symbol_string (MIPSImage &img);
void resolve_a_label (MIPSImage &img, label *sym, instruction *inst);
void resolve_label_uses (MIPSImage &img, label *sym);
