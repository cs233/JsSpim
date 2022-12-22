#ifndef LABEL_H
#define LABEL_H

#include "mem_image.h"
#include "reg_image.h"

typedef struct lab_use
{
  instruction *inst;		/* NULL => Data, not code */
  mem_addr addr;
  struct lab_use *next;
} label_use;


/* Symbol table information on a label. */

typedef struct lab
{
  char *name;			/* Name of label */
  long addr;			/* Address of label or 0 if not yet defined */
  unsigned global_flag : 1;	/* Non-zero => declared global */
  unsigned gp_flag : 1;		/* Non-zero => referenced off gp */
  unsigned const_flag : 1;	/* Non-zero => constant value (in addr) */
  struct lab *next;		/* Hash table link */
  struct lab *next_local;	/* Link in list of local labels */
  label_use *uses;		/* List of instructions that reference */
} label;			/* label that has not yet been defined */

#endif
