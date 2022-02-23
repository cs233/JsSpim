#  SPIM S20 MIPS simulator.
#  Qt interface for SPIM simulator.
#
#  Copyright (c) 1990-2016, James R. Larus.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without modification,
#  are permitted provided that the following conditions are met:
#
#  Redistributions of source code must retain the above copyright notice,
#  this list of conditions and the following disclaimer.
#
#  Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation and/or
#  other materials provided with the distribution.
#
#  Neither the name of the James R. Larus nor the names of its contributors may be
#  used to endorse or promote products derived from this software without specific
#  prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
#  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#-------------------------------------------------
#
# Project created by QtCreator 2010-07-11T10:46:07
#
#-------------------------------------------------

QT       += core widgets gui printsupport

MY_TARGET = wasm
TEMPLATE = app

OBJECTS_DIR = ./objs
MOC_DIR = ./moc

YACCSOURCES = spim/CPU/parser.y
LEXSOURCES  = spim/CPU/scanner.l
#CONFIG      += yacc_no_name_mangle
CONFIG += c++1z
CONFIG -= release debug
QT += widgets gui

contains(QMAKE_CXX, em++) {
    QMAKE_LFLAGS += --emrun -s FORCE_FILESYSTEM=1\
                      -lm -O2 --bind\
                      --preload-file spim/CPU/exceptions.s@/usr/share/spim/exceptions.s\
                      -s ENVIRONMENT=web \
                      -s EXTRA_EXPORTED_RUNTIME_METHODS="\"\\\"['ccall', 'cwrap']\\\"\""
    QMAKE_CXXFLAGS += -pedantic -Wunused -Wno-write-strings -x c++

    MEM_SIZES = TEXT_SIZE=65536 DATA_SIZE=131072 K_TEXT_SIZE=65536
    DEFINES += $${MEM_SIZES} DEFAULT_EXCEPTION_HANDLER="\"\\\"/usr/share/spim/exceptions.s\\\"\""
    QMAKE_POST_LINK += sed -i \'\' \'s/throw\"getitimer() is not implemented yet\"//g\' wasm.js; \
	                      sed -i \'\' \'s/throw\"setitimer() is not implemented yet\"//g\' wasm.js; \
                        rm -f $${MY_TARGET}.html qtlogo.svg qtloader.js
    QMAKE_CLEAN += $${MY_TARGET}.wasm $${MY_TARGET}.js $${MY_TARGET}.data
} else {
    QMAKE_CLEAN += $${MY_TARGET} $${MY_TARGET}-debug $${MY_TARGET}-tsan
}

defined(TSAN, "var"):!equals(TSAN, 0) {             # TSAN Settings
    message("Building thread sanitizer Makefile")
    CONFIG += debug sanitizer sanitize_thread
    TARGET = $${MY_TARGET}-tsan
    DEFINES += DEBUG
} else:defined(DEBUG, "var"):!equals(DEBUG, 0) {    # Debug settings
    message("Building debug Makefile")
    CONFIG += debug
    TARGET = $${MY_TARGET}-debug
    DEFINES += DEBUG
} else {                                            # Release settings
    message("Building release Makefile")
    CONFIG += release
    TARGET = $${MY_TARGET}
}

SOURCES += spim/spim.cpp\
        spim/CPU/data.cpp\
        spim/CPU/inst.cpp\
        spim/CPU/mem.cpp\
        spim/CPU/run.cpp\
        spim/CPU/spim-utils.cpp\
        spim/CPU/string-stream.cpp\
        spim/CPU/sym-tbl.cpp\
        spim/CPU/syscall.cpp\
#        spim/CPU/vasm.cpp\
#    ../Spimbot/spimbotview.cpp\
#    ../Spimbot/spimbotwidget.cpp\
#    ../Spimbot/robot.cpp\
#    ../Spimbot/scenario.cpp\
#    ../Spimbot/puzzles/fillfill.cpp\
#    ../Spimbot/images/images.cpp\
#    ../Spimbot/images/animation.cpp\
#    ../Spimbot/inversus/*.cpp\
#    ../Spimbot/inversus/*.hpp\

        

# HEADERS  += spimview.h\
#        regtextedit.h\
#        texttextedit.h\
#        datatextedit.h\
#        spim_settings.h\
#        settablecheckbox.h\
#        console.h\
#    ../Spimbot/spimbotview.h\
#    ../Spimbot/spimbotwidget.h\
#    ../Spimbot/ui_spimbotview.h\
#    ../Spimbot/ui_spimbotwidget.h\
#    ../Spimbot/ui_spimview.h\
#    ../Spimbot/robot.h\
#    ../Spimbot/scenario.h\
#    ../Spimbot/puzzles/puzzle.h\
#    ../Spimbot/puzzles/fillfill.h\
#    ../Spimbot/images/images.h\
#    ../Spimbot/images/animation.h\
#    ../Spimbot/inversus/*.h\
#    ../Spimbot/utils/vec2d.h\

# FORMS    += spimview.ui\
#         savelogfile.ui\
#         printwindows.ui\
#         runparams.ui\
#         settings.ui\
#         changevalue.ui \
#         breakpoint.ui \
#     ../Spimbot/spimbotview.ui \
#     ../Spimbot/spimbotwidget.ui


INCLUDEPATH = spim/CPU $ENV{EMSCRIPTEN}/system/include/ # ../spim ../QtSpim
INCLUDEPATH += "/usr/local/Cellar/boost/1.76.0/include/"
INCLUDEPATH += "/opt/homebrew/Cellar/boost/1.76.0/include/"

# RESOURCES = windows_images.qrc exception.qrc ../Spimbot/images/spimbot_images.qrc

# win32:RC_FILE = qtspim.rc


QMAKE_YACC          = bison
QMAKE_YACCFLAGS     = -d --defines=parser.tab.h --output=parser.tab.cpp
QMAKE_YACCFLAGS_MANGLE = -p yy
QMAKE_YACC_HEADER   = parser.tab.h
QMAKE_YACC_SOURCE   = parser.tab.cpp

QMAKE_LEX           = flex
QMAKE_LEXFLAGS_MANGLE = -Pyy
QMAKE_LEXFLAGS      = -I -8 --outfile=lex.scanner.c


# Help file
#
#HELP_PROJ           = help/qtspim.qhp
#buildcompressedhelp.name    = Build compressed help
#buildcompressedhelp.input   = HELP_PROJ
#buildcompressedhelp.output  = help/${QMAKE_FILE_BASE}.qch
#buildcompressedhelp.commands= qhelpgenerator ${QMAKE_FILE_IN} -o ${QMAKE_FILE_OUT}
#buildcompressedhelp.CONFIG  = no_link recursive

# qcollectiongenerator must be run in the directory containing the project file, otherwise it
# puts partial paths in the .qhc file, which make it impossible to install the help files in
# other directories.
#
#HELP_COL_PROJ       = help/qtspim.qhcp
#buildhelpcollection.name    = Build help collection
#buildhelpcollection.input   = HELP_COL_PROJ
#buildhelpcollection.output  = help/${QMAKE_FILE_BASE}.qhc
#linux|macx:buildhelpcollection.commands= bash -c '\"pushd ${QMAKE_FILE_PATH}; qcollectiongenerator ${QMAKE_FILE_BASE}.qhcp; popd; $(MOVE) ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qhc ${QMAKE_FILE_OUT};\"'
#win32:buildhelpcollection.commands= cmd -c '\"pushd ${QMAKE_FILE_PATH} & qcollectiongenerator ${QMAKE_FILE_BASE}.qhcp & popd & $(MOVE) ${QMAKE_FILE_PATH}\\${QMAKE_FILE_BASE}.qhc ${QMAKE_FILE_OUT}\"'
#buildhelpcollection.CONFIG  = no_link recursive

#QMAKE_EXTRA_COMPILERS       += buildcompressedhelp buildhelpcollection
#POST_TARGETDEPS             += help/qtspim.qch help/qtspim.qhc


# Microsoft Visual C compiler flags
#
win32-msvc2008 {
  # Compile all files as C++
  #
  QMAKE_CFLAGS_DEBUG    += -TP
  QMAKE_CFLAGS_RELEASE  += -TP

  # Disable security warnings
  #
  DEFINES += _CRT_SECURE_NO_WARNINGS
}
win32-msvc2010 {
  # Compile all files as C++
  #
  QMAKE_CFLAGS_DEBUG    += -TP
  QMAKE_CFLAGS_RELEASE  += -TP

  # Disable security warnings
  #
  DEFINES += _CRT_SECURE_NO_WARNINGS
}
win32-msvc2012 {
  # Compile all files as C++
  #
  QMAKE_CFLAGS_DEBUG    += -TP
  QMAKE_CFLAGS_RELEASE  += -TP

  # Disable security warnings
  #
  DEFINES += _CRT_SECURE_NO_WARNINGS
}


# gcc flags
#
win32-g++ {
  # Compile all files as C++
  # Suppress gcc warning about deprecated conversion from string constant to char*
  #
  QMAKE_CFLAGS_DEBUG    += -Wno-write-strings
  QMAKE_CFLAGS_RELEASE  += -Wno-write-strings
  QMAKE_CXXFLAGS_DEBUG  += -Wno-write-strings
  QMAKE_CXXFLAGS_RELEASE += -Wno-write-strings

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
  QMAKE_MOVE = mv
}

linux-g++-32 {
  # Compile all files as C++
  # Suppress gcc warning about deprecated conversion from string constant to char*
  #
  QMAKE_CFLAGS_DEBUG    += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CFLAGS_RELEASE  += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CXXFLAGS_DEBUG  += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CXXFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -Wall -Wextra

  # Libraries will be installed in standard location
  QMAKE_RPATHDIR = /usr/lib/qtspim/lib

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
}

linux-g++ {
  QMAKE_CFLAGS_DEBUG   += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CXXFLAGS_DEBUG += -Wno-type-limits -Wno-write-strings -Wall -Wextra
  QMAKE_CXXFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -std=c++17 -Wall -Wextra

  # Libraries will be installed in standard location
  QMAKE_RPATHDIR = /usr/lib/qtspim/lib

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
}

# m1
macx:contains(QMAKE_HOST.arch, arm64):{
QMAKE_APPLE_DEVICE_ARCHS=arm64
  # Suppress macOS warning when using recent SDK version
  CONFIG+=sdk_no_version_check
message("mac m1")
  # Compile all files as C++
  # Suppress gcc warning about deprecated conversion from string constant to char*
  #
  QMAKE_CFLAGS_DEBUG    += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CFLAGS_RELEASE  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_DEBUG  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
  QMAKE_INFO_PLIST = macinfo.plist
  QMAKE_MACOSX_DEPLOYMENT_TARGET=10.14

  ICON = NewIcon.icns
}

macx-g++:contains(QMAKE_HOST.arch, x86_64):{
QMAKE_APPLE_DEVICE_ARCHS=x86_64
  # Suppress macOS warning when using recent SDK version
  CONFIG+=sdk_no_version_check
message("mac g++")
  # Compile all files as C++
  # Suppress gcc warning about deprecated conversion from string constant to char*
  #
  QMAKE_CFLAGS_DEBUG    += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CFLAGS_RELEASE  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_DEBUG  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
  QMAKE_INFO_PLIST = macinfo.plist
  QMAKE_MACOSX_DEPLOYMENT_TARGET=10.14

  ICON = NewIcon.icns
}

macx-clang:contains(QMAKE_HOST.arch, x86_64):{
QMAKE_APPLE_DEVICE_ARCHS=x86_64
  # Suppress macOS warning when using recent SDK version
  CONFIG+=sdk_no_version_check
message("mac clang")
  # Compile all files as C++
  # Suppress gcc warning about deprecated conversion from string constant to char*
  #
  QMAKE_CFLAGS_DEBUG    += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CFLAGS_RELEASE  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_DEBUG  += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra
  QMAKE_CXXFLAGS_RELEASE += -Wno-type-limits -Wno-write-strings -Wno-register -Wno-deprecated-register -Wall -Wextra

  # Suppress error when deleting non-existent file.
  #
  QMAKE_DEL_FILE = rm -f
  QMAKE_INFO_PLIST = macinfo.plist
  QMAKE_MACOSX_DEPLOYMENT_TARGET=10.14

  ICON = NewIcon.icns
}
