/* SPIM S20 MIPS simulator.
   Execute SPIM syscalls, both in simulator and bare mode.
   Execute MIPS syscalls in bare mode, when running on MIPS systems.
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


#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "spim.h"
#include "string-stream.h"
#include "inst.h"
#include "image.h"
#include "mem.h"
#include "reg.h"
#include "sym-tbl.h"
#include "syscall.h"


#ifdef _WIN32
/* Windows has an handler that is invoked when an invalid argument is passed to a system
   call. https://msdn.microsoft.com/en-us/library/a9yf33zb(v=vs.110).aspx

   All good, except that the handler tries to invoke Watson and then kill spim with an exception.

   Override the handler to just report an error.
*/

#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>

void myInvalidParameterHandler(MIPSImage &img,
   const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   uintptr_t pReserved)
{
  if (function != NULL)
    {
      run_error (img, "Bad parameter to system call: %s\n", function);
    }
  else
    {
      run_error (img, "Bad parameter to system call\n");
    }
}

static _invalid_parameter_handler oldHandler;

// ig this wont work on Windows anymore due to MIPSImage lmao
void windowsParameterHandlingControl(int flag )
{
  static _invalid_parameter_handler oldHandler;
  static _invalid_parameter_handler newHandler = myInvalidParameterHandler;

  if (flag == 0)
    {
      oldHandler = _set_invalid_parameter_handler(newHandler);
      _CrtSetReportMode(_CRT_ASSERT, 0); // Disable the message box for assertions.
    }
  else
    {
      newHandler = _set_invalid_parameter_handler(oldHandler);
      _CrtSetReportMode(_CRT_ASSERT, 1);  // Enable the message box for assertions.
    }
}
#endif

/* The address of the last exception. Different from EPC
 * if one exception occurs inside another or an interrupt. */
mem_addr last_exception_addr;


/* Decides which syscall to execute or simulate.  Returns zero upon
   exit syscall and non-zero to continue execution. */

int
do_syscall (MIPSImage &img)
{
#ifdef _WIN32
    windowsParameterHandlingControl(0);
#endif

  /* Syscalls for the source-language version of SPIM.  These are easier to
     use than the real syscall and are portable to non-MIPS operating
     systems. */

  switch (img.reg_image().R[REG_V0])
    {
    case PRINT_INT_SYSCALL:
      write_output (img, console_out, "%d", img.reg_image().R[REG_A0]);
      break;

    case PRINT_FLOAT_SYSCALL:
      {
	float val = FPR_S (img.reg_image(), REG_FA0);

	write_output (img, console_out, "%.8f", val);
	break;
      }

    case PRINT_DOUBLE_SYSCALL:
      write_output (img, console_out, "%.18g", img.reg_image().FPR[REG_FA0 / 2]);
      break;

    case PRINT_STRING_SYSCALL:
      write_output (img, console_out, "%s", mem_reference (img, img.reg_image().R[REG_A0]));
      break;

    case READ_INT_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	img.reg_image().R[REG_RES] = atol (str);
	break;
      }

    case READ_FLOAT_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	FPR_S (img.reg_image(), REG_FRES) = (float) atof (str);
	break;
      }

    case READ_DOUBLE_SYSCALL:
      {
	static char str [256];

	read_input (str, 256);
	img.reg_image().FPR [REG_FRES] = atof (str);
	break;
      }

    case READ_STRING_SYSCALL:
      {
	read_input ( (char *) mem_reference (img, img.reg_image().R[REG_A0]), img.reg_image().R[REG_A1]);
	img.mem_image().data_modified = true;
	break;
      }

    case SBRK_SYSCALL:
      {
	mem_addr x = img.mem_image().data_top;
	expand_data (img, img.reg_image().R[REG_A0]);
	img.reg_image().R[REG_RES] = x;
	img.mem_image().data_modified = true;
	break;
      }

    case PRINT_CHARACTER_SYSCALL:
      write_output (img, console_out, "%c", img.reg_image().R[REG_A0]);
      break;

    case READ_CHARACTER_SYSCALL:
      {
	static char str [2];

	read_input (str, 2);
	if (*str == '\0') *str = '\n';      /* makes xspim = spim */
	img.reg_image().R[REG_RES] = (long) str[0];
	break;
      }

    case EXIT_SYSCALL:
      spim_return_value = 0;
      return (0);

    case EXIT2_SYSCALL:
      spim_return_value = img.reg_image().R[REG_A0];	/* value passed to spim's exit() call */
      return (0);

    case OPEN_SYSCALL:
      {
#ifdef _WIN32
        R[REG_RES] = _open((char*)mem_reference (img, R[REG_A0]), R[REG_A1], R[REG_A2]);
#else
	img.reg_image().R[REG_RES] = open((char*)mem_reference (img, img.reg_image().R[REG_A0]), img.reg_image().R[REG_A1], img.reg_image().R[REG_A2]);
#endif
	break;
      }

    case READ_SYSCALL:
      {
	/* Test if address is valid */
	(void)mem_reference (img, img.reg_image().R[REG_A1] + img.reg_image().R[REG_A2] - 1);
#ifdef _WIN32
	img.reg_image().R[REG_RES] = _read(img.reg_image().R[REG_A0], mem_reference (img, img.reg_image().R[REG_A1]), img.reg_image().R[REG_A2]);
#else
	img.reg_image().R[REG_RES] = read(img.reg_image().R[REG_A0], mem_reference (img, img.reg_image().R[REG_A1]), img.reg_image().R[REG_A2]);
#endif
	img.mem_image().data_modified = true;
	break;
      }

    case WRITE_SYSCALL:
      {
	/* Test if address is valid */
	(void)mem_reference (img, img.reg_image().R[REG_A1] + img.reg_image().R[REG_A2] - 1);
#ifdef _WIN32
	img.reg_image().R[REG_RES] = _write(img.reg_image().R[REG_A0], mem_reference (img, img.reg_image().R[REG_A1]), img.reg_image().R[REG_A2]);
#else
	img.reg_image().R[REG_RES] = write(img.reg_image().R[REG_A0], mem_reference (img, img.reg_image().R[REG_A1]), img.reg_image().R[REG_A2]);
#endif
	break;
      }

    case CLOSE_SYSCALL:
      {
#ifdef _WIN32
	img.reg_image().R[REG_RES] = _close(img.reg_image().R[REG_A0]);
#else
	img.reg_image().R[REG_RES] = close(img.reg_image().R[REG_A0]);
#endif
	break;
      }

case PRINT_HEX_SYSCALL:
    write_output (img, console_out, "%x", img.reg_image().R[REG_A0]);
    break;

    default:
      run_error (img, "Unknown system call: %d\n", img.reg_image().R[REG_V0]);
      break;
    }

#ifdef _WIN32
    windowsParameterHandlingControl(1);
#endif
  return (1);
}


void
handle_exception (MIPSImage &img)
{
  if (!quiet && CP0_ExCode(img.reg_image()) != ExcCode_Int)
    error (img, "Exception occurred at PC=0x%08x\n", last_exception_addr);

  img.reg_image().exception_occurred = 0;
  img.reg_image().PC = EXCEPTION_ADDR;

  switch (CP0_ExCode(img.reg_image()))
    {
    case ExcCode_Int:
      break;

    case ExcCode_AdEL:
      if (!quiet)
	error (img, "  Unaligned address in inst/data fetch: 0x%08x\n", img.reg_image().CP0_BadVAddr);
      break;

    case ExcCode_AdES:
      if (!quiet)
	error (img, "  Unaligned address in store: 0x%08x\n", img.reg_image().CP0_BadVAddr);
      break;

    case ExcCode_IBE:
      if (!quiet)
	error (img, "  Bad address in text read: 0x%08x\n", img.reg_image().CP0_BadVAddr);
      break;

    case ExcCode_DBE:
      if (!quiet)
	error (img, "  Bad address in data/stack read: 0x%08x\n", img.reg_image().CP0_BadVAddr);
      break;

    case ExcCode_Sys:
      if (!quiet)
	error (img, "  Error in syscall\n");
      break;

    case ExcCode_Bp:
      img.reg_image().exception_occurred = 0;
      return;

    case ExcCode_RI:
      if (!quiet)
	error (img, "  Reserved instruction execution\n");
      break;

    case ExcCode_CpU:
      if (!quiet)
	error (img, "  Coprocessor unuable\n");
      break;

    case ExcCode_Ov:
      if (!quiet)
	error (img, "  Arithmetic overflow\n");
      break;

    case ExcCode_Tr:
      if (!quiet)
	error (img, "  Trap\n");
      break;

    case ExcCode_FPE:
      if (!quiet)
	error (img, "  Floating point\n");
      break;

    default:
      if (!quiet)
	error (img, "Unknown exception: %d\n", CP0_ExCode);
      break;
    }
}
