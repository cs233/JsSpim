/* SPIM S20 MIPS simulator.
   Misc. routines for SPIM.

   Copyright (c) 1990-2015, James R. Larus.
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


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "spim.h"
#include "version.h"
#include "string-stream.h"
#include "spim-utils.h"
#include "inst.h"
#include "data.h"
#include "reg.h"
#include "mem.h"
#include "scanner.h"
#include "parser.h"
#include "parser_yacc.h"
#include "run.h"
#include "sym-tbl.h"

#include "../Spimbot/robot.h"

bkpt *bkpts = NULL;

size_t num_contexts = 1;
size_t current_image = 0;
void set_current_image(size_t i) { current_image = i; }

/* Internal functions: */

static mem_addr copy_int_to_stack (size_t context, int n);
static mem_addr copy_str_to_stack (size_t context, char *s);
static void delete_all_breakpoints ();


int exception_occurred;

int initial_text_size = TEXT_SIZE;

int initial_data_size = DATA_SIZE;

mem_addr initial_data_limit = DATA_LIMIT;

int initial_stack_size = STACK_SIZE;

mem_addr initial_stack_limit = STACK_LIMIT;

int initial_k_text_size = K_TEXT_SIZE;

int initial_k_data_size = K_DATA_SIZE;

mem_addr initial_k_data_limit = K_DATA_LIMIT;



/* Initialize or reinitialize the state of the machine. */

void
initialize_world (size_t context, char* exception_file_paths,
                  char *exception_file_name, bool free_labels)
{
  reg_image_t &reg_image = reg_images[context];
  reg_image.context = context;
  reg_image.auto_alignment = 1;

  /* Allocate the floating point registers */
  if (reg_image.FGR == NULL)
    reg_image.FPR = (double *) xmalloc (FPR_LENGTH * sizeof (double));
  /* Allocate the memory */
  make_memory (context,
	       initial_text_size,
	       initial_data_size, initial_data_limit,
	       initial_stack_size, initial_stack_limit,
	       initial_k_text_size,
	       initial_k_data_size, initial_k_data_limit);
  initialize_registers (context);
  initialize_inst_tables ();
  initialize_symbol_table (free_labels);
  k_text_begins_at_point (context, K_TEXT_BOT);
  k_data_begins_at_point (context, K_DATA_BOT);
  data_begins_at_point (context, DATA_BOT);
  text_begins_at_point (context, TEXT_BOT);

  if (exception_file_paths != NULL)
    {
      bool old_bare = bare_machine;
      bool old_accept = accept_pseudo_insts;
      char *filename;
      char *files;

      /* Save machine state */
      bare_machine = false;     /* Exception handler uses extended machine */
      accept_pseudo_insts = true;

      /* strtok modifies the string, so we must back up the string prior to use. */
      if ((files = strdup (exception_file_paths)) == NULL)
         fatal_error ("Insufficient memory to complete.\n");

      for (filename = strtok (files, ";"); filename != NULL; filename = strtok (NULL, ";"))
         {
            /* if you pass in non-NULL second parameter, I assume one exception file */
            if (!read_assembly_file (filename, exception_file_name))
               fatal_error ("Cannot read exception handler: %s\n", filename);

            write_output (message_out, "Loaded: %s\n", filename);
         }

      free (files);

      /* Restore machine state */
      bare_machine = old_bare;
      accept_pseudo_insts = old_accept;

      if (!bare_machine)
      {
	(void)make_label_global ("main"); /* In case .globl main forgotten */
	(void)record_label ("main", 0, 0);
      }
    }
  initialize_scanner (stdin, "");
  delete_all_breakpoints ();
}


void
write_startup_message ()
{
  write_output (message_out, "SPIM %s\n", SPIM_VERSION);
  write_output (message_out, "Copyright 1990-2017 by James Larus.\n");
  write_output (message_out, "All Rights Reserved.\n");
  write_output (message_out, "SPIM is distributed under a BSD license.\n");
  write_output (message_out, "See the file README for a full copyright notice.\n");
}



void
initialize_registers (size_t context)
{
  reg_image_t &reg_image = reg_images[context];

  memclr (reg_image.FPR, FPR_LENGTH * sizeof (double));
  reg_image.FGR = (float *) reg_image.FPR;
  reg_image.FWR = (int *) reg_image.FPR;

  memclr (reg_image.R, R_LENGTH * sizeof (reg_word));
  reg_image.R[REG_SP] = STACK_TOP - BYTES_PER_WORD - 4096; /* Initialize $sp */
  reg_image.HI = reg_image.LO = 0;
  reg_image.PC = TEXT_BOT;

  CP0_BadVAddr(reg_image) = 0;
  CP0_Count(reg_image) = 0;
  CP0_Compare(reg_image) = 0;
  CP0_Status(reg_image) = (CP0_Status_CU & 0x30000000) | CP0_Status_IM | CP0_Status_UM;
  CP0_Cause(reg_image) = 0;
  CP0_EPC(reg_image) = 0;
#ifdef SPIM_BIGENDIAN
  CP0_Config =  CP0_Config_BE;
#else
  CP0_Config(reg_image) = 0;
#endif

  FIR(reg_image) = FIR_W | FIR_D | FIR_S;	/* Word, double, & single implemented */
  FCSR(reg_image) = 0x0;
  FCCR(reg_image) = 0x0;
  FEXR(reg_image) = 0x0;
  FENR(reg_image) = 0x0;

  reg_image.RFE_cycle = 0;
}


/* Read file NAME, which should contain assembly code. Return true if
   successful and false otherwise. */

bool
read_assembly_file (char *fpath, char *file_name)
{
  FILE *file = fopen (fpath, "rt");

  if (file == NULL)
    {
      error ("Cannot open file: `%s'\n", fpath);
      return false;
    }
  else
    {
      if (file_name == NULL) {
          file_name = strrchr(fpath, '/');
          file_name = file_name == NULL ? fpath : file_name + 1;
      }
      initialize_scanner (file, file_name);
      initialize_parser (fpath);

      while (!yyparse ()) ;

      fclose (file);
      flush_local_labels (!parse_error_occurred);
      end_of_assembly_file ();
      return true;
    }
}


mem_addr
starting_address ()
{
  return (find_symbol_address (DEFAULT_RUN_LOCATION));
}


#define MAX_ARGS 10000

/* Initialize the SPIM stack from a string containing the command line. */

void
initialize_stack(const char *command_line)
{
    int argc = 0;
    char *argv[MAX_ARGS];
    char *a;
    char *args = str_copy((char*)command_line); /* Destructively modify string */
    char *orig_args = args;

    while (*args != '\0')
    {
        /* Skip leading blanks */
        while (*args == ' ' || *args == '\t') args++;

        /* First non-blank char */
        a = args;

        /* Last non-blank, non-null char */
        while (*args != ' ' && *args != '\t' && *args != '\0') args++;

        /* Terminate word */
        if (a != args)
        {
            if (*args != '\0')
                *args++ = '\0';	/* Null terminate */

            argv[argc++] = a;

            if (MAX_ARGS == argc)
            {
                break;            /* If too many, ignore rest of list */
            }
        }
    }

    initialize_run_stack (argc, argv);
    free (orig_args);
}


/* Initialize the SPIM stack with ARGC, ARGV, and ENVP data. */

#ifdef _MSC_VER
#define environ	_environ
#endif

void
initialize_run_stack (int argc, char **argv)
{
  //FIXME: eventually run for both or neither
  int context = 0;
  reg_image_t &reg_image = reg_images[context];

  char **p;
  extern char **environ;
  int i, j = 0, env_j;
  mem_addr addrs[10000];


  reg_image.R[REG_SP] = STACK_TOP - 1; /* Initialize $sp */

  /* Put strings on stack: */
  /* env: */
  for (p = environ; *p != NULL; p++)
    addrs[j++] = copy_str_to_stack (context, *p);
  env_j = j;

  /* argv; */
  for (i = 0; i < argc; i++)
    addrs[j++] = copy_str_to_stack (context, argv[i]);

  /* Align stack pointer for word-size data */
  reg_image.R[REG_SP] = reg_image.R[REG_SP] & ~3;	/* Round down to nearest word */
  reg_image.R[REG_SP] -= BYTES_PER_WORD;	/* First free word on stack */
  reg_image.R[REG_SP] = reg_image.R[REG_SP] & ~7;	/* Double-word align stack-pointer*/

  /* Build vectors on stack: */
  /* env: */
  (void)copy_int_to_stack (context, 0);	/* Null-terminate vector */
  for (i = env_j - 1; i >= 0; i--)
    reg_image.R[REG_A2] = copy_int_to_stack (context, addrs[i]);

  /* argv: */
  (void)copy_int_to_stack (context, 0);	/* Null-terminate vector */
  for (i = j - 1; i >= env_j; i--)
    reg_image.R[REG_A1] = copy_int_to_stack (context, addrs[i]);

  /* argc: */
  reg_image.R[REG_A0] = argc;
  set_mem_word (context, reg_image.R[REG_SP], argc); /* Leave argc on stack */
}


static mem_addr
copy_str_to_stack (size_t context, char *s)
{
  reg_image_t &reg_image = reg_images[context];

  int i = (int)strlen (s);
  while (i >= 0)
    {
      set_mem_byte (context, reg_image.R[REG_SP], s[i]);
      reg_image.R[REG_SP] -= 1;
      i -= 1;
    }
  return ((mem_addr) reg_image.R[REG_SP] + 1); /* Leaves stack pointer byte-aligned!! */
}


static mem_addr
copy_int_to_stack (size_t context, int n)
{
  reg_image_t &reg_image = reg_images[context];

  set_mem_word (context, reg_image.R[REG_SP], n);
  reg_image.R[REG_SP] -= BYTES_PER_WORD;
  return ((mem_addr) reg_image.R[REG_SP] + BYTES_PER_WORD);
}


/* Run the program, starting at PC, for STEPS instructions. Display each
   instruction before executing if DISPLAY is true.  If CONT_BKPT is
   true, then step through a breakpoint. CONTINUABLE is true if
   execution can continue. Return true if breakpoint is encountered. */

bool
run_program (int steps, bool display, bool cont_bkpt, bool* continuable)
{
  if (map_click && !spimbot_tournament) {
    return true;
  }

  for (int i = 0; i < steps; ++ i) {
    for (size_t j = 0; j < num_contexts; ++ j) {
      size_t context = (j + steps) % num_contexts;
      set_current_image(context);
      reg_image_t &reg_image = reg_images[context];

      if (context == 0 && inst_is_breakpoint (reg_image.PC)) {
        if (cont_bkpt) {
          mem_addr addr = reg_image.PC;

          delete_breakpoint (addr);
          reg_image.exception_occurred = false;
          run_spim (context, display);
          add_breakpoint (addr);
        } else {
          return true;
        }
      } else {
        if (!run_spim (context, display)) {
          *continuable = false;
          world_update();
          return false;
        }
      }
    }
    cont_bkpt = false;
    int spimbot_break = world_update();
    if (spimbot_break >= 0) {
      if (spimbot_break == 0) {
        *continuable = false;
        return false;
      }
      else if (spimbot_break == 1) {
        return true;
      }
    }

    if (force_break) {
      return false;
    }
  }

  return false;
}


/* Set a breakpoint at memory location ADDR. */

void
add_breakpoint (mem_addr addr)
{
  bkpt *rec = (bkpt *) xmalloc (sizeof (bkpt));

  rec->next = bkpts;
  rec->addr = addr;

    bkpts = rec;
}


/* Delete all breakpoints at memory location ADDR. */

void
delete_breakpoint (mem_addr addr)
{
  bkpt *p, *b;
  int deleted_one = 0;

  for (p = NULL, b = bkpts; b != NULL; )
    if (b->addr == addr)
      {
	bkpt *n;

//	set_mem_inst (addr, b->inst);
	if (p == NULL)
	  bkpts = b->next;
	else
	  p->next = b->next;
	n = b->next;
	free (b);
	b = n;
	deleted_one = 1;
      }
    else
      p = b, b = b->next;
  if (!deleted_one)
    error ("No breakpoint to delete at 0x%08x\n", addr);
}


static void
delete_all_breakpoints ()
{
  bkpt *b, *n;

  for (b = bkpts, n = NULL; b != NULL; b = n)
    {
      n = b->next;
      free (b);
    }
  bkpts = NULL;
}


/* List all breakpoints. */

void
list_breakpoints ()
{
  bkpt *b;

  if (bkpts)
    for (b = bkpts;  b != NULL; b = b->next)
      write_output (message_out, "Breakpoint at 0x%08x\n", b->addr);
  else
    write_output (message_out, "No breakpoints set\n");
}



/* Utility routines */


/* Return the entry in the linear TABLE of length LENGTH with key STRING.
   TABLE must be sorted on the key field.
   Return NULL if no such entry exists. */

name_val_val *
map_string_to_name_val_val (name_val_val tbl[], int tbl_len, char *id)
{
  int low = 0;
  int hi = tbl_len - 1;

  while (low <= hi)
    {
      int mid = (low + hi) / 2;
      char *idp = id, *np = tbl[mid].name;

      while (*idp == *np && *idp != '\0') {idp ++; np ++;}

      if (*np == '\0' && *idp == '\0') /* End of both strings */
	return (& tbl[mid]);
      else if (*idp > *np)
	low = mid + 1;
      else
	hi = mid - 1;
    }

  return NULL;
}


/* Return the entry in the linear TABLE of length LENGTH with VALUE1 field NUM.
   TABLE must be sorted on the VALUE1 field.
   Return NULL if no such entry exists. */

name_val_val *
map_int_to_name_val_val (name_val_val tbl[], int tbl_len, int num)
{
  int low = 0;
  int hi = tbl_len - 1;

  while (low <= hi)
    {
      int mid = (low + hi) / 2;

      if (tbl[mid].value1 == num)
	return (&tbl[mid]);
      else if (num > tbl[mid].value1)
	low = mid + 1;
      else
	hi = mid - 1;
    }

  return NULL;
}


#ifdef NEED_VSPRINTF
char *
vsprintf (str, fmt, args)
     char *str,*fmt;
     va_list *args;
{
  FILE _strbuf;

  _strbuf._flag = _IOWRT+_IOSTRG;
  _strbuf._ptr = str;
  _strbuf._cnt = 32767;
  _doprnt(fmt, args, &_strbuf);
  putc('\0', &_strbuf);
  return(str);
}
#endif


#ifdef NEED_STRTOL
unsigned long
strtol (const char* str, const char** eptr, int base)
{
  long result;

  if (base != 0 && base != 16)
    fatal_error ("SPIM's strtol only works for base 16 (not base %d)\n", base);
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
    {
      str += 2;
      sscanf (str, "%lx", &result);
    }
  else if (base == 16)
    {
      sscanf (str, "%lx", &result);
    }
  else
    {
      sscanf (str, "%ld", &result);
    }
  return (result);
}
#endif


#ifdef NEED_STRTOUL
unsigned long
strtoul (const char* str, char** eptr, int base)
{
  unsigned long result;

  if (base != 0 && base != 16)
    fatal_error ("SPIM's strtoul only works for base 16 (not base %d)\n", base);
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
    {
      str += 2;
      sscanf (str, "%lx", &result);
    }
  else if (base == 16)
    {
      sscanf (str, "%lx", &result);
    }
  else
    {
      sscanf (str, "%ld", &result);
    }
  return (result);
}
#endif


char *
str_copy (char *str)
{
  return (strcpy ((char*)xmalloc ((int)strlen (str) + 1), str));
}


void *
xmalloc (int size)
{
  void *x = (void *) malloc (size);

  if (x == 0)
    fatal_error ("Out of memory at request for %d bytes.\n");
  return (x);
}


/* Allocate a zero'ed block of storage. */

void *
zmalloc (int size)
{
  void *z = (void *) malloc (size);

  if (z == 0)
    fatal_error ("Out of memory at request for %d bytes.\n");

  memclr (z, size);
  return (z);
}
