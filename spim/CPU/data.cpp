/* SPIM S20 MIPS simulator.
   Code to manipulate data segment directives.

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


#include "spim.h"
#include "string-stream.h"
#include "spim-utils.h"
#include "inst.h"
#include "image.h"
#include "mem.h"
#include "reg.h"
#include "sym-tbl.h"
#include "parser.h"
#include "run.h"
#include "data.h"

void
user_kernel_data_segment (MIPSImage &img, bool to_kernel)
{
    img.reg_image().in_kernel = to_kernel;
}


void
end_of_assembly_file (MIPSImage &img)
{
  img.reg_image().in_kernel = false;
  img.reg_image().auto_alignment = true;
}


/* Set the point at which the first datum is stored to be ADDRESS +
   64K.	 The 64K increment allocates an area pointed to by register
   $gp, which is initialized. */

void
data_begins_at_point (MIPSImage &img, mem_addr addr)
{
  if (bare_machine)
    img.reg_image().next_data_pc = addr;
  else
    {
      img.reg_image().next_gp_item_addr = addr;
      img.mem_image().gp_midpoint = addr + 32*K;
      img.reg_image().R[REG_GP] = img.mem_image().gp_midpoint;
      img.reg_image().next_data_pc = addr + 64 * K;
    }
}


/* Set the point at which the first datum is stored in the kernel's
   data segment. */

void
k_data_begins_at_point (MIPSImage &img, mem_addr addr)
{
    img.reg_image().next_k_data_pc = addr;
}


/* Arrange that the next datum is stored on a memory boundary with its
   low ALIGNMENT bits equal to 0.  If argument is 0, disable automatic
   alignment.*/

void
align_data (MIPSImage &img, int alignment)
{
  if (alignment == 0)
    img.reg_image().auto_alignment = false;
  else if (img.reg_image().in_kernel)
    {
      img.reg_image().next_k_data_pc =
	(img.reg_image().next_k_data_pc + (1 << alignment) - 1) & (0xffffffff << alignment);
      fix_current_label_address (img, img.reg_image().next_k_data_pc);
    }
  else
    {
      img.reg_image().next_data_pc = (img.reg_image().next_data_pc + (1 << alignment) - 1) & (0xffffffff << alignment);
      fix_current_label_address (img, img.reg_image().next_data_pc);
    }
}


void
set_data_alignment (MIPSImage &img, int alignment)
{
  if (img.reg_image().auto_alignment)
    align_data (img, alignment);
}


void
enable_data_alignment (MIPSImage &img)
{
  img.reg_image().auto_alignment = true;
}


/* Set the location (in user or kernel data space) for the next datum. */

void
set_data_pc (MIPSImage &img, mem_addr addr)
{
  if (img.reg_image().in_kernel)
    img.reg_image().next_k_data_pc = addr;
  else
    img.reg_image().next_data_pc = addr;
}


/* Return the address at which the next datum will be stored.  */

mem_addr
current_data_pc (MIPSImage &img)
{
  return (DATA_PC(img));
}


/* Bump the address at which the next data will be stored by DELTA
   bytes. */

void
increment_data_pc (MIPSImage &img, int delta)
{
  if (img.reg_image().in_kernel)
    {
      img.reg_image().next_k_data_pc += delta;
      if (img.mem_image().k_data_top <= img.reg_image().next_k_data_pc)
        expand_k_data(img, ROUND_UP(img.reg_image().next_k_data_pc - img.mem_image().k_data_top + 1, 64*K));
    }
  else
    {
      img.reg_image().next_data_pc += delta;
      if (img.mem_image().data_top <= img.reg_image().next_data_pc)
        expand_data(img, ROUND_UP(img.reg_image().next_data_pc - img.mem_image().data_top + 1, 64*K));
    }
}


/* Process a .extern NAME SIZE directive. */

void
extern_directive (MIPSImage &img, char *name, int size)
{
  label *sym = make_label_global (img, name);

  if (!bare_machine
      && !sym->gp_flag   // Not already a global symbol
      && size > 0 && size <= SMALL_DATA_SEG_MAX_SIZE
      && img.reg_image().next_gp_item_addr + size < img.mem_image().gp_midpoint + 32*K)
    {
      sym->gp_flag = 1;
      sym->addr = img.reg_image().next_gp_item_addr;
      img.reg_image().next_gp_item_addr += size;
    }
}


/* Process a .lcomm NAME SIZE directive. */

void
lcomm_directive (MIPSImage &img, char *name, int size)
{
  if (!bare_machine
      && size > 0 && size <= SMALL_DATA_SEG_MAX_SIZE
      && img.reg_image().next_gp_item_addr + size < img.mem_image().gp_midpoint + 32*K)
    {
      label *sym = record_label (img, name, img.reg_image().next_gp_item_addr, 1);
      sym->gp_flag = 1;

      img.reg_image().next_gp_item_addr += size;
      /* Don't need to initialize since memory starts with 0's */
    }
  else
    {
      (void)record_label (img, name, img.reg_image().next_data_pc, 1);

      for ( ; size > 0; size --)
	{
	  store_byte (img, 0);
	}
    }
}


/* Process a .ascii STRING or .asciiz STRING directive. */

void
store_string (MIPSImage &img, char *string, int length, bool null_terminate)
{
  for ( ; length > 0; string ++, length --) {
    store_byte (img, *string);
  }
  if (null_terminate)
    {
      store_byte (img, 0);
    }
}


/* Process a .byte EXPR directive. */

void
store_byte (MIPSImage &img, int value)
{
  set_mem_byte (img, DATA_PC(img), value);
  increment_data_pc (img, 1);
}


/* Process a .half EXPR directive. */

void
store_half (MIPSImage &img, int value)
{
  if ((DATA_PC(img) & 0x1) != 0)
    {
#ifdef SPIM_BIGENDIAN
      store_byte ((value >> 8) & 0xff);
      store_byte (value & 0xff);
#else
      store_byte (img, value & 0xff);
      store_byte (img,(value >> 8) & 0xff);
#endif
    }
  else
    {
      set_mem_half (img, DATA_PC(img), value);
      increment_data_pc (img, BYTES_PER_WORD / 2);
    }
}


/* Process a .word EXPR directive. */

void
store_word (MIPSImage &img, int value)
{
  if ((DATA_PC(img) & 0x3) != 0)
    {
#ifdef SPIM_BIGENDIAN
      store_half ((value >> 16) & 0xffff);
      store_half (value & 0xffff);
#else
      store_half (img, value & 0xffff);
      store_half (img, (value >> 16) & 0xffff);
#endif
    }
  else
    {
      set_mem_word (img, DATA_PC(img), value);
      increment_data_pc (img, BYTES_PER_WORD);
    }
}


/* Process a .double EXPR directive. */

void
store_double (MIPSImage &img, double *value)
{
  if ((DATA_PC(img) & 0x7) != 0)
    {
      store_word (img, * ((mem_word *) value));
      store_word (img, * (((mem_word *) value) + 1));
    }
  else
    {
      set_mem_word (img, DATA_PC(img), *((mem_word *) value));
      increment_data_pc (img, BYTES_PER_WORD);
      set_mem_word (img, DATA_PC(img), *(((mem_word *) value) + 1));
      increment_data_pc (img, BYTES_PER_WORD);
    }
}


/* Process a .float EXPR directive. */

void
store_float (MIPSImage &img, double *value)
{
  float val = (float)*value;
  float *vp = &val;

  if ((DATA_PC(img) & 0x3) != 0)
    {
      store_half (img, *(mem_word *) vp & 0xffff);
      store_half (img, (*(mem_word *) vp >> 16) & 0xffff);
    }
  else
    {
      set_mem_word (img, DATA_PC(img), *((mem_word *) vp));
      increment_data_pc (img, BYTES_PER_WORD);
    }
}
