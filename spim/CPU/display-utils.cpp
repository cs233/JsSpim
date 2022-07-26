/* SPIM S20 MIPS simulator.
   Utilities for displaying machine contents.

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
#include "image.h"
#include "reg.h"
#include "mem.h"
#include "run.h"
#include "sym-tbl.h"


char* int_reg_names[32] =
  {"r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
   "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
   "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
   "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"};


static mem_addr format_partial_line (MIPSImage &img, str_stream *ss, mem_addr addr);


/* Write to the stream the contents of the machine's registers, in a wide
   variety of formats. */

void
format_registers (MIPSImage &img, str_stream *ss, int print_gpr_hex, int print_fpr_hex)
{
  int i;
  char *grstr, *fpstr;
  char *grfill, *fpfill;
  const reg_image_t &reg = img.regview_image();

  ss_printf (img, ss, " PC      = %08x   ", reg.PC);
  ss_printf (img, ss, "EPC     = %08x  ", reg.CP0_EPC);
  ss_printf (img, ss, " Cause   = %08x  ", reg.CP0_Cause);
  ss_printf (img, ss, " BadVAddr= %08x\n", reg.CP0_BadVAddr);
  ss_printf (img, ss, " Status  = %08x   ", reg.CP0_Status);
  ss_printf (img, ss, "HI      = %08x  ", reg.HI);
  ss_printf (img, ss, " LO      = %08x\n", reg.LO);

  if (print_gpr_hex)
    grstr = "R%-2d (%2s) = %08x", grfill = "  ";
  else
    grstr = "R%-2d (%2s) = %-10d", grfill = " ";

  ss_printf (img, ss, "\t\t\t\t General Registers\n");
  for (i = 0; i < 8; i++)
    {
      ss_printf (img, ss, grstr, i, int_reg_names[i], reg.R[i]);
      ss_printf (img, ss, grfill);
      ss_printf (img, ss, grstr, i+8, int_reg_names[i+8], reg.R[i+8]);
      ss_printf (img, ss, grfill);
      ss_printf (img, ss, grstr, i+16, int_reg_names[i+16], reg.R[i+16]);
      ss_printf (img, ss, grfill);
      ss_printf (img, ss, grstr, i+24, int_reg_names[i+24], reg.R[i+24]);
      ss_printf (img, ss, "\n");
    }

  ss_printf (img, ss, "\n FIR    = %08x   ", reg.FIR);
  ss_printf (img, ss, " FCSR    = %08x   ", reg.FCSR);
  ss_printf (img, ss, "\t\t\t      Double Floating Point Registers\n");

  if (print_fpr_hex)
    fpstr = "FP%-2d=%08x,%08x", fpfill = " ";
  else
    fpstr = "FP%-2d = %#-13.6g", fpfill = " ";

  if (print_fpr_hex)
    for (i = 0; i < 4; i += 1)
      {
	int *r1, *r2;

	/* Use pointers to cast to ints without invoking float->int conversion
	   so we can just print the bits. */
	r1 = (int *)&reg.FPR[i]; r2 = r1 + 1;
	ss_printf (img, ss, fpstr, 2*i, *r1, *r2);
	ss_printf (img, ss, fpfill);

	r1 = (int *)&reg.FPR[i+4]; r2 = r1 + 1;
	ss_printf (img, ss, fpstr, 2*i+8, *r1, *r2);
	ss_printf (img, ss, fpfill);

	r1 = (int *)&reg.FPR[i+8]; r2 = r1 + 1;
	ss_printf (img, ss, fpstr, 2*i+16, *r1, *r2);
	ss_printf (img, ss, fpfill);

	r1 = (int *)&reg.FPR[i+12]; r2 = r1 + 1;
	ss_printf (img, ss, fpstr, 2*i+24, *r1, *r2);
	ss_printf (img, ss, "\n");
      }
  else for (i = 0; i < 4; i += 1)
    {
      ss_printf (img, ss, fpstr, 2*i, reg.FPR[i]);
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, 2*i+8, reg.FPR[i+4]);
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, 2*i+16, reg.FPR[i+8]);
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, 2*i+24, reg.FPR[i+12]);
      ss_printf (img, ss, "\n");
    }

  if (print_fpr_hex)
    fpstr = "FP%-2d=%08x", fpfill = " ";
  else
    fpstr = "FP%-2d = %#-13.6g", fpfill = " ";

  ss_printf (img, ss, "\t\t\t      Single Floating Point Registers\n");

  if (print_fpr_hex)
    for (i = 0; i < 8; i += 1)
      {
	/* Use pointers to cast to ints without invoking float->int conversion
	   so we can just print the bits. */
	ss_printf (img, ss, fpstr, i, *(int *)&FPR_S(reg, i));
	ss_printf (img, ss, fpfill);

	ss_printf (img, ss, fpstr, i+8, *(int *)&FPR_S(reg, i+8));
	ss_printf (img, ss, fpfill);

	ss_printf (img, ss, fpstr, i+16, *(int *)&FPR_S(reg, i+16));
	ss_printf (img, ss, fpfill);

	ss_printf (img, ss, fpstr, i+24, *(int *)&FPR_S(reg, i+24));
	ss_printf (img, ss, "\n");
      }
  else for (i = 0; i < 8; i += 1)
    {
      ss_printf (img, ss, fpstr, i, FPR_S(reg, i));
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, i+8, FPR_S(reg, i+8));
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, i+16, FPR_S(reg, i+16));
      ss_printf (img, ss, fpfill);
      ss_printf (img, ss, fpstr, i+24, FPR_S(reg, i+24));
      ss_printf (img, ss, "\n");
    }
}



/* Write to the stream a printable representation of the instructions in
   memory addresses: FROM...TO. */

void
format_insts (MIPSImage &img, str_stream *ss, mem_addr from, mem_addr to)
{
  instruction *inst;
  mem_addr i;

  for (i = from; i < to; i += 4)
    {
      inst = read_mem_inst (img, i);
      if (inst != NULL)
	{
	  format_an_inst (img, ss, inst, i);
	}
    }
}


/* Write to the stream a printable representation of the data and stack
   segments. */

void
format_data_segs (MIPSImage &img, str_stream *ss)
{ 
  ss_printf (img, ss, "\tDATA\n");
  format_mem (img, ss, DATA_BOT, img.mem_image().data_top);

  ss_printf (img, ss, "\n\tSTACK\n");
  format_mem (img, ss, ROUND_DOWN (img.reg_image().R[29], BYTES_PER_WORD), STACK_TOP);

  ss_printf (img, ss, "\n\tKERNEL DATA\n");
  format_mem (img, ss, K_DATA_BOT, img.mem_image().k_data_top);
}


#define BYTES_PER_LINE (4*BYTES_PER_WORD)


/* Write to the stream a printable representation of the data in memory
   address: FROM...TO. */

void
format_mem (MIPSImage &img, str_stream *ss, mem_addr from, mem_addr to)
{
  mem_word val;
  mem_addr i = ROUND_UP (from, BYTES_PER_WORD);
  int j;

  i = format_partial_line (img, ss, i);

  for ( ; i < to; )
    {
      /* Count consecutive zero words */
      for (j = 0; (i + (uint32) j * BYTES_PER_WORD) < to; j += 1)
	{
	  val = read_mem_word (img, i + (uint32) j * BYTES_PER_WORD);
	  if (val != 0)
	    {
	      break;
	    }
	}

      if (j >= 4)
	{
	  /* Block of 4 or more zero memory words: */
	  ss_printf (img, ss, "[0x%08x]...[0x%08x]	0x00000000\n",
		     i,
		     i + (uint32) j * BYTES_PER_WORD);

	  i = i + (uint32) j * BYTES_PER_WORD;
	  i = format_partial_line (img, ss, i);
	}
      else
	{
	  /* Fewer than 4 zero words, print them on a single line: */
	  ss_printf (img, ss, "[0x%08x]		      ", i);
	  do
	    {
	      val = read_mem_word (img, i);
	      ss_printf (img, ss, "  0x%08x", (unsigned int)val);
	      i += BYTES_PER_WORD;
	    }
	  while (i % BYTES_PER_LINE != 0);

	  ss_printf (img, ss, "\n");
	}
    }
}


/* Write to the stream a text line containing a fraction of a
   quadword. Return the address after the last one written.  */

static mem_addr
format_partial_line (MIPSImage &img, str_stream *ss, mem_addr addr)
{
  if ((addr % BYTES_PER_LINE) != 0)
    {
      ss_printf (img, ss, "[0x%08x]		      ", addr);

      for (; (addr % BYTES_PER_LINE) != 0; addr += BYTES_PER_WORD)
	{
	  mem_word val = read_mem_word (img, addr);
	  ss_printf (img, ss, "  0x%08x", (unsigned int)val);
	}

      ss_printf (img, ss, "\n");
    }

  return addr;
}
