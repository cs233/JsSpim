file(GLOB Spim_SOURCES CONFIGURE_DEPENDS
    "data.cpp"
    "display-utils.cpp"
    "inst.cpp"
    "image.cpp"
    "image_print_stream.cpp"
    "mem.cpp"
    "run.cpp"
    "spim-utils.cpp"
    "string-stream.cpp"
    "sym-tbl.cpp"
    "syscall.cpp"
)
file(GLOB Spim_HEADERS CONFIGURE_DEPENDS "*.h")

set (Spim_INCLUDE_DIRS "")
foreach (_headerFile ${Spim_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND Spim_INCLUDE_DIRS ${_dir})
endforeach()
list (REMOVE_DUPLICATES Spim_INCLUDE_DIRS)

find_package(BISON 3.5.0 REQUIRED)
BISON_TARGET(MyParser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser_yacc.cpp
            DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser_yacc.h)

find_package(FLEX 2.6.4 REQUIRED)
FLEX_TARGET(MyLexxer scanner.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)

add_compile_options(-pthread -Wall -pedantic -Wextra -Wunused -Wno-write-strings -Wno-narrowing -x c++)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(spim STATIC ${Spim_SOURCES} ${BISON_MyParser_OUTPUTS} ${FLEX_MyLexxer_OUTPUTS} )
set_target_properties(spim PROPERTIES LINK_FLAGS "${LINKER_FLAGS}")
target_include_directories(spim PUBLIC ${Spim_INCLUDE_DIRS})
