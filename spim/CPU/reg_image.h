#ifndef REG_IMAGE_H
#define REG_IMAGE_H

#include "types.h"
#include "instruction.h"

typedef int32 /*@alt unsigned int @*/ reg_word;
typedef uint32 u_reg_word;


/* General purpose registers: */

#define R_LENGTH	32

typedef struct regimage {
	int RFE_cycle;

	/* General purpose registers: */
	reg_word R[R_LENGTH];
	reg_word HI, LO;
	mem_addr PC, nPC;

	/* Floating Point Coprocessor (1) registers: */
	double *FPR = 0;		/* Dynamically allocate so overlay */
	float *FGR = 0;		/* is possible */
	int *FWR = 0;		/* is possible */

	/* Coprocessor registers: */
	reg_word CCR[4][32], CPR[4][32];

	int exception_occurred;

	bool in_kernel = false;			/* => data goes to kdata, not data */

	mem_addr next_text_pc;
	mem_addr next_k_text_pc;
	mem_addr next_data_pc;		/* Location for next datum in user process */
	mem_addr next_k_data_pc;	/* Location for next datum in kernel */
	mem_addr next_gp_item_addr;	/* Address of next item accessed off $gp */
	bool auto_alignment = true;
} reg_image_t;

#endif