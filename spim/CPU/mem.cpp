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





/* Local functions: */

static mem_word bad_mem_read (mem_addr addr, int mask);
static void bad_mem_write (mem_addr addr, mem_word value, int mask);
static instruction *bad_text_read (mem_addr addr);
static void bad_text_write (mem_addr addr, instruction *inst);
static void free_instructions (instruction **inst, int n);
static mem_word read_memory_mapped_IO (mem_addr addr);
static void write_memory_mapped_IO (mem_addr addr, mem_word value);


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
make_memory (int text_size, int data_size, int data_limit,
	     int stack_size, int stack_limit, int k_text_size,
	     int k_data_size, int k_data_limit)
{
  if (data_size <= 65536)
    data_size = 65536;
  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */

  if (mem().text_seg == NULL)
    mem().text_seg = (instruction **) xmalloc (BYTES_TO_INST(text_size));
  else
    {
      free_instructions (mem().text_seg, (mem().text_top - TEXT_BOT) / BYTES_PER_WORD);
      mem().text_seg = (instruction **) realloc (mem().text_seg, BYTES_TO_INST(text_size));
    }
  memclr (mem().text_seg, BYTES_TO_INST(text_size));
  mem().text_top = TEXT_BOT + text_size;

  data_size = ROUND_UP(data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem().data_seg == NULL)
    mem().data_seg = (mem_word *) xmalloc (data_size);
  else
    mem().data_seg = (mem_word *) realloc (mem().data_seg, data_size);
  memclr (mem().data_seg, data_size);
  mem().data_seg_b = (BYTE_TYPE *) mem().data_seg;
  mem().data_seg_h = (short *) mem().data_seg;
  mem().data_top = DATA_BOT + data_size;
  data_size_limit = data_limit;

  stack_size = ROUND_UP(stack_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem().stack_seg == NULL)
    mem().stack_seg = (mem_word *) xmalloc (stack_size);
  else
    mem().stack_seg = (mem_word *) realloc (mem().stack_seg, stack_size);
  memclr (mem().stack_seg, stack_size);
  mem().stack_seg_b = (BYTE_TYPE *) mem().stack_seg;
  mem().stack_seg_h = (short *) mem().stack_seg;
  mem().stack_bot = STACK_TOP - stack_size;
  stack_size_limit = stack_limit;

  if (mem().k_text_seg == NULL)
    mem().k_text_seg = (instruction **) xmalloc (BYTES_TO_INST(k_text_size));
  else
    {
      free_instructions (mem().k_text_seg,
			 (mem().k_text_top - K_TEXT_BOT) / BYTES_PER_WORD);
      mem().k_text_seg = (instruction **) realloc(mem().k_text_seg,
					    BYTES_TO_INST(k_text_size));
    }
  memclr (mem().k_text_seg, BYTES_TO_INST(k_text_size));
  mem().k_text_top = K_TEXT_BOT + k_text_size;

  k_data_size = ROUND_UP(k_data_size, BYTES_PER_WORD); /* Keep word aligned */
  if (mem().k_data_seg == NULL)
    mem().k_data_seg = (mem_word *) xmalloc (k_data_size);
  else
    mem().k_data_seg = (mem_word *) realloc (mem().k_data_seg, k_data_size);
  memclr (mem().k_data_seg, k_data_size);
  mem().k_data_seg_b = (BYTE_TYPE *) mem().k_data_seg;
  mem().k_data_seg_h = (short *) mem().k_data_seg;
  mem().k_data_top = K_DATA_BOT + k_data_size;
  k_data_size_limit = k_data_limit;

  mem().text_modified = true;
  mem().data_modified = true;
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
expand_data (int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = mem().data_top - DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > data_size_limit))
    {
      error ("Can't expand data segment by %d bytes to %d bytes\n",
	     addl_bytes, new_size);
      run_error ("Use -ldata # with # > %d\n", new_size);
    }
  mem().data_seg = (mem_word *) realloc (mem().data_seg, new_size);
  if (mem().data_seg == NULL)
    fatal_error ("realloc failed in expand_data\n");

  mem().data_seg_b = (BYTE_TYPE *) mem().data_seg;
  mem().data_seg_h = (short *) mem().data_seg;
  mem().data_top += delta;

  /* Zero new memory */
  for (p = mem().data_seg_b + old_size; p < mem().data_seg_b + new_size; )
    *p ++ = 0;
}


/* Expand the stack segment by adding N bytes.  Can't use REALLOC
   since it copies from bottom of memory blocks and stack grows down from
   top of its block. */

void
expand_stack (int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = STACK_TOP - mem().stack_bot;
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

  po = mem().stack_seg + (old_size / BYTES_PER_WORD - 1);
  pn = new_seg + (new_size / BYTES_PER_WORD - 1);
  for ( ; po >= mem().stack_seg ; ) *pn -- = *po --;

  free (mem().stack_seg);
  mem().stack_seg = new_seg;
  mem().stack_seg_b = (BYTE_TYPE *) mem().stack_seg;
  mem().stack_seg_h = (short *) mem().stack_seg;
  mem().stack_bot -= (new_size - old_size);
}


/* Expand the kernel data segment by adding N bytes. */

void
expand_k_data (int addl_bytes)
{
  int delta = ROUND_UP(addl_bytes, BYTES_PER_WORD); /* Keep word aligned */
  int old_size = mem().k_data_top - K_DATA_BOT;
  int new_size = old_size + delta;
  BYTE_TYPE *p;

  if ((addl_bytes < 0) || (new_size > k_data_size_limit))
    {
      run_error ("Can't expand kernel data segment by %d bytes to %d bytes.\nUse -lkdata # with # > %d\n",
                 addl_bytes, new_size, new_size);
    }
  mem().k_data_seg = (mem_word *) realloc (mem().k_data_seg, new_size);
  if (mem().k_data_seg == NULL)
    fatal_error ("realloc failed in expand_k_data\n");

  mem().k_data_seg_b = (BYTE_TYPE *) mem().k_data_seg;
  mem().k_data_seg_h = (short *) mem().k_data_seg;
  mem().k_data_top += delta;

  /* Zero new memory */
  for (p = mem().k_data_seg_b + old_size / BYTES_PER_WORD;
       p < mem().k_data_seg_b + new_size / BYTES_PER_WORD; )
    *p ++ = 0;
}



/* Access memory */

void*
mem_reference(mem_addr addr)
{
  if ((addr >= TEXT_BOT) && (addr < mem().text_top))
    return addr - TEXT_BOT + (char*) mem().text_seg;
  else if ((addr >= DATA_BOT) && (addr < mem().data_top))
    return addr - DATA_BOT + (char*) mem().data_seg;
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP))
    return addr - mem().stack_bot + (char*) mem().stack_seg;
  else if ((addr >= K_TEXT_BOT) && (addr < mem().k_text_top))
    return addr - K_TEXT_BOT + (char*) mem().k_text_seg;
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top))
    return addr - K_DATA_BOT + (char*) mem().k_data_seg;
  else
    {
      run_error ("Memory address out of bounds\n");
      return NULL;
    }
}


instruction*
read_mem_inst(mem_addr addr)
{
  if ((addr >= TEXT_BOT) && (addr < mem().text_top) && !(addr & 0x3))
    return mem().text_seg [(addr - TEXT_BOT) >> 2];
  else if ((addr >= K_TEXT_BOT) && (addr < mem().k_text_top) && !(addr & 0x3))
    return mem().k_text_seg [(addr - K_TEXT_BOT) >> 2];
  else
    return bad_text_read (addr);
}


reg_word
read_mem_byte(mem_addr addr)
{
  if ((addr >= DATA_BOT) && (addr < mem().data_top))
    return mem().data_seg_b [addr - DATA_BOT];
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP))
    return mem().stack_seg_b [addr - mem().stack_bot];
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top))
    return mem().k_data_seg_b [addr - K_DATA_BOT];
  else
    return bad_mem_read (addr, 0);
}


reg_word
read_mem_half(mem_addr addr)
{
  if ((addr >= DATA_BOT) && (addr < mem().data_top) && !(addr & 0x1))
    return mem().data_seg_h [(addr - DATA_BOT) >> 1];
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    return mem().stack_seg_h [(addr - mem().stack_bot) >> 1];
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top) && !(addr & 0x1))
    return mem().k_data_seg_h [(addr - K_DATA_BOT) >> 1];
  else
    return bad_mem_read (addr, 0x1);
}


reg_word
read_mem_word(mem_addr addr)
{
  if ((addr >= DATA_BOT) && (addr < mem().data_top) && !(addr & 0x3))
    return mem().data_seg [(addr - DATA_BOT) >> 2];
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    return mem().stack_seg [(addr - mem().stack_bot) >> 2];
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top) && !(addr & 0x3))
    return mem().k_data_seg [(addr - K_DATA_BOT) >> 2];
  else
    return bad_mem_read (addr, 0x3);
}


void
set_mem_inst(mem_addr addr, instruction* inst)
{
  mem().text_modified = true;
  if ((addr >= TEXT_BOT) && (addr < mem().text_top) && !(addr & 0x3))
    mem().text_seg [(addr - TEXT_BOT) >> 2] = inst;
  else if ((addr >= K_TEXT_BOT) && (addr < mem().k_text_top) && !(addr & 0x3))
    mem().k_text_seg [(addr - K_TEXT_BOT) >> 2] = inst;
  else
    bad_text_write (addr, inst);
}


void
set_mem_byte(mem_addr addr, reg_word value)
{
  mem().data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem().data_top))
    mem().data_seg_b [addr - DATA_BOT] = (BYTE_TYPE) value;
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP))
    mem().stack_seg_b [addr - mem().stack_bot] = (BYTE_TYPE) value;
  else if ((addr >= K_DATA_BOT) && (addr <mem(). k_data_top))
    mem().k_data_seg_b [addr - K_DATA_BOT] = (BYTE_TYPE) value;
  else
    bad_mem_write (addr, value, 0);
}


void
set_mem_half(mem_addr addr, reg_word value)
{
  mem().data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem().data_top) && !(addr & 0x1))
    mem().data_seg_h [(addr - DATA_BOT) >> 1] = (short) value;
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP) && !(addr & 0x1))
    mem().stack_seg_h [(addr - mem().stack_bot) >> 1] = (short) value;
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top) && !(addr & 0x1))
    mem().k_data_seg_h [(addr - K_DATA_BOT) >> 1] = (short) value;
  else
    bad_mem_write (addr, value, 0x1);
}


void
set_mem_word(mem_addr addr, reg_word value)
{
  mem().data_modified = true;
  if ((addr >= DATA_BOT) && (addr < mem().data_top) && !(addr & 0x3))
    mem().data_seg [(addr - DATA_BOT) >> 2] = (mem_word) value;
  else if ((addr >= mem().stack_bot) && (addr < STACK_TOP) && !(addr & 0x3))
    mem().stack_seg [(addr - mem().stack_bot) >> 2] = (mem_word) value;
  else if ((addr >= K_DATA_BOT) && (addr < mem().k_data_top) && !(addr & 0x3))
    mem().k_data_seg [(addr - K_DATA_BOT) >> 2] = (mem_word) value;
  else
    bad_mem_write (addr, value, 0x3);
}


/* Handle the infrequent and erroneous cases in memory accesses. */

static instruction *
bad_text_read (mem_addr addr)
{
  RAISE_EXCEPTION (ExcCode_IBE, reg().CP0_BadVAddr = addr);
  return (inst_decode (0));
}


static void
bad_text_write (mem_addr addr, instruction *inst)
{
  RAISE_EXCEPTION (ExcCode_IBE, reg().CP0_BadVAddr = addr);
  set_mem_word (addr, ENCODING (inst));
}


static mem_word
bad_mem_read (mem_addr addr, int mask)
{
  mem_word tmp;

  if ((addr & mask) != 0)
    RAISE_EXCEPTION (ExcCode_AdEL, reg().CP0_BadVAddr = addr)
  else if (addr >= TEXT_BOT && addr < mem().text_top)
    switch (mask)
      {
      case 0x0:
	tmp = ENCODING (mem().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (3 - (addr & 0x3)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x3));
#endif
	return (0xff & tmp);

      case 0x1:
	tmp = ENCODING (mem().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
	tmp = (unsigned)tmp >> (8 * (2 - (addr & 0x2)));
#else
	tmp = (unsigned)tmp >> (8 * (addr & 0x2));
#endif
	return (0xffff & tmp);

      case 0x3:
	{
	instruction *inst = mem().text_seg [(addr - TEXT_BOT) >> 2];
	if (inst == NULL)
	  return 0;
	else
	  return (ENCODING (inst));
	}

      default:
	run_error ("Bad mask (0x%x) in bad_mem_read\n", mask);
      }
  else if (addr > mem().data_top
	   && addr < mem().stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > mem().stack_bot - 16*K*K)
    {
      /* Grow stack segment */
      expand_stack (mem().stack_bot - addr + 4);
      return (0);
    }
  else if (MM_IO_BOT <= addr && addr <= MM_IO_TOP)
    return (read_memory_mapped_IO (addr));
  else
    /* Address out of range */
    RAISE_EXCEPTION (ExcCode_DBE, reg().CP0_BadVAddr = addr)
  return (0);
}


static void
bad_mem_write (mem_addr addr, mem_word value, int mask)
{
  mem_word tmp;

  if ((addr & mask) != 0)
    /* Unaligned address fault */
    RAISE_EXCEPTION (ExcCode_AdES, reg().CP0_BadVAddr = addr)
    else if (addr >= TEXT_BOT && addr < mem().text_top)
  {
    switch (mask)
    {
    case 0x0:
      tmp = ENCODING (mem().text_seg [(addr - TEXT_BOT) >> 2]);
#ifdef SPIM_BIGENDIAN
      tmp = ((tmp & ~(0xff << (8 * (3 - (addr & 0x3)))))
	       | (value & 0xff) << (8 * (3 - (addr & 0x3))));
#else
      tmp = ((tmp & ~(0xff << (8 * (addr & 0x3))))
	       | (value & 0xff) << (8 * (addr & 0x3)));
#endif
      break;

    case 0x1:
      tmp = ENCODING (mem().text_seg [(addr - TEXT_BOT) >> 2]);
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

    if (mem().text_seg [(addr - TEXT_BOT) >> 2] != NULL)
    {
      free_inst (mem().text_seg[(addr - TEXT_BOT) >> 2]);
    }
    mem().text_seg [(addr - TEXT_BOT) >> 2] = inst_decode (tmp);

    mem().text_modified = true;
  }
  else if (addr > mem().data_top
	   && addr < mem().stack_bot
	   /* If more than 16 MB below stack, probably is bad data ref */
	   && addr > mem().stack_bot - 16*K*K)
  {
    /* Grow stack segment */
    expand_stack (mem().stack_bot - addr + 4);
    if (addr >= mem().stack_bot)
    {
      if (mask == 0)
	mem().stack_seg_b [addr - mem().stack_bot] = (char)value;
      else if (mask == 1)
	mem().stack_seg_h [(addr - mem().stack_bot) >> 1] = (short)value;
      else
	mem().stack_seg [(addr - mem().stack_bot) >> 2] = value;
    }
    else
      RAISE_EXCEPTION (ExcCode_DBE, reg().CP0_BadVAddr = addr)

    mem().data_modified = true;
  }
  else if (MM_IO_BOT <= addr && addr <= MM_IO_TOP)
    write_memory_mapped_IO (addr, value);
  else
    /* Address out of range */
    RAISE_EXCEPTION (ExcCode_DBE, reg().CP0_BadVAddr = addr)
}



/* Memory-mapped IO routines. */

static int recv_control = 0;	/* No input */
static int recv_buffer;
static int recv_buffer_full_timer = 0;

static int trans_control = TRANS_READY;	/* Ready to write */
static int trans_buffer;
static int trans_buffer_full_timer = 0;


/* Check if input is available and output is possible.  If so, update the
   memory-mapped control registers and buffers. */

void
check_memory_mapped_IO ()
{
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
	  RAISE_INTERRUPT (RECV_INT_LEVEL);
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
	  RAISE_INTERRUPT (TRANS_INT_LEVEL);
	}
    }
}


/* Invoked on a write to the memory-mapped IO area. */

static void
write_memory_mapped_IO (mem_addr addr, mem_word value)
{
  switch (addr)
    {
    case TRANS_CTRL_ADDR:
      /* Program can only set the interrupt enable, not ready, bit. */
      if ((value & TRANS_INT_ENABLE) != 0)
	{
	  /* Enable interrupts: */
	  trans_control |= TRANS_INT_ENABLE;
	  if (trans_control & TRANS_READY)
	    {
	      /* Raise interrupt on enabling a ready transmitter */
	      RAISE_INTERRUPT (TRANS_INT_LEVEL);
	    }
	}
      else
	{
	  /* Disable interrupts: */
	  trans_control &= ~TRANS_INT_ENABLE;
	  CLEAR_INTERRUPT (TRANS_INT_LEVEL); /* Clear IP bit in Cause */
	}
      break;

    case TRANS_BUFFER_ADDR:
      /* Ignore write if device is not ready. */
      if ((trans_control & TRANS_READY) != 0)
	{
	  /* Write char: */
	  trans_buffer = value & 0xff;
	  put_console_char ((char)trans_buffer);
	  /* Device is busy for a while: */
	  trans_control &= ~TRANS_READY;
	  trans_buffer_full_timer = TRANS_LATENCY;
          CLEAR_INTERRUPT (TRANS_INT_LEVEL); /* Clear IP bit in Cause */
	}
      break;

    case RECV_CTRL_ADDR:
      /* Program can only set the interrupt enable, not ready, bit. */
      if ((value & RECV_INT_ENABLE) != 0)
	{
	  /* Enable interrupts: */
	  recv_control |= RECV_INT_ENABLE;
	  if (recv_control & RECV_READY)
	    {
	      /* Raise interrupt on enabling a ready receiver */
	      RAISE_INTERRUPT (RECV_INT_LEVEL);
	    }
	}
      else
	{
	  /* Disable interrupts: */
	  recv_control &= ~RECV_INT_ENABLE;
	  CLEAR_INTERRUPT (RECV_INT_LEVEL); /* Clear IP bit in Cause */
	}
      break;

    case RECV_BUFFER_ADDR:
      /* Nop: program can't change buffer. */
      break;

    default:
      run_error ("Write to unused memory-mapped IO address (0x%x)\n", addr);
    }
}


/* Invoked on a read in the memory-mapped IO area. */

static mem_word
read_memory_mapped_IO (mem_addr addr)
{
  switch (addr)
    {
    case TRANS_CTRL_ADDR:
      return (trans_control);

    case TRANS_BUFFER_ADDR:
      return (trans_buffer & 0xff);

    case RECV_CTRL_ADDR:
      return (recv_control);

    case RECV_BUFFER_ADDR:
      recv_control &= ~RECV_READY; /* Buffer now empty */
      recv_buffer_full_timer = 0;
      CLEAR_INTERRUPT (RECV_INT_LEVEL); /* Clear IP bit in Cause */
      return (recv_buffer & 0xff);

    default:
      run_error ("Read from unused memory-mapped IO address (0x%x)\n", addr);
      return (0);
    }
}



/* Misc. routines */

void
print_mem (mem_addr addr)
{
  mem_word value;

  if ((addr & 0x3) != 0)
    addr &= ~0x3;		/* Address must be word-aligned */

  if (TEXT_BOT <= addr && addr < mem().text_top)
    print_inst (addr);
  else if (DATA_BOT <= addr && addr < mem().data_top)
    {
      value = read_mem_word (addr);
      write_output (message_out, "Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (mem().stack_bot <= addr && addr < STACK_TOP)
    {
      value = read_mem_word (addr);
      write_output (message_out, "Stack seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else if (K_TEXT_BOT <= addr && addr < mem().k_text_top)
    print_inst (addr);
  else if (K_DATA_BOT <= addr && addr < mem().k_data_top)
    {
      value = read_mem_word (addr);
      write_output (message_out,
		    "Kernel Data seg @ 0x%08x (%d) = 0x%08x (%d)\n",
		    addr, addr, value, value);
    }
  else
    error ("Address 0x%08x (%d) to print_mem is out of bounds\n", addr, addr);
}
