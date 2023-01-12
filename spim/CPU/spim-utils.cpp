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


#include <chrono>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <thread>

#include "spim.h"
#include "version.h"
#include "string-stream.h"
#include "spim-utils.h"
#include "inst.h"
#include "data.h"
#include "image.h"
#include "mem.h"
#include "reg.h"
#include "scanner.h"
#include "parser.h"
#include "parser_yacc.h"
#include "run.h"
#include "sym-tbl.h"

/* Internal functions: */

static mem_addr copy_int_to_stack (MIPSImage &img, int n);
static mem_addr copy_str_to_stack (MIPSImage &img, char *s);
static void delete_all_breakpoints (MIPSImage &img);


bkpt *bkpts = NULL;

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
initialize_world (MIPSImage &img, char *exception_files, bool print_message)
{
  img.reg_image().auto_alignment = 1;
  
  /* Allocate the floating point registers */
  if (img.reg_image().FGR == NULL)
    img.reg_image().FPR = (double *) xmalloc (img, FPR_LENGTH * sizeof (double));
  /* Allocate the memory */
  make_memory (img, initial_text_size,
	       initial_data_size, initial_data_limit,
	       initial_stack_size, initial_stack_limit,
	       initial_k_text_size,
	       initial_k_data_size, initial_k_data_limit);
  initialize_registers (img);
  initialize_inst_tables ();
  initialize_symbol_table (img);
  k_text_begins_at_point (img, K_TEXT_BOT);
  k_data_begins_at_point (img, K_DATA_BOT);
  data_begins_at_point (img, DATA_BOT);
  text_begins_at_point (img, TEXT_BOT);

  if (exception_files != NULL)
    {
      bool old_bare = bare_machine;
      bool old_accept = accept_pseudo_insts;
      char *filename;
      char *files;

      /* Save machine state */
      bare_machine = false;     /* Exception handler uses extended machine */
      accept_pseudo_insts = true;

      /* strtok modifies the string, so we must back up the string prior to use. */
      if ((files = strdup (exception_files)) == NULL)
         fatal_error (img, "Insufficient memory to complete.\n");

      for (filename = strtok (files, ";"); filename != NULL; filename = strtok (NULL, ";"))
         {
            if (!read_assembly_file (img, filename))
               fatal_error (img, "Cannot read exception handler: %s\n", filename);

            if (print_message)
                write_output (img, message_out, "Loaded: %s\n", filename);
         }

      free (files);

      /* Restore machine state */
      bare_machine = old_bare;
      accept_pseudo_insts = old_accept;

      if (!bare_machine)
      {
	(void)make_label_global (img, "main"); /* In case .globl main forgotten */
	(void)record_label (img, "main", 0, 0);
      }
    }
  initialize_scanner (stdin, "");
  delete_all_breakpoints (img); // bruh TODO: this function is meant to be contextual-based and then we have THIS here?!?!
}


void
write_startup_message (MIPSImage &img)
{
  write_output (img, message_out, "SPIM %s\n", SPIM_VERSION);
  write_output (img, message_out, "Copyright 1990-2017 by James Larus.\n");
  write_output (img, message_out, "All Rights Reserved.\n");
  write_output (img, message_out, "SPIM is distributed under a BSD license.\n");
  write_output (img, message_out, "See the file README for a full copyright notice.\n");
}



void
initialize_registers (MIPSImage &img)
{
  memclr (img.reg_image().FPR, FPR_LENGTH * sizeof (double));
  img.reg_image().FGR = (float *) img.reg_image().FPR;
  img.reg_image().FWR = (int *) img.reg_image().FPR;

  memclr (img.reg_image().R, R_LENGTH * sizeof (reg_word));
  img.reg_image().R[REG_SP] = STACK_TOP - BYTES_PER_WORD - 4096; /* Initialize $sp */
  img.reg_image().HI = img.reg_image().LO = 0;
  img.reg_image().PC = 0;

  img.reg_image().CP0_BadVAddr = 0;
  img.reg_image().CP0_Count = 0;
  img.reg_image().CP0_Compare = 0;
  img.reg_image().CP0_Status = (CP0_Status_CU & 0x30000000) | CP0_Status_IM | CP0_Status_UM;
  img.reg_image().CP0_Cause = 0;
  img.reg_image().CP0_EPC = 0;
#ifdef SPIM_BIGENDIAN
  img.reg_image().CP0_Config =  img.reg_image().CP0_Config_BE;
#else
  img.reg_image().CP0_Config = 0;
#endif

  img.reg_image().FIR = FIR_W | FIR_D | FIR_S;	/* Word, double, & single implemented */
  img.reg_image().FCSR = 0x0;
  img.reg_image().RFE_cycle = 0;
}


/* Read file NAME, which should contain assembly code. Return true if
   successful and false otherwise. */

bool
read_assembly_file (MIPSImage &img, char *fpath)
{
  FILE *file = fopen (fpath, "rt");

  if (file == NULL)
    {
      error (img, "Cannot open file: `%s'\n", fpath);
      return false;
    }
  else
    {
      char *file_name;
      file_name = strrchr(fpath, '/');
      file_name = file_name == NULL ? fpath : file_name + 1;
    
      initialize_scanner (file, file_name);
      initialize_parser (fpath);

      while (!yyparse (img)) ;

      fclose (file);
      flush_local_labels (img, !parse_error_occurred);
      end_of_assembly_file (img);
      return true;
    }
}


mem_addr
starting_address (MIPSImage &img)
{
  return (find_symbol_address (img, DEFAULT_RUN_LOCATION));
}


#define MAX_ARGS 10000

/* Initialize the SPIM stack from a string containing the command line. */

void
initialize_stack(MIPSImage &img, const char *command_line)
{
    int argc = 0;
    char *argv[MAX_ARGS];
    char *a;
    char *args = str_copy(img, (char*)command_line); /* Destructively modify string */
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

    initialize_run_stack (img, argc, argv);
    free (orig_args);
}


/* Initialize the SPIM stack with ARGC, ARGV, and ENVP data. */

#ifdef _MSC_VER
#define environ	_environ
#endif

void
initialize_run_stack (MIPSImage &img, int argc, char **argv)
{
  char **p;
  extern char **environ;
  int i, j = 0, env_j;
  mem_addr addrs[10000];


  img.reg_image().R[REG_SP] = STACK_TOP - 1; /* Initialize $sp */

  /* Put strings on stack: */
  /* env: */
  for (p = environ; *p != NULL; p++)
    addrs[j++] = copy_str_to_stack (img, *p);
  env_j = j;

  /* argv; */
  for (i = 0; i < argc; i++)
    addrs[j++] = copy_str_to_stack (img, argv[i]);

  /* Align stack pointer for word-size data */
  img.reg_image().R[REG_SP] = img.reg_image().R[REG_SP] & ~3;	/* Round down to nearest word */
  img.reg_image().R[REG_SP] -= BYTES_PER_WORD;	/* First free word on stack */
  img.reg_image().R[REG_SP] = img.reg_image().R[REG_SP] & ~7;	/* Double-word align stack-pointer*/

  /* Build vectors on stack: */
  /* env: */
  (void)copy_int_to_stack (img, 0);	/* Null-terminate vector */
  for (i = env_j - 1; i >= 0; i--)
    img.reg_image().R[REG_A2] = copy_int_to_stack (img, addrs[i]);

  /* argv: */
  (void)copy_int_to_stack (img, 0);	/* Null-terminate vector */
  for (i = j - 1; i >= env_j; i--)
    img.reg_image().R[REG_A1] = copy_int_to_stack (img, addrs[i]);

  /* argc: */
  img.reg_image().R[REG_A0] = argc;
  set_mem_word (img, img.reg_image().R[REG_SP], argc); /* Leave argc on stack */
}


static mem_addr
copy_str_to_stack (MIPSImage &img, char *s)
{
  int i = (int)strlen (s);
  while (i >= 0)
    {
      set_mem_byte (img, img.reg_image().R[REG_SP], s[i]);
      img.reg_image().R[REG_SP] -= 1;
      i -= 1;
    }
  return ((mem_addr) img.reg_image().R[REG_SP] + 1); /* Leaves stack pointer byte-aligned!! */
}


static mem_addr
copy_int_to_stack (MIPSImage &img, int n)
{
  set_mem_word (img, img.reg_image().R[REG_SP], n);
  img.reg_image().R[REG_SP] -= BYTES_PER_WORD;
  return ((mem_addr) img.reg_image().R[REG_SP] + BYTES_PER_WORD);
}

/* Run the program, starting at PC, for 1 instruction. Display each
   instruction before executing if DISPLAY is true.  If CONT_BKPT is
   true, then step through a breakpoint. CONTINUABLE is true if
   execution can continue. Return true if breakpoint is encountered. */

bool
step_program (MIPSImage &img, bool display, bool cont_bkpt, bool* continuable)
{
    img.reg_image().exception_occurred = false;
    *continuable = spim_step(img, display);

    if (img.reg_image().exception_occurred && CP0_ExCode(img.reg_image()) == ExcCode_Bp) {
        /* Turn off EXL bit, so subsequent interrupts set EPC since the break is
      handled by SPIM code, not MIPS code. */
        img.reg_image().CP0_Status &= ~CP0_Status_EXL;
        return true;
    }

    return false;
}

bool run_spim_program(std::vector<MIPSImage> &imgs, int steps, bool display, bool cont_bkpt, bool* continuable, std::timed_mutex &mtx, const unsigned long &delay_usec) {
  int pgrm_done;

  *continuable = true;

  for (int i = 0; i < steps; ++i) {
    pgrm_done = 0;

    bool bkpt_occurred = false;

    if (delay_usec) {
        std::this_thread::sleep_for(std::chrono::microseconds(delay_usec));
    }

    std::lock_guard<std::timed_mutex> lock(mtx);
    if (!cont_bkpt) {
        for (auto & img : imgs) {
            auto res = img.breakpoints().find(img.reg_image().PC);
            if (res != img.breakpoints().end()) {
                bkpt_occurred = true;
                error(img, "Breakpoint encountered at 0x%08x\n", img.reg_image().PC);
            }
        }
    }

    if (bkpt_occurred) {
      return true;
    }

    for (auto &img : imgs) {
      bool cont; // Determines if the given context program is finished
      pgrm_done += !step_program(img, display, cont_bkpt, &cont);

      *continuable &= cont; // If any of the contexts are not continuable, then end the program
    }

    // End the execution immediately if one context is not continuable
    if (!*continuable) {
      return false;
    }

    cont_bkpt = false; // Don't skip future breakpoints
  }

  return (pgrm_done != NUM_CONTEXTS);
}

bool
run_spimbot_program (int steps, bool display, bool cont_bkpt, bool* continuable) {
  /*if (map_click && !spimbot_tournament) {
    return true;
  }*/

  // for (int i = 0; i < steps; ++i, ctx_switch(0)) {
  //   for (size_t j = 0; j < NUM_CONTEXTS; ++j, ctx_increment()) {
  //     bool result = step_program(display, cont_bkpt, continuable);
  //   }

  //   int spimbot_break = 0;//world_update();
  //   if (spimbot_break >= 0) {
  //     if (spimbot_break == 0) {
  //       *continuable = false;
  //       return false;
  //     }
  //     else if (spimbot_break == 1) {
  //       return true;
  //     }
  //   }
  // }

}


/* Set a breakpoint at memory location ADDR. */

bool
add_breakpoint (MIPSImage &img, mem_addr addr)
{
    const auto [_, ok] = img.breakpoints().emplace(addr, addr);

    if (!ok) {
        error (img, "Cannot put a breakpoint at address 0x%08x\n", addr);
        return false;
    }

    error(img, "Added breakpoint at address 0x%08x\n", addr);
    return true;
}


/* Delete all breakpoints at memory location ADDR. */

bool
delete_breakpoint (MIPSImage &img, mem_addr addr)
{
    const auto ok = img.breakpoints().erase(addr);

    if (!ok) {
        error (img, "No breakpoint to delete at 0x%08x\n", addr);
        return false;
    }

    error (img, "Deleted breakpoint at 0x%08x\n", addr);
    return true;
}

// Should delete all breakpoints for a given context
static void
delete_all_breakpoints (MIPSImage &img)
{
  // for (size_t j = 0; j < NUM_CONTEXTS; ++j, ctx_increment()) {
  //   breakpoints().clear();
  // }
  img.breakpoints().clear();
}


/* List all breakpoints. */

void
list_breakpoints (MIPSImage &img)
{
  if (img.breakpoints().size() > 0) {
    for (auto const &b: img.breakpoints()) { // b is std::pair<mem_addr, breakpoint>
      write_output (img, message_out, "Breakpoint at 0x%08x\n", b.first);
    }
  } else {
    write_output (img, message_out, "No breakpoints set\n");
  }
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
strtol (MIPSImage &img, const char* str, const char** eptr, int base)
{
  long result;

  if (base != 0 && base != 16)
    fatal_error (img, "SPIM's strtol only works for base 16 (not base %d)\n", base);
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
strtoul (MIPSImage &img, const char* str, char** eptr, int base)
{
  unsigned long result;

  if (base != 0 && base != 16)
    fatal_error (img, "SPIM's strtoul only works for base 16 (not base %d)\n", base);
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
str_copy (MIPSImage &img, char *str)
{
  return (strcpy ((char*)xmalloc (img, (int)strlen (str) + 1), str));
}


void *
xmalloc (MIPSImage &img, int size)
{
  void *x = (void *) malloc (size);

  if (x == 0)
    fatal_error (img, "Out of memory at request for %d bytes.\n");
  return (x);
}


/* Allocate a zero'ed block of storage. */

void *
zmalloc (MIPSImage &img, int size)
{
  void *z = (void *) malloc (size);

  if (z == 0)
    fatal_error (img, "Out of memory at request for %d bytes.\n");

  memclr (z, size);
  return (z);
}
