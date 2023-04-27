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
#include "image.h"
#include "reg.h"
#include "mem.h"

#include <optional>



/* Local functions: */

static mem_word bad_mem_read (MIPSImage &img, mem_addr addr, int mask);
static void bad_mem_write (MIPSImage &img, mem_addr addr, mem_word value, int mask);
static instruction *bad_text_read (MIPSImage &img, mem_addr addr);
static void bad_text_write (MIPSImage &img, mem_addr addr, instruction *inst);
static void free_instructions (instruction **inst, int n);
static mem_word read_memory_mapped_IO (MIPSImage &img, mem_addr addr);
static void write_memory_mapped_IO (MIPSImage &img, mem_addr addr, mem_word value);


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
make_memory (MIPSImage &img, int text_size, int data_size, int data_limit,
	     int stack_size, int stack_limit, int k_text_size,
	     int k_data_size, int k_data_limit)
{
  if (data_size <= 65536)
    data_size = 65536;
  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */

  if (img.mem_image().text_seg == NULL) {
    img.mem_image().text_seg = (instruction **) xmalloc (img, BYTES_TO_INST(text_size));
    img.mem_image().text_prof = (unsigned *) xmalloc(img, text_size);
  } else
    {
      free_instructions (img.mem_image().text_seg, (img.mem_image().text_top - TEXT_BOT) / BYTES_PER_WORD);
      img.mem_image().text_seg = (instruction **) realloc (img.mem_image().text_seg, BYTES_TO_INST(text_size));
      img.mem_image().text_prof = (unsigned *)realloc(img.mem_image().text_prof,text_size);
    }
  memclr (img.mem_image().text_seg, BYTES_TO_INST(text_size));
  memclr(img.mem_image().text_prof,text_size);
  img.mem_image().text_top = TEXT_BOT + text_size;

  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (img.mem_image().data_seg == NULL)
    img.mem_image().data_seg = (mem_word *) xmalloc (img, data_size);
  else
    img.mem_image().data_seg = (mem_word *) realloc (img.mem_image().data_seg, data_size);
  memclr (img.mem_image().data_seg, data_size);
  img.mem_image().data_seg_b = (BYTE_TYPE *) img.mem_image().data_seg;
  img.mem_image().data_seg_h = (short *) img.mem_image().data_seg;
  img.mem_image().data_top = DATA_BOT + data_size;
  data_size_limit = data_limit;

  stack_size = ROUND_UP(stack_size, BYTES_PER_WORD); /* Keep word aligned */
  if (img.mem_image().stack_seg == NULL)
    img.mem_image().stack_seg = (mem_word *) xmalloc (img, stack_size);
  else
    img.mem_image().stack_seg = (mem_word *) realloc (img.mem_image().stack_seg, stack_size);
  memclr (img.mem_image().stack_seg, stack_size);
  img.mem_image().stack_seg_b = (BYTE_TYPE *) img.mem_image().stack_seg;
  img.mem_image().stack_seg_h = (short *) img.mem_image().stack_seg;
  img.mem_image().stack_bot = STACK_TOP - stack_size;
  stack_size_limit = stack_limit;

  if (img.mem_image().special_seg == NULL) {
    img.mem_image().special_seg = (mem_word *) xmalloc (img, SPECIAL_TOP - SPECIAL_BOT);
         img.mem_image().special_seg_b = (BYTE_TYPE *) img.mem_image().special_seg;
         img.mem_image().special_seg_h = (short *) img.mem_image().special_seg;
  }
  memclr (img.mem_image().special_seg, (SPECIAL_TOP - SPECIAL_BOT));

  if (img.mem_image().k_text_seg == NULL) {
    img.mem_image().k_text_seg = (instruction **) xmalloc (img, BYTES_TO_INST(k_text_size));
    img.mem_image().k_text_prof = (unsigned *) xmalloc(img, k_text_size);
  } else
    {
      free_instructions (img.mem_image().k_text_seg,
			 (img.mem_image().k_text_top - K_TEXT_BOT) / BYTES_PER_WORD);
      img.mem_image().k_text_seg = (instruction **) realloc(img.mem_image().k_text_seg,
					    BYTES_TO_INST(k_text_size));
      img.mem_image().k_text_prof = (unsigned *) realloc(img.mem_image().k_text_prof, k_text_size);
    }
  memclr (img.mem_image().k_text_seg, BYTES_TO_INST(k_text_size));
  memclr (img.mem_image().k_text_prof, k_text_size);
  img.mem_image().k_text_top = K_TEXT_BOT + k_text_size;

  k_data_size = ROUND_UP(k_data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (img.mem_image().k_data_seg == NULL)
    img.mem_image().k_data_seg = (mem_word *) xmalloc (img, k_data_size);
  else
    img.mem_image().k_data_seg = (mem_word *) realloc (img.mem_image().k_data_seg, k_data_size);
  memclr (img.mem_image().k_data_seg, k_data_size);
  img.mem_image().k_data_seg_b = (BYTE_TYPE *) img.mem_image().k_data_seg;
  img.mem_image().k_data_seg_h = (short *) img.mem_image().k_data_seg;
  img.mem_image().k_data_top = K_DATA_BOT + k_data_size;
  k_data_size_limit = k_data_limit;

  img.mem_image().text_modified = true;
  img.mem_image().data_modified = true;
}


void mem_dump_profile(MIPSImage &img) {
  mem_image_t &mem_image = img.mem_image();

  str_stream ss;
  ss_init(&ss);
  FILE *file = NULL;

  // TODO: need to standardize this for multiple contexts
  if ((mem_image.prof_file_name == NULL) || (mem_image.prof_file_name[0] == 0))  {
    return;
  }
  file = fopen(mem_image.prof_file_name, "w");
  if (file == NULL) {
    printf("failed to open profile file: %s\n", mem_image.prof_file_name);
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
    format_an_inst(img, &ss, inst, addr);
    //print_inst_internal (&buf[10], sizeof(buf)-12, inst, addr);
    fprintf(file, "%s", ss_to_string(img, &ss));
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
    format_an_inst(img, &ss, inst, addr);
    //print_inst_internal (&buf[10], sizeof(buf)-12, inst, addr);
    fprintf(file, "%s", ss_to_string(img, &ss));
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
expand_data (MIPSImage &img, int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = img.mem_image().data_top - DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > data_size_limit))
    {
      error (img, "Can't expand data segment by %d bytes to %d bytes\n",
	     addl_bytes, new_size);
      run_error (img, "Use -ldata # with # > %d\n", new_size);
    }
  img.mem_image().data_seg = (mem_word *) realloc (img.mem_image().data_seg, new_size);
  if (img.mem_image().data_seg == NULL)
    fatal_error (img, "realloc failed in expand_data\n");

  img.mem_image().data_seg_b = (BYTE_TYPE *) img.mem_image().data_seg;
  img.mem_image().data_seg_h = (short *) img.mem_image().data_seg;
  img.mem_image().data_top += delta;

  /* Zero new memory */
  for (p = img.mem_image().data_seg_b + old_size; p < img.mem_image().data_seg_b + new_size; )
    *p ++ = 0;
}


/* Expand the stack segment by adding N bytes.  Can't use REALLOC
   since it copies from bottom of memory blocks and stack grows down from
   top of its block. */

void
expand_stack (MIPSImage &img, int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = STACK_TOP - img.mem_image().stack_bot;
  int new_size = old_size + MAX (delta, old_size);
  mem_word *new_seg;
  mem_word *po, *pn;

  if ((addl_bytes < 0) || (new_size > stack_size_limit))
    {
      run_error (img, "Can't expand stack segment by %d bytes to %d bytes.\nUse -lstack # with # > %d\n",
                 addl_bytes, new_size, new_size);
    }

  new_seg = (mem_word *) xmalloc (img, new_size);
  memset(new_seg, 0, new_size);

  po = img.mem_image().stack_seg + (old_size / BYTES_PER_WORD - 1);
  pn = new_seg + (new_size / BYTES_PER_WORD - 1);
  for ( ; po >= img.mem_image().stack_seg ; ) *pn -- = *po --;

  free (img.mem_image().stack_seg);
  img.mem_image().stack_seg = new_seg;
  img.mem_image().stack_seg_b = (BYTE_TYPE *) img.mem_image().stack_seg;
  img.mem_image().stack_seg_h = (short *) img.mem_image().stack_seg;
  img.mem_image().stack_bot -= (new_size - old_size);
}


/* Expand the kernel data segment by adding N bytes. */

void
expand_k_data (MIPSImage &img, int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = img.mem_image().k_data_top - K_DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > k_data_size_limit))
    {
      run_error (img, "Can't expand kernel data segment by %d bytes to %d bytes.\nUse -lkdata # with # > %d\n",
                 addl_bytes, new_size, new_size);
    }
  img.mem_image().k_data_seg = (mem_word *) realloc (img.mem_image().k_data_seg, new_size);
  if (img.mem_image().k_data_seg == NULL)
    fatal_error (img, "realloc failed in expand_k_data\n");

  img.mem_image().k_data_seg_b = (BYTE_TYPE *) img.mem_image().k_data_seg;
  img.mem_image().k_data_seg_h = (short *) img.mem_image().k_data_seg;
  img.mem_image().k_data_top += delta;

  /* Zero new memory */
  for (p = img.mem_image().k_data_seg_b + old_size / BYTES_PER_WORD;
       p < img.mem_image().k_data_seg_b + new_size / BYTES_PER_WORD; )
    *p ++ = 0;
}



/* Access memory */

void*
mem_reference(MIPSImage &img, mem_addr addr)
{
  if ((addr >= TEXT_BOT) && (addr < img.mem_image().text_top))
    return addr - TEXT_BOT + (char*) img.mem_image().text_seg;
  else if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top))
    return addr - DATA_BOT + (char*) img.mem_image().data_seg;
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP))
    return addr - img.mem_image().stack_bot + (char*) img.mem_image().stack_seg;
  else if ((addr >= K_TEXT_BOT) && (addr < img.mem_image().k_text_top))
    return addr - K_TEXT_BOT + (char*) img.mem_image().k_text_seg;
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top))
    return addr - K_DATA_BOT + (char*) img.mem_image().k_data_seg;
  else
    {
      run_error (img, "Memory address out of bounds\n");
      return NULL;
    }
}


instruction*
read_mem_inst(MIPSImage &img, mem_addr addr)
{
  if ((addr >= TEXT_BOT) && (addr < img.mem_image().text_top) && !(addr & 0x3)) {
    ++ img.mem_image().text_prof[(addr - TEXT_BOT) >> 2];
    return img.mem_image().text_seg [(addr - TEXT_BOT) >> 2];
  } else if ((addr >= K_TEXT_BOT) && (addr < img.mem_image().k_text_top) && !(addr & 0x3)) {
    ++ img.mem_image().k_text_prof[(addr - K_TEXT_BOT) >> 2];
    return img.mem_image().k_text_seg [(addr - K_TEXT_BOT) >> 2];
  } else {
    return bad_text_read (img, addr);
  }
}


reg_word
read_mem_byte(MIPSImage &img, mem_addr addr)
{
  std::optional<reg_word> custom_read = img.custom_memory_read_byte(addr);
  if (custom_read.has_value())
    return custom_read.value();

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top))
    return img.mem_image().data_seg_b [addr - DATA_BOT];
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP))
    return img.mem_image().stack_seg_b [addr - img.mem_image().stack_bot];
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top))
    return img.mem_image().k_data_seg_b [addr - K_DATA_BOT];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP))
    return img.mem_image().special_seg_b [addr - SPECIAL_BOT];
  else
    return bad_mem_read (img, addr, 0);
}


reg_word
read_mem_half(MIPSImage &img, mem_addr addr)
{
  std::optional<reg_word> custom_read = img.custom_memory_read_half(addr);
  if (custom_read.has_value())
    return custom_read.value();

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top) && !(addr & 0x1))
    return img.mem_image().data_seg_h [(addr - DATA_BOT) >> 1];
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    return img.mem_image().stack_seg_h [(addr - img.mem_image().stack_bot) >> 1];
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top) && !(addr & 0x1))
    return img.mem_image().k_data_seg_h [(addr - K_DATA_BOT) >> 1];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x1))
    return img.mem_image().special_seg_h [(addr - SPECIAL_BOT) >> 1];
  else
    return bad_mem_read (img, addr, 0x1);
}


reg_word
read_mem_word(MIPSImage &img, mem_addr addr)
{
  std::optional<reg_word> custom_read = img.custom_memory_read_word(addr);
  if (custom_read.has_value())
    return custom_read.value();

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top) && !(addr & 0x3))
    return img.mem_image().data_seg [(addr - DATA_BOT) >> 2];
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    return img.mem_image().stack_seg [(addr - img.mem_image().stack_bot) >> 2];
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top) && !(addr & 0x3))
    return img.mem_image().k_data_seg [(addr - K_DATA_BOT) >> 2];
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x3))
    return img.mem_image().special_seg [(addr - SPECIAL_BOT) >> 2];
  else
    return bad_mem_read (img, addr, 0x3);
}


void
set_mem_inst(MIPSImage &img, mem_addr addr, instruction* inst)
{
  img.mem_image().text_modified = true;
  if ((addr >= TEXT_BOT) && (addr < img.mem_image().text_top) && !(addr & 0x3))
    img.mem_image().text_seg [(addr - TEXT_BOT) >> 2] = inst;
  else if ((addr >= K_TEXT_BOT) && (addr < img.mem_image().k_text_top) && !(addr & 0x3))
    img.mem_image().k_text_seg [(addr - K_TEXT_BOT) >> 2] = inst;
  else
    bad_text_write (img, addr, inst);
}


void
set_mem_byte(MIPSImage &img, mem_addr addr, reg_word value)
{
  img.mem_image().data_modified = true;
  if (img.custom_memory_write_byte(addr, value))
    return;

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top))
    img.mem_image().data_seg_b [addr - DATA_BOT] = (BYTE_TYPE) value;
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP))
    img.mem_image().stack_seg_b [addr - img.mem_image().stack_bot] = (BYTE_TYPE) value;
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top))
    img.mem_image().k_data_seg_b [addr - K_DATA_BOT] = (BYTE_TYPE) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP))
    img.mem_image().special_seg [addr - SPECIAL_BOT] = (BYTE_TYPE) value;
  else
    bad_mem_write (img, addr, value, 0);
}


void
set_mem_half(MIPSImage &img, mem_addr addr, reg_word value)
{
  img.mem_image().data_modified = true;
  if (img.custom_memory_write_half(addr, value))
    return;

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top) && !(addr & 0x1))
    img.mem_image().data_seg_h [(addr - DATA_BOT) >> 1] = (short) value;
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    img.mem_image().stack_seg_h [(addr - img.mem_image().stack_bot) >> 1] = (short) value;
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top) && !(addr & 0x1))
    img.mem_image().k_data_seg_h [(addr - K_DATA_BOT) >> 1] = (short) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x1))
    img.mem_image().special_seg_h [(addr - SPECIAL_BOT) >> 1] = (short) value;
  else
    bad_mem_write (img, addr, value, 0x1);
}


void
set_mem_word(MIPSImage &img, mem_addr addr, reg_word value)
{
  img.mem_image().data_modified = true;
  if (img.custom_memory_write_word(addr, value))
    return;

  if ((addr >= DATA_BOT) && (addr < img.mem_image().data_top) && !(addr & 0x3))
    img.mem_image().data_seg [(addr - DATA_BOT) >> 2] = (mem_word) value;
  else if ((addr >= img.mem_image().stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    img.mem_image().stack_seg [(addr - img.mem_image().stack_bot) >> 2] = (mem_word) value;
  else if ((addr >= K_DATA_BOT) && (addr < img.mem_image().k_data_top) && !(addr & 0x3))
    img.mem_image().k_data_seg [(addr - K_DATA_BOT) >> 2] = (mem_word) value;
  else if ((addr >= SPECIAL_BOT) && (addr < SPECIAL_TOP) && !(addr & 0x3))
    img.mem_image().special_seg [(addr - SPECIAL_BOT) >> 2] = (mem_word) value;
  else
    bad_mem_write (img, addr, value, 0x3);
}


/* Handle the infrequent and erroneous cases in memory accesses. */

static instruction *
bad_text_read (MIPSImage &img, mem_addr addr)
{
  RAISE_EXCEPTION (img, ExcCode_IBE, img.reg_image().CP0_BadVAddr = addr);
  return (inst_decode (img, 0));
}


static void
bad_text_write (MIPSImage &img, mem_addr addr, instruction *inst)
{
  RAISE_EXCEPTION (img, ExcCode_IBE, img.reg_image().CP0_BadVAddr = addr);
  set_mem_word (img, addr, ENCODING (inst));
}


static mem_word
bad_mem_read (MIPSImage &img, mem_addr addr, int mask)
{
  mem_word tmp;

  if ((addr & mask) != 0)
    RAISE_EXCEPTION (img, ExcCode_AdEL, img.reg_image().CP0_BadVAddr = addr)
  else if (addr >= TEXT_BOT && addr < img.mem_image().text_top)
    switch (mask)
      {
      case 0x0:
	tmp = ENCODING (img.mem_image().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (3 - (addr & 0x3)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x3));
#endif
	return (0xff & tmp);

      case 0x1:
	tmp = ENCODING (img.mem_image().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (2 - (addr & 0x2)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x2));
#endif
	return (0xffff & tmp);

      case 0x3:
	{
	instruction *inst = img.mem_image().text_seg [(addr - TEXT_BOT) >> 2];
	if (inst == NULL)
	  return 0;
	else
	  return (ENCODING (inst));
	}

      default:
	run_error (img, "Bad mask (0x%x) in bad_mem_read\n", mask);
      }
  else if (addr > img.mem_image().data_top
	   && addr < img.mem_image().stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > img.mem_image().stack_bot - 16*K*K)
    {
      /* Grow stack segment */
      expand_stack (img, img.mem_image().stack_bot - addr + 4);
      return (0);
    }
  else if (MM_IO_BOT <= addr && addr <= MM_IO_TOP)
    return (read_memory_mapped_IO (img, addr));
  else
    /* Address out of range */
    RAISE_EXCEPTION (img, ExcCode_DBE, img.reg_image().CP0_BadVAddr = addr)
  return (0);
}


static void
bad_mem_write (MIPSImage &img, mem_addr addr, mem_word value, int mask)
{
  mem_word tmp;

  if ((addr & mask) != 0)
    /* Unaligned address fault */
    RAISE_EXCEPTION (img, ExcCode_AdES, img.reg_image().CP0_BadVAddr = addr)
    else if (addr >= TEXT_BOT && addr < img.mem_image().text_top)
  {
    switch (mask)
    {
    case 0x0:
      tmp = ENCODING (img.mem_image().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
      tmp = ((tmp & ~(0xff << (8 * (3 - (addr & 0x3)))))
	       | (value & 0xff) << (8 * (3 - (addr & 0x3))));
#else
      tmp = ((tmp & ~(0xff << (8 * (addr & 0x3))))
	       | (value & 0xff) << (8 * (addr & 0x3)));
#endif
      break;

    case 0x1:
      tmp = ENCODING (img.mem_image().text_seg [(addr - TEXT_BOT) >> 2]);
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
      run_error (img, "Bad mask (0x%x) in bad_mem_read\n", mask);
    }

    if (img.mem_image().text_seg [(addr - TEXT_BOT) >> 2] != NULL)
    {
      free_inst (img.mem_image().text_seg[(addr - TEXT_BOT) >> 2]);
    }
    img.mem_image().text_seg [(addr - TEXT_BOT) >> 2] = inst_decode (img, tmp);

    img.mem_image().text_modified = true;
  }
  else if (addr > img.mem_image().data_top
	   && addr < img.mem_image().stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > img.mem_image().stack_bot - 16*K*K)
  {
    /* Grow stack segment */
    expand_stack (img, img.mem_image().stack_bot - addr + 4);
    if (addr >= img.mem_image().stack_bot)
    {
      if (mask == 0)
	img.mem_image().stack_seg_b [addr - img.mem_image().stack_bot] = (char)value;
      else if (mask == 1)
	img.mem_image().stack_seg_h [(addr - img.mem_image().stack_bot) >> 1] = (short)value;
      else
	img.mem_image().stack_seg [(addr - img.mem_image().stack_bot) >> 2] = value;
    }
    else
      RAISE_EXCEPTION (img, ExcCode_DBE, img.reg_image().CP0_BadVAddr = addr)

    img.mem_image().data_modified = true;
  }
  else if (MM_IO_BOT <= addr && addr <= MM_IO_TOP)
    write_memory_mapped_IO (img, addr, value);
  else
    /* Address out of range */
    RAISE_EXCEPTION (img, ExcCode_DBE, img.reg_image().CP0_BadVAddr = addr)
}




/* Check if input is available and output is possible.  If so, update the
   memory-mapped control registers and buffers. */

void
check_memory_mapped_IO ()
{
}


/* Invoked on a write to the memory-mapped IO area. */

static void
write_memory_mapped_IO (MIPSImage &img, mem_addr addr, mem_word)
{
  // TODO: Check the blame again
  void *todo;
  switch (addr) {
    default:
      run_error (img, "Write to unused memory-mapped IO address (0x%x)\n", addr);
  }
}


/* Invoked on a read in the memory-mapped IO area. */

static mem_word
read_memory_mapped_IO (MIPSImage &img, mem_addr addr)
{
  switch (addr) {
    default:
      run_error (img, "Read from unused memory-mapped IO address (0x%x)\n", addr);
    
    return (0);
  }
}



/* Misc. routines */

void
print_mem (MIPSImage &img, mem_addr addr)
{
  mem_word value;

  if ((addr & 0x3) != 0)
    addr &= ~0x3;		/* Address must be word-aligned */

  if (TEXT_BOT <= addr && addr < img.mem_image().text_top)
    print_inst (img, addr);
  else if (DATA_BOT <= addr && addr < img.mem_image().data_top)
    {
      value = read_mem_word (img, addr);
      write_output (img, message_out, "Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (img.mem_image().stack_bot <= addr && addr < STACK_TOP)
    {
      value = read_mem_word (img, addr);
      write_output (img, message_out, "Stack seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (K_TEXT_BOT <= addr && addr < img.mem_image().k_text_top)
    print_inst (img, addr);
  else if (K_DATA_BOT <= addr && addr < img.mem_image().k_data_top)
    {
      value = read_mem_word (img, addr);
      write_output (img, message_out,
		    "Kernel Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else
    error (img, "Address 0x%08x (%d) to print_mem is out of bounds\n", addr, addr);
}
