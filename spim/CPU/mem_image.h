#ifndef MEM_IMAGE_H
#define MEM_IMAGE_H

#include "instruction.h"

/* Type of contents of a memory word. */

typedef int32 /*@alt unsigned int @*/ mem_word;

#define BYTE_TYPE signed char

typedef struct memimage {
	/* The text segment. */
	instruction **text_seg = 0;
	unsigned *text_prof = 0;
	int text_modified = 0;		/* => text segment was written */
	mem_addr text_top = 0;

	/* The data segment. */
	mem_word *data_seg = 0;
	bool data_modified = 0;		/* => a data segment was written */
	short *data_seg_h = 0;		/* Points to same vector as DATA_SEG */
	BYTE_TYPE *data_seg_b = 0;		/* Ditto */
	mem_addr data_top = 0;
	mem_addr gp_midpoint = 0;		/* Middle of $gp area */

	/* The stack segment. */
	mem_word *stack_seg = 0;
	short *stack_seg_h = 0;		/* Points to same vector as STACK_SEG */
	BYTE_TYPE *stack_seg_b = 0;		/* Ditto */
	mem_addr stack_bot = 0;

	/* Used for SPIMbot stuff. */
	mem_word *special_seg = 0;
	short *special_seg_h = 0;
	BYTE_TYPE *special_seg_b = 0;

	/* The kernel text segment. */
	instruction **k_text_seg = 0;
	unsigned *k_text_prof = 0;
	mem_addr k_text_top = 0;

	/* The kernel data segment. */
	mem_word *k_data_seg = 0;
	short *k_data_seg_h = 0;
	BYTE_TYPE *k_data_seg_b = 0;
	mem_addr k_data_top = 0;

	char* prof_file_name = 0;
} mem_image_t;

#endif