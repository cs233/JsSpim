/* SPIM S20 MIPS simulator.
   Macros for accessing memory.

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

#ifndef MEM_H
#define MEM_H

#include "reg.h"


/* A note on directions:  "Bottom" of memory is the direction of
   decreasing addresses.  "Top" is the direction of increasing addresses.*/

#include "image.h"




/* Exported functions: */

void check_memory_mapped_IO ();
void expand_data (MIPSImage &img, int addl_bytes);
void expand_k_data (MIPSImage &img, int addl_bytes);
void expand_stack (MIPSImage &img, int addl_bytes);
void make_memory (MIPSImage &img, int text_size, int data_size, int data_limit,
		  int stack_size, int stack_limit, int k_text_size,
		  int k_data_size, int k_data_limit);
void* mem_reference(MIPSImage &img, mem_addr addr); // TODO: Stopped here
void print_mem (MIPSImage &img, mem_addr addr);
instruction* read_mem_inst(MIPSImage &img, mem_addr addr);
reg_word read_mem_byte(MIPSImage &img, mem_addr addr);
reg_word read_mem_half(MIPSImage &img, mem_addr addr);
reg_word read_mem_word(MIPSImage &img, mem_addr addr);
void set_mem_inst(MIPSImage &img, mem_addr addr, instruction* inst);
void set_mem_byte(MIPSImage &img, mem_addr addr, reg_word value);
void set_mem_half(MIPSImage &img, mem_addr addr, reg_word value);
void set_mem_word(MIPSImage &img, mem_addr addr, reg_word value);

#endif
