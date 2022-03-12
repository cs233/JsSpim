#ifndef MEM_IMAGE_H
#define MEM_IMAGE_H

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

	char* prof_file_name;

} mem_image_t;

#endif