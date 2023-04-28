#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include "types.h"

/* Type of a memory address.  Must be a 32-bit quantity to match MIPS.  */

typedef uint32 /*@alt int @*/ mem_addr;


/* Represenation of the expression that produce a value for an instruction's
   immediate field.  Immediates have the form: label +/- offset. */

typedef struct immexpr
{
  int offset;			/* Offset from symbol */
  struct lab *symbol;		/* Symbolic label */
  short bits;			/* > 0 => 31..16, < 0 => 15..0 */
  bool pc_relative;		/* => offset from label in code */
} imm_expr;

/* Representation of an instruction. Store the instruction fields in an
   overlapping manner similar to the real encoding (but not identical, to
   speed decoding in C code, as opposed to hardware).. */

typedef struct inst_s
{
  short opcode;

  union
    {
      /* R-type or I-type: */
      struct
	{
	  unsigned char rs;
	  unsigned char rt;

	  union
	    {
	      short imm;

	      struct
		{
		  unsigned char rd;
		  unsigned char shamt;
		} r;
	    } r_i;
	} r_i;

      /* J-type: */
      mem_addr target;
    } r_t;

  int32 encoding;
  imm_expr *expr = 0;
  char *source_line = 0;
} instruction;

#endif
