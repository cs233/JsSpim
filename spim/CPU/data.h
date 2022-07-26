/* SPIM S20 MIPS simulator.
   Interface to code to manipulate data segment directives.

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

#ifndef DATA_H
#define DATA_H

/* Exported functions: */

void align_data (MIPSImage &img, int alignment); // internal
mem_addr current_data_pc (MIPSImage &img);
void data_begins_at_point (MIPSImage &img, mem_addr addr);
void enable_data_alignment (MIPSImage &img); // internal
void end_of_assembly_file (MIPSImage &img);
void extern_directive (MIPSImage &img, char *name, int size); // internal
void increment_data_pc (MIPSImage &img, int value); // internal
void k_data_begins_at_point (MIPSImage &img, mem_addr addr);
void lcomm_directive (MIPSImage &img, char *name, int size); // internal
void set_data_alignment (MIPSImage &img, int); // internal
void set_data_pc (MIPSImage &img, mem_addr addr); // internal
void set_text_pc (MIPSImage &img, mem_addr addr);
void store_byte (MIPSImage &img, int value); // internal
void store_double (MIPSImage &img, double *value); // internal
void store_float (MIPSImage &img, double *value); // internal
void store_half (MIPSImage &img, int value); // internal
void store_string (MIPSImage &img, char *string, int length, bool null_terminate); // internal
void store_word (MIPSImage &img, int value);
void user_kernel_data_segment (MIPSImage &img, bool to_kernel);

#endif
