#ifndef MEM_IMAGE_H
#define MEM_IMAGE_H

#include "consts.h"
#include "types.h"
#include "instruction.h"

#include <stdlib.h>

/* Type of contents of a memory word. */

typedef int32 /*@alt unsigned int @*/ mem_word;

#define BYTE_TYPE signed char

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

void free_instructions (instruction **inst, int n);

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

    ~memimage() {
        free_instructions(text_seg, (text_top - TEXT_BOT) / BYTES_PER_WORD);
        free_instructions(k_text_seg, (k_text_top - K_TEXT_BOT) / BYTES_PER_WORD);
        if (text_seg)
            free(text_seg);
        if (text_prof)
            free(text_prof);
        if (data_seg)
            free(data_seg);
        if (stack_seg)
            free(stack_seg);
        if (special_seg)
            free(special_seg);
        if (k_text_seg)
            free(k_text_seg);
        if (k_text_prof)
            free(k_text_prof);
        if (k_data_seg)
            free(k_data_seg);

    }
} mem_image_t;

#endif
