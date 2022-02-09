Files spim-jsspim/CPU/data.cpp and spim-qtbase/CPU/data.cpp differ
    * just one variable rename
    * done (keep JS)
Files spim-jsspim/CPU/data.h and spim-qtbase/CPU/data.h are identical
    * done (identical)
Files spim-jsspim/CPU/display-utils.cpp and spim-qtbase/CPU/display-utils.cpp differ *
    ** depends on other files
Files spim-jsspim/CPU/dump_ops.cpp and spim-qtbase/CPU/dump_ops.cpp are identical
    * done (identical)
Files spim-jsspim/CPU/exceptions.s and spim-qtbase/CPU/exceptions.s are identical
    * done (identical)
Files spim-jsspim/CPU/inst.cpp and spim-qtbase/CPU/inst.h differ **
    * QTSPIM has more text alignment functions
    * QTSPIM checks addr!=0 when checking for breakpoint. auto breakpt at addr 0? 
    * QTSPIM implements jump instructions with a diff bitmask 0x3ffffff vs JSSPIM 0x2ffffff
    ** QTSPIM has some floating point definitions CCFP NDFP TFF
Files spim-jsspim/CPU/inst.h and spim-qtbase/CPU/inst.cpp differ **
    ** QTSPIM has local func mk_co_r_inst for R3 FP instructions. unclear if this is a real difference in implementation or just a weird abstraction
Files spim-jsspim/CPU/mem.cpp and spim-qtbase/CPU/mem.cpp are identical
    * done (identical)
Files spim-jsspim/CPU/mem.h and spim-qtbase/CPU/mem.h are identical
    * done (identical)
Files spim-jsspim/CPU/op.h and spim-qtbase/CPU/op.h are identical
    * done (identical)
Files spim-jsspim/CPU/parser.h and spim-qtbase/CPU/parser.h are identical
    * done (identical)
Files spim-jsspim/CPU/parser.y and spim-qtbase/CPU/parser.y differ **
    * QTSPIM has branch_offset "3" vs JSSPIM "2". same as jump inst difference?
    * QTSPIM inserts nop instructions when branching??
    ** QTSPIM has a store_fp_op function. same as R3FP difference?
    * QTSPIM uses align text functions, JSSPIM uses align data functions
    ** JSSPIM has some weird double casting
Files spim-jsspim/CPU/reg.h and spim-qtbase/CPU/reg.h differ **
    ** QTSPIM and JSSPIM implement floating point control registers differently
    ** unclear whether the two are logically equivalent or legitimately different
    ** will look at its usage in other places before deciding what to do
Files spim-jsspim/CPU/run.cpp and spim-qtbase/CPU/run.cpp differ *
    ** differences in floating point control register access (see above)
    ** possibly logically equivalent - requires testing
Files spim-jsspim/CPU/run.h and spim-qtbase/CPU/run.h differ
    * documentation difference
    * done (keep JS)
Files spim-jsspim/CPU/scanner.h and spim-qtbase/CPU/scanner.h are identical
    * done (identical)
Files spim-jsspim/CPU/scanner.l and spim-qtbase/CPU/scanner.l differ
    * documentation difference
    * done (keep JS)
Files spim-jsspim/CPU/spim-syscall.h and spim-qtbase/CPU/spim-syscall.h are identical
    * done (identical)
Files spim-jsspim/CPU/spim-utils.cpp and spim-qtbase/CPU/spim-utils.cpp differ *
    * differences in floating point control register access (see reg.h)
Files spim-jsspim/CPU/spim-utils.h and spim-qtbase/CPU/spim-utils.h are identical
Files spim-jsspim/CPU/spim.h and spim-qtbase/CPU/spim.h are identical
Files spim-jsspim/CPU/string-stream.cpp and spim-qtbase/CPU/string-stream.cpp are identical
Files spim-jsspim/CPU/string-stream.h and spim-qtbase/CPU/string-stream.h are identical
Files spim-jsspim/CPU/sym-tbl.cpp and spim-qtbase/CPU/sym-tbl.cpp are identical
Files spim-jsspim/CPU/sym-tbl.h and spim-qtbase/CPU/sym-tbl.h are identical
Files spim-jsspim/CPU/syscall.cpp and spim-qtbase/CPU/syscall.cpp are identical
Files spim-jsspim/CPU/syscall.h and spim-qtbase/CPU/syscall.h are identical
    * done (identical)
Files spim-jsspim/CPU/version.h and spim-qtbase/CPU/version.h differ
Files spim-jsspim/Makefile and spim-qtbase/Makefile differ
Files spim-jsspim/README and spim-qtbase/README differ
Files spim-jsspim/Tests/read.s and spim-qtbase/Tests/read.s differ
Files spim-jsspim/Tests/time.s and spim-qtbase/Tests/time.s differ
Files spim-jsspim/Tests/timer.s and spim-qtbase/Tests/timer.s differ
Files spim-jsspim/Tests/tt.alu.bare.s and spim-qtbase/Tests/tt.alu.bare.s differ
Files spim-jsspim/Tests/tt.bare.s and spim-qtbase/Tests/tt.bare.s differ
Files spim-jsspim/Tests/tt.be.s and spim-qtbase/Tests/tt.be.s differ
Files spim-jsspim/Tests/tt.core.s and spim-qtbase/Tests/tt.core.s differ
Files spim-jsspim/Tests/tt.dir.s and spim-qtbase/Tests/tt.dir.s differ
Files spim-jsspim/Tests/tt.fpu.bare.s and spim-qtbase/Tests/tt.fpu.bare.s differ
Files spim-jsspim/Tests/tt.in and spim-qtbase/Tests/tt.in differ
Files spim-jsspim/Tests/tt.io.s and spim-qtbase/Tests/tt.io.s differ
Files spim-jsspim/Tests/tt.le.s and spim-qtbase/Tests/tt.le.s differ
Files spim-jsspim/helloworld.s and spim-qtbase/helloworld.s differ
    * not relevant
Files spim-jsspim/spim.cpp and spim-qtbase/spim.cpp differ *
    * quite different
    * as this is the top-level unit for both program, we should keep JSSPIM's copy
    * ~no differences between QtSpim base and QtSpimbot spim.cpp (does spimbot even require this file?)
