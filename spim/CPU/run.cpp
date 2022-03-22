/* SPIM S20 MIPS simulator.
   Execute SPIM instructions.

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


#ifdef mips
#define _IEEE 1
#include <nan.h>
#else
#define NaN(X) ((X) != (X))
#endif

#include <math.h>
#include <stdio.h>

#ifdef _WIN32
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#endif

#include "spim.h"
#include "string-stream.h"
#include "spim-utils.h"
#include "inst.h"
#include "image.h"
#include "sym-tbl.h"
#include "parser_yacc.h"
#include "syscall.h"
#include "run.h"

bool force_break = false;	/* For the execution env. to force an execution break */

#ifdef _MSC_BUILD
/* Disable MS VS warning about constant predicate in conditional. */
#pragma warning(disable: 4127)
#endif


/* Local functions: */

static void set_fpu_cc (int cond, int cc, int less, int equal, int unordered);
static void signed_multiply (reg_word v1, reg_word v2);
static void unsigned_multiply (reg_word v1, reg_word v2);


#define SIGN_BIT(X) ((X) & 0x80000000)

#define ARITH_OVFL(RESULT, OP1, OP2) (SIGN_BIT (OP1) == SIGN_BIT (OP2) \
				      && SIGN_BIT (OP1) != SIGN_BIT (RESULT))



/* True when delayed_branches is true and instruction is executing in delay
slot of another instruction. */
static int running_in_delay_slot = 0;


/* Executed delayed branch and jump instructions by running the
   instruction from the delay slot before transfering control.  Note,
   in branches that don't jump, the instruction in the delay slot is
   executed by falling through normally.

   We take advantage of the MIPS architecture, which leaves undefined
   the result of executing a delayed instruction in a delay slot.  Here
   we execute the second branch. */

#define BRANCH_INST(TEST, TARGET, NULLIFY)			\
		{						\
		  if (TEST)					\
		    {						\
		      mem_addr target = (TARGET);		\
		      if (delayed_branches)			\
			{					\
			  /* +4 since jump in delay slot */	\
			  target += BYTES_PER_WORD;		\
			}					\
		      JUMP_INST(target)				\
		     }						\
		  else if (NULLIFY)				\
		    {						\
		      /* If test fails and nullify bit set, skip\
			 instruction in delay slot. */		\
		      reg().PC += BYTES_PER_WORD;			\
		    }						\
		 }


#define JUMP_INST(TARGET)					\
		{						\
		  if (delayed_branches)				\
		    {						\
		      running_in_delay_slot = 1;		\
		      spim_step (display);\
		      running_in_delay_slot = 0;		\
		    }						\
		    /* -4 since PC is bumped after this inst */	\
		    reg().PC = (TARGET) - BYTES_PER_WORD;		\
		 }


/* If the delayed_load flag is false, the result from a load is available
   immediate.  If the delayed_load flag is true, the result from a load is
   not available until the subsequent instruction has executed (as in the
   real machine). We need a two element shift register for the value and its
   destination, as the instruction following the load can itself be a load
   instruction. */

#define LOAD_INST(DEST_A, LD, MASK)				\
		 {						\
		  LOAD_INST_BASE (DEST_A, (LD & (MASK)))	\
		 }


#define LOAD_INST_BASE(DEST_A, VALUE)				\
		{						\
		  if (delayed_loads)				\
		    {						\
		      delayed_load_addr1 = (DEST_A);		\
		      delayed_load_value1 = (VALUE); 		\
		    }						\
		    else					\
		    {						\
		      *(DEST_A) = (VALUE);			\
		    }						\
		 }


#define DO_DELAYED_UPDATE()					\
		if (delayed_loads)				\
		  {						\
		    /* Check for delayed updates */		\
		    if (delayed_load_addr2 != NULL)		\
		      {						\
			*delayed_load_addr2 = delayed_load_value2; \
		      }						\
		    delayed_load_addr2 = delayed_load_addr1;	\
		    delayed_load_value2 = delayed_load_value1;	\
		    delayed_load_addr1 = NULL;			\
		   }



/* Run the program stored in memory, starting at address PC for
   1 instruction. If flag DISPLAY is true, print
   each instruction before it executes. Return true if program's
   execution can continue. */

bool
spim_step (bool display)
{
  instruction *inst;
  static reg_word *delayed_load_addr1 = NULL, delayed_load_value1;
  static reg_word *delayed_load_addr2 = NULL, delayed_load_value2;

	reg().R[0] = 0;		/* Maintain invariant value */

	inst = read_mem_inst (reg().PC);
	if (reg().exception_occurred) /* In reading instruction */
	{
		reg().exception_occurred = 0;
		handle_exception ();
		return true;
	}
	else if (inst == NULL)
	{
		run_error ("Attempt to execute non-instruction at 0x%08x\n", reg().PC);
		return false;
	}
	else if (EXPR (inst) != NULL
		&& EXPR (inst)->symbol != NULL
		&& EXPR (inst)->symbol->addr == 0)
	{
			run_error ("Instruction references undefined symbol at 0x%08x\n  %s", reg().PC, inst_to_string(reg().PC));
		return false;
	}

	if (display)
		print_inst (reg().PC);

#ifdef TEST_ASM
	  test_assembly (inst);
#endif

	  DO_DELAYED_UPDATE ();

	  switch (OPCODE (inst))
	    {
	    case Y_ADD_OP:
	      {
		reg_word vs = reg().R[RS (inst)], vt = reg().R[RT (inst)];
		reg_word sum = vs + vt;

		if (ARITH_OVFL (sum, vs, vt))
		  RAISE_EXCEPTION (ExcCode_Ov, break);
		reg().R[RD (inst)] = sum;
		break;
	      }

	    case Y_ADDI_OP:
	      {
		reg_word vs = reg().R[RS (inst)], imm = (short) IMM (inst);
		reg_word sum = vs + imm;

		if (ARITH_OVFL (sum, vs, imm))
		  RAISE_EXCEPTION (ExcCode_Ov, break);
		reg().R[RT (inst)] = sum;
		break;
	      }

	    case Y_ADDIU_OP:
	      reg().R[RT (inst)] = reg().R[RS (inst)] + (short) IMM (inst);
	      break;

	    case Y_ADDU_OP:
	      reg().R[RD (inst)] = reg().R[RS (inst)] + reg().R[RT (inst)];
	      break;

	    case Y_AND_OP:
	      reg().R[RD (inst)] = reg().R[RS (inst)] & reg().R[RT (inst)];
	      break;

	    case Y_ANDI_OP:
	      reg().R[RT (inst)] = reg().R[RS (inst)] & (0xffff & IMM (inst));
	      break;

	    case Y_BC2F_OP:
	    case Y_BC2FL_OP:
	    case Y_BC2T_OP:
	    case Y_BC2TL_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_BEQ_OP:
	      BRANCH_INST (reg().R[RS (inst)] == reg().R[RT (inst)],
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BEQL_OP:
	      BRANCH_INST (reg().R[RS (inst)] == reg().R[RT (inst)],
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BGEZ_OP:
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BGEZL_OP:
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BGEZAL_OP:
	      reg().R[31] = reg().PC + (delayed_branches ? 2 * BYTES_PER_WORD : BYTES_PER_WORD);
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BGEZALL_OP:
	      reg().R[31] = reg().PC + (delayed_branches ? 2 * BYTES_PER_WORD : BYTES_PER_WORD);
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BGTZ_OP:
	      BRANCH_INST (reg().R[RS (inst)] != 0 && SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BGTZL_OP:
	      BRANCH_INST (reg().R[RS (inst)] != 0 && SIGN_BIT (reg().R[RS (inst)]) == 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BLEZ_OP:
	      BRANCH_INST (reg().R[RS (inst)] == 0 || SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BLEZL_OP:
	      BRANCH_INST (reg().R[RS (inst)] == 0 || SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BLTZ_OP:
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BLTZL_OP:
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BLTZAL_OP:
	      reg().R[31] = reg().PC + (delayed_branches ? 2 * BYTES_PER_WORD : BYTES_PER_WORD);
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BLTZALL_OP:
	      reg().R[31] = reg().PC + (delayed_branches ? 2 * BYTES_PER_WORD : BYTES_PER_WORD);
	      BRANCH_INST (SIGN_BIT (reg().R[RS (inst)]) != 0,
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BNE_OP:
	      BRANCH_INST (reg().R[RS (inst)] != reg().R[RT (inst)],
			   reg().PC + IDISP (inst),
			   0);
	      break;

	    case Y_BNEL_OP:
	      BRANCH_INST (reg().R[RS (inst)] != reg().R[RT (inst)],
			   reg().PC + IDISP (inst),
			   1);
	      break;

	    case Y_BREAK_OP:
	      if (RD (inst) == 1)
		/* Debugger breakpoint */
		RAISE_EXCEPTION (ExcCode_Bp, return true)
	      else
		RAISE_EXCEPTION (ExcCode_Bp, break);

	    case Y_CACHE_OP:
	      break;		/* Memory details not implemented */

	    case Y_CFC0_OP:
	      reg().R[RT (inst)] = reg().CCR[0][RD (inst)];
	      break;

	    case Y_CFC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_CLO_OP:
	      {
		reg_word val = reg().R[RS (inst)];
		int i;
		for (i = 31; 0 <= i; i -= 1)
		  if (((val >> i) & 0x1) == 0) break;

		reg().R[RD (inst) ] = 31 - i;
		break;
	      }

	    case Y_CLZ_OP:
	      {
		reg_word val = reg().R[RS (inst)];
		int i;
		for (i = 31; 0 <= i; i -= 1)
		  if (((val >> i) & 0x1) == 1) break;

		reg().R[RD (inst) ] = 31 - i;
		break;
	      }

	    case Y_COP2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_CTC0_OP:
	      reg().CCR[0][RD (inst)] = reg().R[RT (inst)];
	      break;

	    case Y_CTC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_DIV_OP:
	      /* The behavior of this instruction is undefined on divide by
		 zero or overflow. */
	      if (reg().R[RT (inst)] != 0
		  && !(reg().R[RS (inst)] == (reg_word)0x80000000
                       && reg().R[RT (inst)] == (reg_word)0xffffffff))
		{
		  reg().LO = (reg_word) reg().R[RS (inst)] / (reg_word) reg().R[RT (inst)];
		  reg().HI = (reg_word) reg().R[RS (inst)] % (reg_word) reg().R[RT (inst)];
		}
	      break;

	    case Y_DIVU_OP:
	      /* The behavior of this instruction is undefined on divide by
		 zero or overflow. */
	      if (reg().R[RT (inst)] != 0
		  && !(reg().R[RS (inst)] == (reg_word)0x80000000
                       && reg().R[RT (inst)] == (reg_word)0xffffffff))
		{
		  reg().LO = (u_reg_word) reg().R[RS (inst)] / (u_reg_word) reg().R[RT (inst)];
		  reg().HI = (u_reg_word) reg().R[RS (inst)] % (u_reg_word) reg().R[RT (inst)];
		}
	      break;

	    case Y_ERET_OP:
	      {
		reg().CP0_Status &= ~CP0_Status_EXL;	/* Clear EXL bit */
		JUMP_INST (reg().CP0_EPC); 		/* Jump to EPC */
	      }
	      break;

	    case Y_J_OP:
	      JUMP_INST (((reg().PC & 0xf0000000) | TARGET (inst) << 2));
	      break;

	    case Y_JAL_OP:
	      if (delayed_branches)
		reg().R[31] = reg().PC + 2 * BYTES_PER_WORD;
	      else
		reg().R[31] = reg().PC + BYTES_PER_WORD;
	      JUMP_INST (((reg().PC & 0xf0000000) | (TARGET (inst) << 2)));
	      break;

	    case Y_JALR_OP:
	      {
		mem_addr tmp = reg().R[RS (inst)];

		if (delayed_branches)
		  reg().R[RD (inst)] = reg().PC + 2 * BYTES_PER_WORD;
		else
		  reg().R[RD (inst)] = reg().PC + BYTES_PER_WORD;
		JUMP_INST (tmp);
	      }
	      break;

	    case Y_JR_OP:
	      {
		mem_addr tmp = reg().R[RS (inst)];

		JUMP_INST (tmp);
	      }
	      break;

	    case Y_LB_OP:
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_byte (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffffffff);
	      break;

	    case Y_LBU_OP:
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_byte (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xff);
	      break;

	    case Y_LH_OP:
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_half (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffffffff);
	      break;

	    case Y_LHU_OP:
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_half (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffff);
	      break;

	    case Y_LL_OP:
	      /* Uniprocess, so this instruction is just a load */
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_word (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffffffff);
	      break;

	    case Y_LUI_OP:
	      reg().R[RT (inst)] = (IMM (inst) << 16) & 0xffff0000;
	      break;

	    case Y_LW_OP:
	      LOAD_INST (&reg().R[RT (inst)],
			 read_mem_word (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffffffff);
	      break;

	    case Y_LDC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_LWC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_LWL_OP:
	      {
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		reg_word word;	/* Can't be register */
		int byte = addr & 0x3;
		reg_word reg_val = reg().R[RT (inst)];

		word = read_mem_word (addr & 0xfffffffc);
		if (!reg().exception_occurred)
#ifdef SPIM_BIGENDIAN
		  switch (byte)
		    {
		    case 0:
		      word = word;
		      break;

		    case 1:
		      word = ((word & 0xffffff) << 8) | (reg_val & 0xff);
		      break;

		    case 2:
		      word = ((word & 0xffff) << 16) | (reg_val & 0xffff);
		      break;

		    case 3:
		      word = ((word & 0xff) << 24) | (reg_val & 0xffffff);
		      break;
		    }
#else
		switch (byte)
		  {
		  case 0:
		    word = ((word & 0xff) << 24) | (reg_val & 0xffffff);
		    break;

		  case 1:
		    word = ((word & 0xffff) << 16) | (reg_val & 0xffff);
		    break;

		  case 2:
		    word = ((word & 0xffffff) << 8) | (reg_val & 0xff);
		    break;

		  case 3:
		    break;
		  }
#endif
		LOAD_INST_BASE (&reg().R[RT (inst)], word);
		break;
	      }

	    case Y_LWR_OP:
	      {
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		reg_word word;	/* Can't be register */
		int byte = addr & 0x3;
		reg_word reg_val = reg().R[RT (inst)];

		word = read_mem_word (addr & 0xfffffffc);
		if (!reg().exception_occurred)
#ifdef SPIM_BIGENDIAN
		  switch (byte)
		    {
		    case 0:
		      word = (reg_val & 0xffffff00) | ((unsigned)(word & 0xff000000) >> 24);
		      break;

		    case 1:
		      word = (reg_val & 0xffff0000) | ((unsigned)(word & 0xffff0000) >> 16);
		      break;

		    case 2:
		      word = (reg_val & 0xff000000) | ((unsigned)(word & 0xffffff00) >> 8);
		      break;

		    case 3:
		      word = word;
		      break;
		    }
#else
		switch (byte)
		  {
		  case 0:
		    break;

		  case 1:
		    word = (reg_val & 0xff000000) | ((word & 0xffffff00) >> 8);
		    break;

		  case 2:
		    word = (reg_val & 0xffff0000) | ((word & 0xffff0000) >> 16);
		    break;

		  case 3:
		    word = (reg_val & 0xffffff00) | ((word & 0xff000000) >> 24);
		    break;
		  }
#endif
		LOAD_INST_BASE (&reg().R[RT (inst)], word);
		break;
	      }

	    case Y_MADD_OP:
	    case Y_MADDU_OP:
	      {
		reg_word lo = reg().LO, hi = reg().HI;
		reg_word tmp;
		if (OPCODE (inst) == Y_MADD_OP)
		  {
		    signed_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
		  }
		else		/* Y_MADDU_OP */
		  {
		    unsigned_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
		  }
		tmp = lo + reg().LO;
		if ((unsigned)tmp < (unsigned)reg().LO || (unsigned)tmp < (unsigned)lo)
		  {
		    /* Addition of low-order word overflows */
		    hi += 1;
		  }
		reg().LO = tmp;
		reg().HI = hi + reg().HI;
		break;
	      }

	    case Y_MFC0_OP:
	      reg().R[RT (inst)] = reg().CPR[0][FS (inst)];
	      break;

	    case Y_MFC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_MFHI_OP:
	      reg().R[RD (inst)] = reg().HI;
	      break;

	    case Y_MFLO_OP:
	      reg().R[RD (inst)] = reg().LO;
	      break;

	    case Y_MOVN_OP:
	      if (reg().R[RT (inst)] != 0)
		reg().R[RD (inst)] = reg().R[RS (inst)];
	      break;

	    case Y_MOVZ_OP:
	      if (reg().R[RT (inst)] == 0)
		reg().R[RD (inst)] = reg().R[RS (inst)];
	      break;

	    case Y_MSUB_OP:
	    case Y_MSUBU_OP:
	      {
		reg_word lo = reg().LO, hi = reg().HI;
		reg_word tmp;

		if (OPCODE (inst) == Y_MSUB_OP)
		  {
		    signed_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
		  }
		else		/* Y_MSUBU_OP */
		  {
		    unsigned_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
		  }

		tmp = lo - reg().LO;
		if ((unsigned)reg().LO > (unsigned)lo)
		  {
		    /* Subtraction of low-order word borrows */
		    hi -= 1;
		  }
		reg().LO = tmp;
		reg().HI = hi - reg().HI;
		break;
	      }

	    case Y_MTC0_OP:
	      reg().CPR[0][FS (inst)] = reg().R[RT (inst)];
	      switch (FS (inst))
		{
		case CP0_Compare_Reg:
		  reg().CP0_Cause &= ~CP0_Cause_IP7;	/* Writing clears HW interrupt 5 */
		  break;

		case CP0_Status_Reg:
		  reg().CP0_Status &= CP0_Status_Mask;
		  reg().CP0_Status |= ((CP0_Status_CU & 0x30000000) | CP0_Status_UM);
		  break;

		case CP0_Cause_Reg:
		  reg().CPR[0][FS (inst)] &= CP0_Cause_Mask;
		  break;

		case CP0_Config_Reg:
		  reg().CPR[0][FS (inst)] &= CP0_Config_Mask;
		  break;

		default:
		  break;
		}
	      break;

	    case Y_MTC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_MTHI_OP:
	      reg().HI = reg().R[RS (inst)];
	      break;

	    case Y_MTLO_OP:
	      reg().LO = reg().R[RS (inst)];
	      break;

	    case Y_MUL_OP:
	      signed_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
	      reg().R[RD (inst)] = reg().LO;
	      break;

	    case Y_MULT_OP:
	      signed_multiply(reg().R[RS (inst)], reg().R[RT (inst)]);
	      break;

	    case Y_MULTU_OP:
	      unsigned_multiply (reg().R[RS (inst)], reg().R[RT (inst)]);
	      break;

	    case Y_NOR_OP:
	      reg().R[RD (inst)] = ~ (reg().R[RS (inst)] | reg().R[RT (inst)]);
	      break;

	    case Y_OR_OP:
	      reg().R[RD (inst)] = reg().R[RS (inst)] | reg().R[RT (inst)];
	      break;

	    case Y_ORI_OP:
	      reg().R[RT (inst)] = reg().R[RS (inst)] | (0xffff & IMM (inst));
	      break;

	    case Y_PREF_OP:
	      break;		/* Memory details not implemented */

	    case Y_RFE_OP:
#ifdef MIPS1
	      /* This is MIPS-I, not compatible with MIPS32 or the
		 definition of the bits in the CP0 Status register in that
		 architecture. */
	      reg().CP0_Status = (reg().CP0_Status & 0xfffffff0) | ((reg().CP0_Status & 0x3c) >> 2);
#else
	      RAISE_EXCEPTION (ExcCode_RI, {}); /* Not MIPS32 instruction */
#endif
	      break;

	    case Y_SB_OP:
	      set_mem_byte (reg().R[BASE (inst)] + IOFFSET (inst), reg().R[RT (inst)]);
	      break;

	    case Y_SC_OP:
	      /* Uniprocessor, so instruction is just a store */
	      set_mem_word (reg().R[BASE (inst)] + IOFFSET (inst), reg().R[RT (inst)]);
	      break;

	    case Y_SDC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_SH_OP:
	      set_mem_half (reg().R[BASE (inst)] + IOFFSET (inst), reg().R[RT (inst)]);
	      break;

	    case Y_SLL_OP:
	      {
		int shamt = SHAMT (inst);

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = reg().R[RT (inst)] << shamt;
		else
		  reg().R[RD (inst)] = reg().R[RT (inst)];
		break;
	      }

	    case Y_SLLV_OP:
	      {
		int shamt = (reg().R[RS (inst)] & 0x1f);

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = reg().R[RT (inst)] << shamt;
		else
		  reg().R[RD (inst)] = reg().R[RT (inst)];
		break;
	      }

	    case Y_SLT_OP:
	      if (reg().R[RS (inst)] < reg().R[RT (inst)])
		reg().R[RD (inst)] = 1;
	      else
		reg().R[RD (inst)] = 0;
	      break;

	    case Y_SLTI_OP:
	      if (reg().R[RS (inst)] < (short) IMM (inst))
		reg().R[RT (inst)] = 1;
	      else
		reg().R[RT (inst)] = 0;
	      break;

	    case Y_SLTIU_OP:
	      {
		int x = (short) IMM (inst);

		if ((u_reg_word) reg().R[RS (inst)] < (u_reg_word) x)
		  reg().R[RT (inst)] = 1;
		else
		  reg().R[RT (inst)] = 0;
		break;
	      }

	    case Y_SLTU_OP:
	      if ((u_reg_word) reg().R[RS (inst)] < (u_reg_word) reg().R[RT (inst)])
		reg().R[RD (inst)] = 1;
	      else
		reg().R[RD (inst)] = 0;
	      break;

	    case Y_SRA_OP:
	      {
		int shamt = SHAMT (inst);
		reg_word val = reg().R[RT (inst)];

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = val >> shamt;
		else
		  reg().R[RD (inst)] = val;
		break;
	      }

	    case Y_SRAV_OP:
	      {
		int shamt = reg().R[RS (inst)] & 0x1f;
		reg_word val = reg().R[RT (inst)];

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = val >> shamt;
		else
		  reg().R[RD (inst)] = val;
		break;
	      }

	    case Y_SRL_OP:
	      {
		int shamt = SHAMT (inst);
		u_reg_word val = reg().R[RT (inst)];

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = val >> shamt;
		else
		  reg().R[RD (inst)] = val;
		break;
	      }

	    case Y_SRLV_OP:
	      {
		int shamt = reg().R[RS (inst)] & 0x1f;
		u_reg_word val = reg().R[RT (inst)];

		if (shamt >= 0 && shamt < 32)
		  reg().R[RD (inst)] = val >> shamt;
		else
		  reg().R[RD (inst)] = val;
		break;
	      }

	    case Y_SUB_OP:
	      {
		reg_word vs = reg().R[RS (inst)], vt = reg().R[RT (inst)];
		reg_word diff = vs - vt;

		if (SIGN_BIT (vs) != SIGN_BIT (vt)
		    && SIGN_BIT (vs) != SIGN_BIT (diff))
		  RAISE_EXCEPTION (ExcCode_Ov, break);
		reg().R[RD (inst)] = diff;
		break;
	      }

	    case Y_SUBU_OP:
	      reg().R[RD (inst)] = (u_reg_word) reg().R[RS (inst)] - (u_reg_word) reg().R[RT (inst)];
	      break;

	    case Y_SW_OP:
	      set_mem_word (reg().R[BASE (inst)] + IOFFSET (inst), reg().R[RT (inst)]);
	      break;

	    case Y_SWC2_OP:
	      RAISE_EXCEPTION (ExcCode_CpU, {}); /* No Coprocessor 2 */
	      break;

	    case Y_SWL_OP:
	      {
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		mem_word data;
		reg_word regw = reg().R[RT (inst)];
		int byte = addr & 0x3;

		data = read_mem_word (addr & 0xfffffffc);
#ifdef SPIM_BIGENDIAN
		switch (byte)
		  {
		  case 0:
		    data = regw;
		    break;

		  case 1:
		    data = (data & 0xff000000) | (regw >> 8 & 0xffffff);
		    break;

		  case 2:
		    data = (data & 0xffff0000) | (regw >> 16 & 0xffff);
		    break;

		  case 3:
		    data = (data & 0xffffff00) | (regw >> 24 & 0xff);
		    break;
		  }
#else
		switch (byte)
		  {
		  case 0:
		    data = (data & 0xffffff00) | (regw >> 24 & 0xff);
		    break;

		  case 1:
		    data = (data & 0xffff0000) | (regw >> 16 & 0xffff);
		    break;

		  case 2:
		    data = (data & 0xff000000) | (regw >> 8 & 0xffffff);
		    break;

		  case 3:
		    data = regw;
		    break;
		  }
#endif
		set_mem_word (addr & 0xfffffffc, data);
		break;
	      }

	    case Y_SWR_OP:
	      {
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		mem_word data;
		reg_word regw = reg().R[RT (inst)];
		int byte = addr & 0x3;

		data = read_mem_word (addr & 0xfffffffc);
#ifdef SPIM_BIGENDIAN
		switch (byte)
		  {
		  case 0:
		    data = ((regw << 24) & 0xff000000) | (data & 0xffffff);
		    break;

		  case 1:
		    data = ((regw << 16) & 0xffff0000) | (data & 0xffff);
		    break;

		  case 2:
		    data = ((regw << 8) & 0xffffff00) | (data & 0xff) ;
		    break;

		  case 3:
		    data = regw;
		    break;
		  }
#else
		switch (byte)
		  {
		  case 0:
		    data = regw;
		    break;

		  case 1:
		    data = ((regw << 8) & 0xffffff00) | (data & 0xff) ;
		    break;

		  case 2:
		    data = ((regw << 16) & 0xffff0000) | (data & 0xffff);
		    break;

		  case 3:
		    data = ((regw << 24) & 0xff000000) | (data & 0xffffff);
		    break;
		  }
#endif
		set_mem_word (addr & 0xfffffffc, data);
		break;
	      }

	    case Y_SYNC_OP:
	      break;		/* Memory details not implemented */

	    case Y_SYSCALL_OP:
	      if (!do_syscall ())
		return false;
	      break;

	    case Y_TEQ_OP:
	      if (reg().R[RS (inst)] == reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TEQI_OP:
	      if (reg().R[RS (inst)] == IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TGE_OP:
	      if (reg().R[RS (inst)] >= reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TGEI_OP:
	      if (reg().R[RS (inst)] >= IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TGEIU_OP:
	      if ((u_reg_word) reg().R[RS (inst)] >= (u_reg_word) IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TGEU_OP:
	      if ((u_reg_word) reg().R[RS (inst)] >= (u_reg_word) reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TLBP_OP:
	      RAISE_EXCEPTION(ExcCode_RI, {}); /* TLB not implemented */
	      break;

	    case Y_TLBR_OP:
	      RAISE_EXCEPTION(ExcCode_RI, {}); /* TLB not implemented */
	      break;

	    case Y_TLBWI_OP:
	      RAISE_EXCEPTION(ExcCode_RI, {}); /* TLB not implemented */
	      break;

	    case Y_TLBWR_OP:
	      RAISE_EXCEPTION(ExcCode_RI, {}); /* TLB not implemented */
	      break;

	    case Y_TLT_OP:
	      if (reg().R[RS (inst)] < reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TLTI_OP:
	      if (reg().R[RS (inst)] < IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TLTIU_OP:
	      if ((u_reg_word) reg().R[RS (inst)] < (u_reg_word) IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TLTU_OP:
	      if ((u_reg_word) reg().R[RS (inst)] < (u_reg_word) reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TNE_OP:
	      if (reg().R[RS (inst)] != reg().R[RT (inst)])
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_TNEI_OP:
	      if (reg().R[RS (inst)] != IMM (inst))
		RAISE_EXCEPTION(ExcCode_Tr, {});
	      break;

	    case Y_XOR_OP:
	      reg().R[RD (inst)] = reg().R[RS (inst)] ^ reg().R[RT (inst)];
	      break;

	    case Y_XORI_OP:
	      reg().R[RT (inst)] = reg().R[RS (inst)] ^ (0xffff & IMM (inst));
	      break;


	      /* FPA Operations */

	    case Y_ABS_S_OP:
	      SET_FPR_S (reg(), FD (inst), fabs (FPR_S (reg(), FS (inst))));
	      break;

	    case Y_ABS_D_OP:
	      SET_FPR_D (reg(), FD (inst), fabs (FPR_D (reg(), FS (inst))));
	      break;

	    case Y_ADD_S_OP:
	      SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)) + FPR_S (reg(), FT (inst)));
	      /* Should trap on inexact/overflow/underflow */
	      break;

	    case Y_ADD_D_OP:
	      SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)) + FPR_D (reg(), FT (inst)));
	      /* Should trap on inexact/overflow/underflow */
	      break;

	    case Y_BC1F_OP:
	    case Y_BC1FL_OP:
	    case Y_BC1T_OP:
	    case Y_BC1TL_OP:
	      {
		int cc = CC (inst);
		int nd = ND (inst);	/* 1 => nullify */
		int tf = TF (inst);	/* 0 => BC1F, 1 => BC1T */
		BRANCH_INST (FCC(reg(), cc) == tf,
			     reg().PC + IDISP (inst),
			     nd);
		break;
	      }

	    case Y_C_F_S_OP:
	    case Y_C_UN_S_OP:
	    case Y_C_EQ_S_OP:
	    case Y_C_UEQ_S_OP:
	    case Y_C_OLT_S_OP:
	    case Y_C_OLE_S_OP:
	    case Y_C_ULT_S_OP:
	    case Y_C_ULE_S_OP:
	    case Y_C_SF_S_OP:
	    case Y_C_NGLE_S_OP:
	    case Y_C_SEQ_S_OP:
	    case Y_C_NGL_S_OP:
	    case Y_C_LT_S_OP:
	    case Y_C_NGE_S_OP:
	    case Y_C_LE_S_OP:
	    case Y_C_NGT_S_OP:
	      {
		float v1 = FPR_S (reg(), FS (inst)), v2 = FPR_S (reg(), FT (inst));
		double dv1 = v1, dv2 = v2;
		int cond = COND (inst);
		int cc = CCFP (inst);

		if (NaN (dv1) || NaN (dv2))
		  {
		    if (cond & COND_IN)
		      {
			RAISE_EXCEPTION (ExcCode_FPE, break);
		      }
		    set_fpu_cc (cond, cc, 0, 0, 1);
		  }
		else
		  {
		    set_fpu_cc (cond, cc, v1 < v2, v1 == v2, 0);
		  }
	      }
	      break;

	    case Y_C_F_D_OP:
	    case Y_C_UN_D_OP:
	    case Y_C_EQ_D_OP:
	    case Y_C_UEQ_D_OP:
	    case Y_C_OLT_D_OP:
	    case Y_C_OLE_D_OP:
	    case Y_C_ULT_D_OP:
	    case Y_C_ULE_D_OP:
	    case Y_C_SF_D_OP:
	    case Y_C_NGLE_D_OP:
	    case Y_C_SEQ_D_OP:
	    case Y_C_NGL_D_OP:
	    case Y_C_LT_D_OP:
	    case Y_C_NGE_D_OP:
	    case Y_C_LE_D_OP:
	    case Y_C_NGT_D_OP:
	      {
		double v1 = FPR_D (reg(), FS (inst)), v2 = FPR_D (reg(), FT (inst));
		int cond = COND (inst);
		int cc = CCFP(inst);

		if (NaN (v1) || NaN (v2))
		  {
		    if (cond & COND_IN)
		      {
			RAISE_EXCEPTION (ExcCode_FPE, break);
		      }
		    set_fpu_cc (cond, cc, 0, 0, 1);
		  }
		else
		  {
		    set_fpu_cc (cond, cc, v1 < v2, v1 == v2, 0);
		  }
	      }
	      break;

	    case Y_CFC1_OP:
	      reg().R[RT (inst)] = reg().FCR[FS (inst)];
	      break;

	    case Y_CTC1_OP:
	      reg().FCR[FS (inst)] = reg().R[RT (inst)];
        
	      if (FIR_REG == FS (inst))
		{
		  /* Read only register */
		  reg().FIR = FIR_MASK;
		}
		  else if (25 == FS (inst)) 	// FCCR => set FCC to R[RT]
		{
			ASSIGN_FCC(reg(), reg().R[RT (inst)]);
		}
	      else if (FCSR_REG == FS (inst))
		{
		  if ((reg().R[RT (inst)] & ~FCSR_MASK) != 0)
		    /* Trying to set unsupported mode */
		    RAISE_EXCEPTION (ExcCode_FPE, {});
		}
	      break;

	    case Y_CEIL_W_D_OP:
	      {
		double val = FPR_D (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)ceil (val));
		break;
	      }

	    case Y_CEIL_W_S_OP:
	      {
		double val = (double) FPR_S (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)ceil (val));
		break;
	      }

	    case Y_CVT_D_S_OP:
	      {
		double val = FPR_S (reg(), FS (inst));

		SET_FPR_D (reg(), FD (inst), val);
		break;
	      }

	    case Y_CVT_D_W_OP:
	      {
		double val = (double) FPR_W (reg(), FS (inst));

		SET_FPR_D (reg(), FD (inst), val);
		break;
	      }

	    case Y_CVT_S_D_OP:
	      {
		float val = (float) FPR_D (reg(), FS (inst));

		SET_FPR_S (reg(), FD (inst), val);
		break;
	      }

	    case Y_CVT_S_W_OP:
	      {
		float val = (float) FPR_W (reg(), FS (inst));

		SET_FPR_S (reg(), FD (inst), val);
		break;
	      }

	    case Y_CVT_W_D_OP:
	      {
		int val = (int32) FPR_D (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), val);
		break;
	      }

	    case Y_CVT_W_S_OP:
	      {
		int val = (int32) FPR_S (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), val);
		break;
	      }

	    case Y_DIV_S_OP:
	      SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)) / FPR_S (reg(), FT (inst)));
	      break;

	    case Y_DIV_D_OP:
	      SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)) / FPR_D (reg(), FT (inst)));
	      break;

	    case Y_FLOOR_W_D_OP:
	      {
		double val = FPR_D (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)floor (val));
		break;
	      }

	    case Y_FLOOR_W_S_OP:
	      {
		double val = (double) FPR_S (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)floor (val));
		break;
	      }

	    case Y_LDC1_OP:
	      {
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		if ((addr & 0x3) != 0)
		  RAISE_EXCEPTION (ExcCode_AdEL, reg().CP0_BadVAddr = addr);

		LOAD_INST ((reg_word *) &FPR_S(reg(), FT (inst)),
			   read_mem_word (addr),
			   0xffffffff);
		LOAD_INST ((reg_word *) &FPR_S(reg(), FT (inst) + 1),
			   read_mem_word (addr + sizeof(mem_word)),
			   0xffffffff);
		break;
	      }

	    case Y_LWC1_OP:
	      LOAD_INST ((reg_word *) &FPR_S(reg(), FT (inst)),
			 read_mem_word (reg().R[BASE (inst)] + IOFFSET (inst)),
			 0xffffffff);
	      break;

	    case Y_MFC1_OP:
	      {
		float val = FPR_S(reg(), FS (inst));
		reg_word *vp = (reg_word *) &val;

		reg().R[RT (inst)] = *vp; /* Fool coercion */
		break;
	      }

	    case Y_MOV_S_OP:
	      SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)));
	      break;

	    case Y_MOV_D_OP:
	      SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)));
	      break;

	    case Y_MOVF_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) == 0)
		  reg().R[RD (inst)] = reg().R[RS (inst)];
		break;
	      }

	    case Y_MOVF_D_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) == 0)
		  SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)));
		break;
	      }

	    case Y_MOVF_S_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) == 0)
		  SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)));
		break;

	      }

	    case Y_MOVN_D_OP:
	      {
		if (reg().R[RT (inst)] != 0)
		  SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)));
		break;
	      }

	    case Y_MOVN_S_OP:
	      {
		if (reg().R[RT (inst)] != 0)
		  SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)));
		break;
	      }

	    case Y_MOVT_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) != 0)
		  reg().R[RD (inst)] = reg().R[RS (inst)];
		break;
	      }

	    case Y_MOVT_D_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) != 0)
		  SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)));
		break;
	      }

	    case Y_MOVT_S_OP:
	      {
		int cc = CC (inst);
		if (FCC(reg(), cc) != 0)
		  SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)));
		break;

	      }

	    case Y_MOVZ_D_OP:
	      {
		if (reg().R[RT (inst)] == 0)
		  SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)));
		break;
	      }

	    case Y_MOVZ_S_OP:
	      {
		if (reg().R[RT (inst)] == 0)
		  SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)));
		break;

	      }

	    case Y_MTC1_OP:
	      {
		reg_word word = reg().R[RT (inst)];
		float *wp = (float *) &word;

		SET_FPR_S(reg(), FS (inst), *wp); /* fool coercion */
		break;
	      }

	    case Y_MUL_S_OP:
	      SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)) * FPR_S (reg(), FT (inst)));
	      break;

	    case Y_MUL_D_OP:
	      SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)) * FPR_D (reg(), FT (inst)));
	      break;

	    case Y_NEG_S_OP:
	      SET_FPR_S (reg(), FD (inst), -FPR_S (reg(), FS (inst)));
	      break;

	    case Y_NEG_D_OP:
	      SET_FPR_D (reg(), FD (inst), -FPR_D (reg(), FS (inst)));
	      break;

	    case Y_ROUND_W_D_OP:
	      {
		double val = FPR_D (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)(val + 0.5)); /* Casting truncates */
		break;
	      }

	    case Y_ROUND_W_S_OP:
	      {
		double val = (double) FPR_S (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)(val + 0.5)); /* Casting truncates */
		break;
	      }

	    case Y_SDC1_OP:
	      {
		double val = FPR_D (reg(), RT (inst));
		reg_word *vp = (reg_word*)&val;
		mem_addr addr = reg().R[BASE (inst)] + IOFFSET (inst);
		if ((addr & 0x3) != 0)
		  RAISE_EXCEPTION (ExcCode_AdEL, reg().CP0_BadVAddr = addr);

		set_mem_word (addr, *vp);
		set_mem_word (addr + sizeof(mem_word), *(vp + 1));
		break;
	      }

	    case Y_SQRT_D_OP:
	      SET_FPR_D (reg(), FD (inst), sqrt (FPR_D (reg(), FS (inst))));
	      break;

	    case Y_SQRT_S_OP:
	      SET_FPR_S (reg(), FD (inst), sqrt (FPR_S (reg(), FS (inst))));
	      break;

	    case Y_SUB_S_OP:
	      SET_FPR_S (reg(), FD (inst), FPR_S (reg(), FS (inst)) - FPR_S (reg(), FT (inst)));
	      break;

	    case Y_SUB_D_OP:
	      SET_FPR_D (reg(), FD (inst), FPR_D (reg(), FS (inst)) - FPR_D (reg(), FT (inst)));
	      break;

	    case Y_SWC1_OP:
	      {
		float val = FPR_S(reg(), RT (inst));
		reg_word *vp = (reg_word *) &val;

		set_mem_word (reg().R[BASE (inst)] + IOFFSET (inst), *vp);
		break;
	      }

	    case Y_TRUNC_W_D_OP:
	      {
		double val = FPR_D (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)val); /* Casting truncates */
		break;
	      }

	    case Y_TRUNC_W_S_OP:
	      {
		double val = (double) FPR_S (reg(), FS (inst));

		SET_FPR_W (reg(), FD (inst), (int32)val); /* Casting truncates */
		break;
	      }

	    default:
	      fatal_error ("Unknown instruction type: %d\n", OPCODE (inst));
	      break;
	    }

	  /* After instruction executes: */
	  reg().PC += BYTES_PER_WORD;

	  if (reg().exception_occurred)
	    {
		  if ((reg().CP0_Cause >> 2) > LAST_REAL_EXCEPT)
            reg().CP0_EPC = reg().PC - BYTES_PER_WORD;
	      handle_exception ();
	    }

  return true;
}


/* Multiply two 32-bit numbers, V1 and V2, to produce a 64 bit result in
   the HI/LO registers.	 The algorithm is high-school math:

	 A B
       x C D
       ------
       AD || BD
 AC || CB || 0

 where A and B are the high and low short words of V1, C and D are the short
 words of V2, AD is the product of A and D, and X || Y is (X << 16) + Y.
 Since the algorithm is programmed in C, we need to be careful not to
 overflow. */

static void
unsigned_multiply (reg_word v1, reg_word v2)
{
  u_reg_word a, b, c, d;
  u_reg_word bd, ad, cb, ac;
  u_reg_word mid, mid2, carry_mid = 0;

  a = (v1 >> 16) & 0xffff;
  b = v1 & 0xffff;
  c = (v2 >> 16) & 0xffff;
  d = v2 & 0xffff;

  bd = b * d;
  ad = a * d;
  cb = c * b;
  ac = a * c;

  mid = ad + cb;
  if (mid < ad || mid < cb)
    /* Arithmetic overflow or carry-out */
    carry_mid = 1;

  mid2 = mid + ((bd >> 16) & 0xffff);
  if (mid2 < mid || mid2 < ((bd >> 16) & 0xffff))
    /* Arithmetic overflow or carry-out */
    carry_mid += 1;

  reg().LO = (bd & 0xffff) | ((mid2 & 0xffff) << 16);
  reg().HI = ac + (carry_mid << 16) + ((mid2 >> 16) & 0xffff);
}


static void
signed_multiply (reg_word v1, reg_word v2)
{
  int neg_sign = 0;

  if (v1 < 0)
    {
      v1 = - v1;
      neg_sign = 1;
    }
  if (v2 < 0)
    {
      v2 = - v2;
      neg_sign = ! neg_sign;
    }

  unsigned_multiply (v1, v2);
  if (neg_sign)
    {
      reg().LO = ~ reg().LO;
      reg().HI = ~ reg().HI;
      reg().LO += 1;
      if (reg().LO == 0)
	reg().HI += 1;
    }
}

static void
set_fpu_cc (int cond, int cc, int less, int equal, int unordered)
{
  int result = 0;

  if (cond & COND_LT) result |= less;
  if (cond & COND_EQ) result |= equal;
  if (cond & COND_UN) result |= unordered;

  SET_FCC(reg(), cc, result);
}


void
raise_exception (int excode)
{
  if (ExcCode_Int != excode
      || ((reg().CP0_Status & CP0_Status_IE) /* Allow interrupt if IE and !EXL */
	  && !(reg().CP0_Status & CP0_Status_EXL)))
    {
      /* Ignore interrupt exception when interrupts disabled.  */
      reg().exception_occurred = 1;
	  last_exception_addr = reg().PC;

      if (running_in_delay_slot)
	{
	  /* In delay slot */
	  if ((reg().CP0_Status & CP0_Status_EXL) == 0)
	    {
	      /* Branch's addr */
	      reg().CP0_EPC = ROUND_DOWN (reg().PC - BYTES_PER_WORD, BYTES_PER_WORD);
	      /* Set BD bit to record that instruction is in delay slot */
	      reg().CP0_Cause |= CP0_Cause_BD;
	    }
	}
      else
	{
	  /* Not in delay slot */
	  if ((reg().CP0_Status & CP0_Status_EXL) == 0)
	    {
	      /* Faulting instruction's address */
	      reg().CP0_EPC = ROUND_DOWN (reg().PC, BYTES_PER_WORD);
	    }
	}
      /* ToDo: set CE field of Cause register to coprocessor causing exception */

      /* Record cause of exception */
      reg().CP0_Cause = (reg().CP0_Cause & ~CP0_Cause_ExcCode) | (excode << 2);

      /* Turn on EXL bit to prevent subsequent interrupts from affecting EPC */
      reg().CP0_Status |= CP0_Status_EXL;

#ifdef MIPS1
      reg().CP0_Status = (reg().CP0_Status & 0xffffffc0) | ((reg().CP0_Status & 0xf) << 2);
#endif
    }
}
