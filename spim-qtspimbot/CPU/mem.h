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


/* Type of contents of a memory word. */

typedef int32 /*@alt unsigned int @*/ mem_word;

#define BYTE_TYPE signed char

typedef struct memimage {
	/* The text segment. */
	instruction **text_seg;
	unsigned *text_prof;
	int text_modified;		/* => text segment was written */
	mem_addr text_top;

	/* The data segment. */
	mem_word *data_seg;
	bool data_modified;		/* => a data segment was written */
	short *data_seg_h;		/* Points to same vector as DATA_SEG */
	BYTE_TYPE *data_seg_b;		/* Ditto */
	mem_addr data_top;
	mem_addr gp_midpoint;		/* Middle of $gp area */

	/* The stack segment. */
	mem_word *stack_seg;
	short *stack_seg_h;		/* Points to same vector as STACK_SEG */
	BYTE_TYPE *stack_seg_b;		/* Ditto */
	mem_addr stack_bot;

	/* Used for SPIMbot stuff. */
	mem_word *special_seg;
	short *special_seg_h;
	BYTE_TYPE *special_seg_b;

	/* The kernel text segment. */
	instruction **k_text_seg;
	unsigned *k_text_prof;
	mem_addr k_text_top;

	/* The kernel data segment. */
	mem_word *k_data_seg;
	short *k_data_seg_h;
	BYTE_TYPE *k_data_seg_b;
	mem_addr k_data_top;

} mem_image_t;

extern mem_image_t mem_images[2];

/* The text boundaries. */
#define TEXT_BOT ((mem_addr) 0x400000)
/* Amount to grow text segment when we run out of space for instructions. */
#define TEXT_CHUNK_SIZE	4096

/* The data boundaries. */
#define DATA_BOT ((mem_addr) 0x10000000)

/* The stack boundaries. */
/* Exclusive, but include 4K at top of stack. */
#define STACK_TOP ((mem_addr) 0x80000000)

/* The kernel text boundaries. */
#define K_TEXT_BOT ((mem_addr) 0x80000000)

/* The Kernel data boundaries. */
#define K_DATA_BOT ((mem_addr) 0x90000000)

/* Memory-mapped IO area: */
#define MM_IO_BOT		((mem_addr) 0xffff0000)
#define MM_IO_TOP		((mem_addr) 0xffffffff)

#define SPECIAL_BOT		((mem_addr) 0xfffe0000)
#define SPECIAL_TOP		((mem_addr) 0xffff0000)

/* Read from console: */
#define RECV_CTRL_ADDR		((mem_addr) 0xffff0000)
#define RECV_BUFFER_ADDR	((mem_addr) 0xffff0004)

#define RECV_READY		0x1
#define RECV_INT_ENABLE		0x2

#define RECV_INT_LEVEL		3 /* HW Interrupt 1 */


/* Write to console: */
#define TRANS_CTRL_ADDR		((mem_addr) 0xffff0008)
#define TRANS_BUFFER_ADDR	((mem_addr) 0xffff000c)

#define TRANS_READY		0x1
#define TRANS_INT_ENABLE	0x2

#define TRANS_INT_LEVEL		2 /* HW Interrupt 0 */




/* Exported functions: */

void check_memory_mapped_IO (size_t context);
void expand_data (size_t context, int addl_bytes);
void expand_k_data (size_t context, int addl_bytes);
void expand_stack (size_t context, int addl_bytes);
void make_memory (size_t context, int text_size, int data_size, int data_limit,
		  int stack_size, int stack_limit, int k_text_size,
		  int k_data_size, int k_data_limit);
void mem_dump_profile();
void* mem_reference(size_t context, mem_addr addr);
void print_mem (mem_addr addr);
instruction* read_mem_inst(size_t context, mem_addr addr);
reg_word read_mem_byte(size_t context, mem_addr addr);
reg_word read_mem_half(size_t context, mem_addr addr);
reg_word read_mem_word(size_t context, mem_addr addr);
void set_mem_inst(size_t context, mem_addr addr, instruction* inst);
void set_mem_byte(size_t context, mem_addr addr, reg_word value);
void set_mem_half(size_t context, mem_addr addr, reg_word value);
void set_mem_word(size_t context, mem_addr addr, reg_word value);

#endif
