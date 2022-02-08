/* SPIM S20 MIPS simulator.
   Code to create, maintain and access memory.

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
#include "reg.h"
#include "mem.h"

#include "../Spimbot/robot.h"

extern char* prof_file_name;

/* Exported Variables: */

mem_image_t mem_images[2];


/* Local functions: */

static mem_word bad_mem_read (size_t context, mem_addr addr, int mask);
static void bad_mem_write (size_t context, mem_addr addr, mem_word value, int mask);
static instruction *bad_text_read (size_t context, mem_addr addr);
static void bad_text_write (size_t context, mem_addr addr, instruction *inst);
static void free_instructions (instruction **inst, int n);


/* Local variables: */

static int32 data_size_limit, stack_size_limit, k_data_size_limit;



/* Memory is allocated in five chunks:
	text, data, stack, kernel text, and kernel data.

   The arrays are independent and have different semantics.

   text is allocated from 0x400000 up and only contains INSTRUCTIONs.
   It does not expand.

   data is allocated from 0x10000000 up.  It can be extended by the
   SBRK system call.  Programs can only read and write this segment.

   stack grows from 0x7fffefff down.  It is automatically extended.
   Programs can only read and write this segment.

   k_text is like text, except its is allocated from 0x80000000 up.

   k_data is like data, but is allocated from 0x90000000 up.

   Both kernel text and kernel data can only be accessed in kernel mode.
*/

/* The text segments contain pointers to instructions, not actual
   instructions, so they must be allocated large enough to hold as many
   pointers as there would be instructions (the two differ on machines in
   which pointers are not 32 bits long).  The following calculations round
   up in case size is not a multiple of BYTES_PER_WORD.  */

#define BYTES_TO_INST(N) (((N) + BYTES_PER_WORD - 1) / BYTES_PER_WORD * sizeof(instruction*))


void
make_memory (size_t context, int text_size, int data_size, int data_limit,
	     int stack_size, int stack_limit, int k_text_size,
	     int k_data_size, int k_data_limit)
{
  mem_image_t &mem_image = mem_images[context];

  if (data_size <= 65536)
    data_size = 65536;
  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */

  if (mem_image.text_seg == NULL) {
    mem_image.text_seg = (instruction **) xmalloc (BYTES_TO_INST(text_size));
    mem_image.text_prof = (unsigned *) xmalloc(text_size);
  }
  else
    {
      free_instructions (mem_image.text_seg, (mem_image.text_top - TEXT_BOT) / BYTES_PER_WORD);
      mem_image.text_seg = (instruction **) realloc (mem_image.text_seg, BYTES_TO_INST(text_size));
      mem_image.text_prof = (unsigned *)realloc(mem_image.text_prof,text_size);
    }
  memclr (mem_image.text_seg, BYTES_TO_INST(text_size));
  memclr(mem_image.text_prof,text_size);
  mem_image.text_top = TEXT_BOT + text_size;

  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem_image.data_seg == NULL)
    mem_image.data_seg = (mem_word *) xmalloc (data_size);
  else
    mem_image.data_seg = (mem_word *) realloc (mem_image.data_seg, data_size);
  memclr (mem_image.data_seg, data_size);
  mem_image.data_seg_b = (BYTE_TYPE *) mem_image.data_seg;
  mem_image.data_seg_h = (short *) mem_image.data_seg;
  mem_image.data_top = DATA_BOT + data_size;
  data_size_limit = data_limit;

  stack_size = ROUND_UP(stack_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem_image.stack_seg == NULL)
    mem_image.stack_seg = (mem_word *) xmalloc (stack_size);
  else
    mem_image.stack_seg = (mem_word *) realloc (mem_image.stack_seg, stack_size);
  memclr (mem_image.stack_seg, stack_size);
  mem_image.stack_seg_b = (BYTE_TYPE *) mem_image.stack_seg;
  mem_image.stack_seg_h = (short *) mem_image.stack_seg;
  mem_image.stack_bot = STACK_TOP - stack_size;
  stack_size_limit = stack_limit;

  if (mem_image.special_seg == NULL) {
    mem_image.special_seg = (mem_word *) xmalloc (SPECIAL_TOP - SPECIAL_BOT);
         mem_image.special_seg_b = (BYTE_TYPE *) mem_image.special_seg;
         mem_image.special_seg_h = (short *) mem_image.special_seg;
  }
  memclr (mem_image.special_seg, (SPECIAL_TOP - SPECIAL_BOT));

  if (mem_image.k_text_seg == NULL){
    mem_image.k_text_seg = (instruction **) xmalloc (BYTES_TO_INST(k_text_size));
    mem_image.k_text_prof = (unsigned *) xmalloc(k_text_size);
  }
  else
    {
      free_instructions (mem_image.k_text_seg,
			 (mem_image.k_text_top - K_TEXT_BOT) / BYTES_PER_WORD);
      mem_image.k_text_seg = (instruction **) realloc(mem_image.k_text_seg,
					    BYTES_TO_INST(k_text_size));
      mem_image.k_text_prof = (unsigned *) realloc(mem_image.k_text_prof, k_text_size);
    }
  memclr (mem_image.k_text_seg, BYTES_TO_INST(k_text_size));
  memclr (mem_image.k_text_prof, k_text_size);
  mem_image.k_text_top = K_TEXT_BOT + k_text_size;

  k_data_size = ROUND_UP(k_data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem_image.k_data_seg == NULL)
    mem_image.k_data_seg = (mem_word *) xmalloc (k_data_size);
  else
    mem_image.k_data_seg = (mem_word *) realloc (mem_image.k_data_seg, k_data_size);
  memclr (mem_image.k_data_seg, k_data_size);
  mem_image.k_data_seg_b = (BYTE_TYPE *) mem_image.k_data_seg;
  mem_image.k_data_seg_h = (short *) mem_image.k_data_seg;
  mem_image.k_data_top = K_DATA_BOT + k_data_size;
  k_data_size_limit = k_data_limit;

  mem_image.text_modified = true;
  mem_image.data_modified = true;
}


void mem_dump_profile() {
  mem_image_t &mem_image = mem_images[0];

  str_stream ss;
  ss_init(&ss);
  FILE *file = NULL;

  if ((prof_file_name == NULL) || (prof_file_name[0] == 0))  {
    return;
  }
  file = fopen(prof_file_name, "w");
  if (file == NULL) {
    printf("failed to open profile file: %s\n", prof_file_name);
    return;
  }

  int text_size = (mem_image.text_top - TEXT_BOT)/4;
  for (int i = 0 ; i < text_size ; ++ i) {
    instruction *inst = mem_image.text_seg[i];
    if (inst == NULL) {
      continue;
    }
    unsigned prof_count = mem_image.text_prof[i];
    mem_addr addr = TEXT_BOT + (i << 2);
    fprintf(file, "%9d ", prof_count - 1);
    format_an_inst(&ss, inst, addr);
    //print_inst_internal (&buf[10], sizeof(buf)-12, inst, addr);
    fprintf(file, "%s", ss_to_string(&ss));
    ss_clear(&ss);
    fflush(file);
  }

  fprintf(file, "\n\nkernel text segment\n\n");

  int k_text_size = (mem_image.k_text_top - K_TEXT_BOT)/4;
  for (int i = 0 ; i < k_text_size ; ++ i) {
    instruction *inst = mem_image.k_text_seg[i];
    if (inst == NULL) {
      continue;
    }
    unsigned prof_count = mem_image.k_text_prof[i];
    mem_addr addr = K_TEXT_BOT + (i << 2);
    fprintf(file, "%9d ", prof_count - 1);
    format_an_inst(&ss, inst, addr);
    //print_inst_internal (&buf[10], sizeof(buf)-12, inst, addr);
    fprintf(file, "%s", ss_to_string(&ss));
    ss_clear(&ss);
  }

  fclose(file);
}


/* Free the storage used by the old instructions in memory. */

static void
free_instructions (instruction **inst, int n)
{
  for ( ; n > 0; n --, inst ++)
    if (*inst)
      free_inst (*inst);
}


/* Expand the data segment by adding N bytes. */

void
expand_data (size_t context, int addl_bytes)
{
  mem_image_t &mem_image = mem_images[context];

  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = mem_image.data_top - DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > data_size_limit))
    {
      error ("Can't expand data segment by %d bytes to %d bytes\n",
	     addl_bytes, new_size);
      run_error ("Use -ldata # with # > %d\n", new_size);
    }
  mem_image.data_seg = (mem_word *) realloc (mem_image.data_seg, new_size);
  if (mem_image.data_seg == NULL)
    fatal_error ("realloc failed in expand_data\n");

  mem_image.data_seg_b = (BYTE_TYPE *) mem_image.data_seg;
  mem_image.data_seg_h = (short *) mem_image.data_seg;
  mem_image.data_top += delta;

  /* Zero new memory */
  for (p = mem_image.data_seg_b + old_size; p < mem_image.data_seg_b + new_size; )
    *p ++ = 0;
}


/* Expand the stack segment by adding N bytes.  Can't use REALLOC
   since it copies from bottom of memory blocks and stack grows down from
   top of its block. */

void
expand_stack (size_t context, int addl_bytes)
{
  mem_image_t &mem_image = mem_images[context];

  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = STACK_TOP - mem_image.stack_bot;
  int new_size = old_size + MAX (delta, old_size);
  mem_word *new_seg;
  mem_word *po, *pn;

  if ((addl_bytes < 0) || (new_size > stack_size_limit))
    {
      run_error ("Can't expand stack segment by %d bytes to %d bytes.\nUse -lstack # with # > %d\n",
                 addl_bytes, new_size, new_size);
    }

  new_seg = (mem_word *) xmalloc (new_size);
  memset(new_seg, 0, new_size);

  po = mem_image.stack_seg + (old_size / BYTES_PER_WORD - 1);
  pn = new_seg + (new_size / BYTES_PER_WORD - 1);
  for ( ; po >= mem_image.stack_seg ; ) *pn -- = *po --;

  free (mem_image.stack_seg);
  mem_image.stack_seg = new_seg;
  mem_image.stack_seg_b = (BYTE_TYPE *) mem_image.stack_seg;
  mem_image.stack_seg_h = (short *) mem_image.stack_seg;
  mem_image.stack_bot -= (new_size - old_size);
}


/* Expand the kernel data segment by adding N bytes. */

void
expand_k_data (size_t context, int addl_bytes)
{
  mem_image_t &mem_image = mem_images[context];

  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = mem_image.k_data_top - K_DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > k_data_size_limit))
    {
      run_error ("Can't expand kernel data segment by %d bytes to %d bytes.\nUse -lkdata # with # > %d\n",
                 addl_bytes, new_size, new_size);
    }
  mem_image.k_data_seg = (mem_word *) realloc (mem_image.k_data_seg, new_size);
  if (mem_image.k_data_seg == NULL)
    fatal_error ("realloc failed in expand_k_data\n");

  mem_image.k_data_seg_b = (BYTE_TYPE *) mem_image.k_data_seg;
  mem_image.k_data_seg_h = (short *) mem_image.k_data_seg;
  mem_image.k_data_top += delta;

  /* Zero new memory */
  for (p = mem_image.k_data_seg_b + old_size / BYTES_PER_WORD;
       p < mem_image.k_data_seg_b + new_size / BYTES_PER_WORD; )
    *p ++ = 0;
}



/* Access memory */

void*
mem_reference(size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  if ((addr >= TEXT_BOT) && (addr < mem_image.text_top))
    return addr - TEXT_BOT + (char*) mem_image.text_seg;
  else if ((addr >= DATA_BOT) && (addr < mem_image.data_top))
    return addr - DATA_BOT + (char*) mem_image.data_seg;
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP))
    return addr - mem_image.stack_bot + (char*) mem_image.stack_seg;
  else if ((addr >= K_TEXT_BOT) && (addr < mem_image.k_text_top))
    return addr - K_TEXT_BOT + (char*) mem_image.k_text_seg;
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top))
    return addr - K_DATA_BOT + (char*) mem_image.k_data_seg;
  else
    {
      run_error ("Memory address out of bounds\n");
      return NULL;
    }
}


instruction*
read_mem_inst(size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  if ((addr >= TEXT_BOT) && (addr < mem_image.text_top) && !(addr & 0x3)) {
    ++ mem_image.text_prof[(addr - TEXT_BOT) >> 2];
    return mem_image.text_seg [(addr - TEXT_BOT) >> 2];
  }
  else if ((addr >= K_TEXT_BOT) && (addr < mem_image.k_text_top) && !(addr & 0x3)) {
    ++ mem_image.k_text_prof[(addr - K_TEXT_BOT) >> 2];
    return mem_image.k_text_seg [(addr - K_TEXT_BOT) >> 2];
  }
  else
    return bad_text_read (context, addr);
}


reg_word
read_mem_byte(size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  if ((addr >= DATA_BOT) && (addr < mem_image.data_top))
    return mem_image.data_seg_b [addr - DATA_BOT];
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP))
    return mem_image.stack_seg_b [addr - mem_image.stack_bot];
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top))
    return mem_image.k_data_seg_b [addr - K_DATA_BOT];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP))
    return mem_image.special_seg_b [addr - SPECIAL_BOT];
  else
    return bad_mem_read (context, addr, 0);
}


reg_word
read_mem_half(size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  if ((addr >= DATA_BOT) && (addr < mem_image.data_top) && !(addr & 0x1))
    return mem_image.data_seg_h [(addr - DATA_BOT) >> 1];
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    return mem_image.stack_seg_h [(addr - mem_image.stack_bot) >> 1];
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top) && !(addr & 0x1))
    return mem_image.k_data_seg_h [(addr - K_DATA_BOT) >> 1];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x1))
    return mem_image.special_seg_h [(addr - SPECIAL_BOT) >> 1];
  else
    return bad_mem_read (context, addr, 0x1);
}


reg_word
read_mem_word(size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  if ((addr >= DATA_BOT) && (addr < mem_image.data_top) && !(addr & 0x3))
    return mem_image.data_seg [(addr - DATA_BOT) >> 2];
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    return mem_image.stack_seg [(addr - mem_image.stack_bot) >> 2];
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top) && !(addr & 0x3))
    return mem_image.k_data_seg [(addr - K_DATA_BOT) >> 2];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x3))
    return mem_image.special_seg [(addr - SPECIAL_BOT) >> 2];
  else
    return bad_mem_read (context, addr, 0x3);
}


void
set_mem_inst(size_t context, mem_addr addr, instruction* inst)
{
  mem_image_t &mem_image = mem_images[context];

  mem_image.text_modified = true;
  if ((addr >= TEXT_BOT) && (addr < mem_image.text_top) && !(addr & 0x3))
    mem_image.text_seg [(addr - TEXT_BOT) >> 2] = inst;
  else if ((addr >= K_TEXT_BOT) && (addr < mem_image.k_text_top) && !(addr & 0x3))
    mem_image.k_text_seg [(addr - K_TEXT_BOT) >> 2] = inst;
  else
    bad_text_write (context, addr, inst);
}


void
set_mem_byte(size_t context, mem_addr addr, reg_word value)
{
  mem_image_t &mem_image = mem_images[context];

  mem_image.data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem_image.data_top))
    mem_image.data_seg_b [addr - DATA_BOT] = (BYTE_TYPE) value;
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP))
    mem_image.stack_seg_b [addr - mem_image.stack_bot] = (BYTE_TYPE) value;
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top))
    mem_image.k_data_seg_b [addr - K_DATA_BOT] = (BYTE_TYPE) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP))
     mem_image.special_seg_b [addr - SPECIAL_BOT] = (BYTE_TYPE) value;
  else
    bad_mem_write (context, addr, value, 0);
}


void
set_mem_half(size_t context, mem_addr addr, reg_word value)
{
  mem_image_t &mem_image = mem_images[context];

  mem_image.data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem_image.data_top) && !(addr & 0x1))
    mem_image.data_seg_h [(addr - DATA_BOT) >> 1] = (short) value;
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    mem_image.stack_seg_h [(addr - mem_image.stack_bot) >> 1] = (short) value;
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top) && !(addr & 0x1))
    mem_image.k_data_seg_h [(addr - K_DATA_BOT) >> 1] = (short) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x1))
    mem_image.special_seg_h [(addr - SPECIAL_BOT) >> 1] = (short) value;
  else
    bad_mem_write (context, addr, value, 0x1);
}


void
set_mem_word(size_t context, mem_addr addr, reg_word value)
{
  mem_image_t &mem_image = mem_images[context];

  mem_image.data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem_image.data_top) && !(addr & 0x3))
    mem_image.data_seg [(addr - DATA_BOT) >> 2] = (mem_word) value;
  else if ((addr >= mem_image.stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    mem_image.stack_seg [(addr - mem_image.stack_bot) >> 2] = (mem_word) value;
  else if ((addr >= K_DATA_BOT) && (addr < mem_image.k_data_top) && !(addr & 0x3))
    mem_image.k_data_seg [(addr - K_DATA_BOT) >> 2] = (mem_word) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x3))
    mem_image.special_seg [(addr - SPECIAL_BOT) >> 2] = (mem_word) value;
  else
    bad_mem_write (context, addr, value, 0x3);
}


/* Handle the infrequent and erroneous cases in memory accesses. */

static instruction *
bad_text_read (size_t context, mem_addr addr)
{
  reg_image_t &reg_image = reg_images[context];

  RAISE_EXCEPTION (reg_image, ExcCode_IBE, CP0_BadVAddr(reg_image) = addr);
  return (inst_decode (0));
}


static void
bad_text_write (size_t context, mem_addr addr, instruction *inst)
{
  reg_image_t &reg_image = reg_images[context];

  RAISE_EXCEPTION (reg_image, ExcCode_IBE, CP0_BadVAddr(reg_image) = addr);
  set_mem_word (context, addr, ENCODING (inst));
}


static mem_word
bad_mem_read (size_t context, mem_addr addr, int mask)
{
  reg_image_t &reg_image = reg_images[context];
  mem_image_t &mem_image = mem_images[context];

  mem_word tmp;

  if ((addr & mask) != 0)
    RAISE_EXCEPTION (reg_image, ExcCode_AdEL, CP0_BadVAddr(reg_image) = addr)
  else if (addr >= TEXT_BOT && addr < mem_image.text_top)
    switch (mask)
      {
      case 0x0:
	tmp = ENCODING (mem_image.text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (3 - (addr & 0x3)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x3));
#endif
	return (0xff & tmp);

      case 0x1:
	tmp = ENCODING (mem_image.text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (2 - (addr & 0x2)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x2));
#endif
	return (0xffff & tmp);

      case 0x3:
	{
	instruction *inst = mem_image.text_seg [(addr - TEXT_BOT) >> 2];
	if (inst == NULL)
	  return 0;
	else
	  return (ENCODING (inst));
	}

      default:
	run_error ("Bad mask (0x%x) in bad_mem_read\n", mask);
      }
  else if (addr > mem_image.data_top
	   && addr < mem_image.stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > mem_image.stack_bot - 16*K*K)
    {
      /* Grow stack segment */
      expand_stack (context,mem_image.stack_bot - addr + 4);
      return (0);
    }
  else if (SPIMBOT_IO_BOT <= addr && addr <= SPIMBOT_IO_TOP)
    return (read_spimbot_IO (context, addr));
  else
    /* Address out of range */
    RAISE_EXCEPTION (reg_image, ExcCode_DBE, CP0_BadVAddr(reg_image) = addr)
  return (0);
}


static void
bad_mem_write (size_t context, mem_addr addr, mem_word value, int mask)
{
  reg_image_t &reg_image = reg_images[context];
  mem_image_t &mem_image = mem_images[context];

  mem_word tmp;

  if ((addr & mask) != 0)
    /* Unaligned address fault */
    RAISE_EXCEPTION (reg_image, ExcCode_AdES, CP0_BadVAddr(reg_image) = addr)
    else if (addr >= TEXT_BOT && addr < mem_image.text_top)
  {
    switch (mask)
    {
    case 0x0:
      tmp = ENCODING (mem_image.text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
      tmp = ((tmp & ~(0xff << (8 * (3 - (addr & 0x3)))))
	       | (value & 0xff) << (8 * (3 - (addr & 0x3))));
#else
      tmp = ((tmp & ~(0xff << (8 * (addr & 0x3))))
	       | (value & 0xff) << (8 * (addr & 0x3)));
#endif
      break;

    case 0x1:
      tmp = ENCODING (mem_image.text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
      tmp = ((tmp & ~(0xffff << (8 * (2 - (addr & 0x2)))))
	       | (value & 0xffff) << (8 * (2 - (addr & 0x2))));
#else
      tmp = ((tmp & ~(0xffff << (8 * (addr & 0x2))))
	       | (value & 0xffff) << (8 * (addr & 0x2)));
#endif
      break;

    case 0x3:
      tmp = value;
      break;

    default:
      tmp = 0;
      run_error ("Bad mask (0x%x) in bad_mem_read\n", mask);
    }

    if (mem_image.text_seg [(addr - TEXT_BOT) >> 2] != NULL)
    {
      free_inst (mem_image.text_seg[(addr - TEXT_BOT) >> 2]);
    }
    mem_image.text_seg [(addr - TEXT_BOT) >> 2] = inst_decode (tmp);

    mem_image.text_modified = true;
  }
  else if (addr > mem_image.data_top
	   && addr < mem_image.stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > mem_image.stack_bot - 16*K*K)
  {
    /* Grow stack segment */
    expand_stack (context,mem_image.stack_bot - addr + 4);
    if (addr >= mem_image.stack_bot)
    {
      if (mask == 0)
	mem_image.stack_seg_b [addr - mem_image.stack_bot] = (char)value;
      else if (mask == 1)
	mem_image.stack_seg_h [(addr - mem_image.stack_bot) >> 1] = (short)value;
      else
	mem_image.stack_seg [(addr - mem_image.stack_bot) >> 2] = value;
    }
    else
      RAISE_EXCEPTION (reg_image, ExcCode_DBE, CP0_BadVAddr(reg_image) = addr)

    mem_image.data_modified = true;
  }
  else if (SPIMBOT_IO_BOT <= addr && addr <= SPIMBOT_IO_TOP)
    write_spimbot_IO (context, addr, value);
  else
    /* Address out of range */
    RAISE_EXCEPTION (reg_image, ExcCode_DBE, CP0_BadVAddr(reg_image) = addr)
}



/* Memory-mapped IO routines. */

static int recv_control = 0;	/* No input */
static int recv_buffer;
static int recv_buffer_full_timer = 0;

static int trans_control = TRANS_READY;	/* Ready to write */
// static int trans_buffer;
static int trans_buffer_full_timer = 0;


/* Check if input is available and output is possible.  If so, update the
   memory-mapped control registers and buffers. */

void
check_memory_mapped_IO (size_t context)
{
  reg_image_t &reg_image = reg_images[context];

  if (recv_buffer_full_timer > 0)
    {
      /* Do not check for more input until this interval expires. */
      recv_buffer_full_timer -= 1;
    }
  else if (console_input_available ())
    {
      /* Read new char into the buffer and raise an interrupt, if interrupts
	 are enabled for device. */
      /* assert(recv_buffer_full_timer == 0); */
      recv_buffer = get_console_char ();
      recv_control |= RECV_READY;
      recv_buffer_full_timer = RECV_INTERVAL;
      if (recv_control & RECV_INT_ENABLE)
	{
	  RAISE_INTERRUPT (reg_image, RECV_INT_LEVEL);
	}
    }

  if (trans_buffer_full_timer > 0)
    {
      /* Do not allow output until this interval expires. */
      trans_buffer_full_timer -= 1;
    }
  else if (!(trans_control & TRANS_READY))
    {
      /* Done writing: empty the buffer and raise an interrupt, if interrupts
	 are enabled for device. */
      /* assert(trans_buffer_full_timer == 0); */
      trans_control |= TRANS_READY;
      if (trans_control & TRANS_INT_ENABLE)
	{
	  RAISE_INTERRUPT (reg_image, TRANS_INT_LEVEL);
	}
    }
}


/* Invoked on a write to the memory-mapped IO area. */
// SPIMBOT: we over-ride these functions with our own I/O routines

// static void
// write_memory_mapped_IO (mem_addr addr, mem_word value)
// {
//   reg_image_t &reg_image = reg_images[0];
//
//   switch (addr)
//     {
//     case TRANS_CTRL_ADDR:
//       /* Program can only set the interrupt enable, not ready, bit. */
//       if ((value & TRANS_INT_ENABLE) != 0)
// 	{
// 	  /* Enable interrupts: */
// 	  trans_control |= TRANS_INT_ENABLE;
// 	  if (trans_control & TRANS_READY)
// 	    {
// 	      /* Raise interrupt on enabling a ready transmitter */
// 	      RAISE_INTERRUPT (reg_image, TRANS_INT_LEVEL);
// 	    }
// 	}
//       else
// 	{
// 	  /* Disable interrupts: */
// 	  trans_control &= ~TRANS_INT_ENABLE;
// 	  CLEAR_INTERRUPT (reg_image, TRANS_INT_LEVEL); /* Clear IP bit in Cause */
// 	}
//       break;
//
//     case TRANS_BUFFER_ADDR:
//       /* Ignore write if device is not ready. */
//       if ((trans_control & TRANS_READY) != 0)
// 	{
// 	  /* Write char: */
// 	  trans_buffer = value & 0xff;
// 	  put_console_char ((char)trans_buffer);
// 	  /* Device is busy for a while: */
// 	  trans_control &= ~TRANS_READY;
// 	  trans_buffer_full_timer = TRANS_LATENCY;
//           CLEAR_INTERRUPT (reg_image, TRANS_INT_LEVEL); /* Clear IP bit in Cause */
// 	}
//       break;
//
//     case RECV_CTRL_ADDR:
//       /* Program can only set the interrupt enable, not ready, bit. */
//       if ((value & RECV_INT_ENABLE) != 0)
// 	{
// 	  /* Enable interrupts: */
// 	  recv_control |= RECV_INT_ENABLE;
// 	  if (recv_control & RECV_READY)
// 	    {
// 	      /* Raise interrupt on enabling a ready receiver */
// 	      RAISE_INTERRUPT (reg_image, RECV_INT_LEVEL);
// 	    }
// 	}
//       else
// 	{
// 	  /* Disable interrupts: */
// 	  recv_control &= ~RECV_INT_ENABLE;
// 	  CLEAR_INTERRUPT (reg_image, RECV_INT_LEVEL); /* Clear IP bit in Cause */
// 	}
//       break;
//
//     case RECV_BUFFER_ADDR:
//       /* Nop: program can't change buffer. */
//       break;
//
//     default:
//       run_error ("Write to unused memory-mapped IO address (0x%x)\n", addr);
//     }
// }
//
//
// /* Invoked on a read in the memory-mapped IO area. */
//
// static mem_word
// read_memory_mapped_IO (mem_addr addr)
// {
//   reg_image_t &reg_image = reg_images[0];
//
//   switch (addr)
//     {
//     case TRANS_CTRL_ADDR:
//       return (trans_control);
//
//     case TRANS_BUFFER_ADDR:
//       return (trans_buffer & 0xff);
//
//     case RECV_CTRL_ADDR:
//       return (recv_control);
//
//     case RECV_BUFFER_ADDR:
//       recv_control &= ~RECV_READY; /* Buffer now empty */
//       recv_buffer_full_timer = 0;
//       CLEAR_INTERRUPT (reg_image, RECV_INT_LEVEL); /* Clear IP bit in Cause */
//       return (recv_buffer & 0xff);
//
//     default:
//       run_error ("Read from unused memory-mapped IO address (0x%x)\n", addr);
//       return (0);
//     }
// }



/* Misc. routines */

void
print_mem (size_t context, mem_addr addr)
{
  mem_image_t &mem_image = mem_images[context];

  mem_word value;

  if ((addr & 0x3) != 0)
    addr &= ~0x3;		/* Address must be word-aligned */

  if (TEXT_BOT <= addr && addr < mem_image.text_top)
    print_inst (addr);
  else if (DATA_BOT <= addr && addr < mem_image.data_top)
    {
      value = read_mem_word (context, addr);
      write_output (message_out, "Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (mem_image.stack_bot <= addr && addr < STACK_TOP)
    {
      value = read_mem_word (context, addr);
      write_output (message_out, "Stack seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (K_TEXT_BOT <= addr && addr < mem_image.k_text_top)
    print_inst (addr);
  else if (K_DATA_BOT <= addr && addr < mem_image.k_data_top)
    {
      value = read_mem_word (context, addr);
      write_output (message_out,
		    "Kernel Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else
    error ("Address 0x%08x (%d) to print_mem is out of bounds\n", addr, addr);
}
