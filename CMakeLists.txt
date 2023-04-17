cmake_minimum_required(VERSION 3.21.1)

project(JsSpim)

# -DCMAKE_BUILD_TYPE=Debug for debug info
# -fsanitize=address

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1) # helps out clangd LSP
set(EXTRA_DEBUG_LINKER_FLAGS "" CACHE STRING "You can add additional debug linker flags here")

list(APPEND DEBUG_PROFILES "Debug" "RelWithDebugInfo")

if (EMSCRIPTEN)
    message("Preparing WASM Makefile")
    if (NOT CMAKE_TOOLCHAIN_FILE)
        message(WARNING "CMAKE_TOOLCHAIN_FILE is not defined, you may need to set it \
                -DCMAKE_TOOLCHAIN_FILE=\"path/to/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake\" \
                OR using 'emcmake cmake ..' to build")
    endif ()

    set(MIN_VERSION "3.1.14")
    set(MAX_VERSION "3.1.25")
    if (${EMSCRIPTEN_VERSION} VERSION_LESS MIN_VERSION)
        message(FATAL_ERROR "EMSCRIPTEN version looks too old. Was ${EMSCRIPTEN_VERSION}, should be at least ${MIN_VERSION}.")
    endif ()
    if (${EMSCRIPTEN_VERSION} VERSION_GREATER MAX_VERSION)
        message(FATAL_ERROR "EMSCRIPTEN version is too new. Was ${EMSCRIPTEN_VERSION}, should be at least ${MAX_VERSION}.")
    endif ()

    set(DEFAULT_EXCEPTION_HANDLER "\"/usr/share/spim/exceptions.s\"")

    add_compile_definitions(WASM)
    set(DIST_DIR "dist")
    add_custom_target(build-dist-directory ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/${DIST_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/${DIST_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/js/ ${CMAKE_SOURCE_DIR}/${DIST_DIR}/js/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/css/ ${CMAKE_SOURCE_DIR}/${DIST_DIR}/css/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Tests/ ${CMAKE_SOURCE_DIR}/${DIST_DIR}/Tests/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/index.html ${CMAKE_SOURCE_DIR}/${DIST_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/coi-serviceworker.min.js ${CMAKE_SOURCE_DIR}/${DIST_DIR}/
    )
else()
    message("Preparing native Makefile")
    if (NOT DEFINED IDGAF)
        message(FATAL_ERROR
            "Compiling JsSpim natively is not supported as it serves zero functionality. It should\
            be able to, but there will be no interface to go with it, so it is only really good for\
            checking for compilation errors.

            To override this message, add the \"IDGAF\" define.
        ")
    endif()
    unset(IDGAF)
    set(DEFAULT_EXCEPTION_HANDLER "\"spim/CPU/exceptions.s\"")
    # add spim.cpp and spim.h
endif()

configure_file(spim/CPU/exceptions.s spim/CPU/exceptions.s COPYONLY)

add_compile_definitions(
    DEFAULT_EXCEPTION_HANDLER=${DEFAULT_EXCEPTION_HANDLER}
    TEXT_SIZE=65536
    DATA_SIZE=131072
    K_TEXT_SIZE=65536
)

if (EMSCRIPTEN)
    add_executable(wasm main.cpp spim/spim.cpp spim/worker.cpp)
else()
    add_executable(wasm spim/spim.cpp spim/worker.cpp) # main.cpp
endif()
target_link_directories(wasm PUBLIC spim/CPU/)
add_subdirectory(spim/CPU)

target_link_libraries(wasm spim)

target_compile_options(wasm PRIVATE -I. -I$CPU -Wall -pedantic -Wextra -Wunused -Wno-write-strings -x c++)

if (EMSCRIPTEN)
    target_link_options(wasm PRIVATE
        -pthread
        -lm
        -lembind
        "SHELL:-s EXIT_RUNTIME=0"
        "SHELL:-s PROXY_TO_PTHREAD"
        "SHELL:-s PTHREAD_POOL_SIZE_STRICT=0"
        "SHELL:-s NO_DISABLE_EXCEPTION_CATCHING"
        "SHELL:-s INITIAL_MEMORY=256MB"
        "SHELL:-s DEFAULT_PTHREAD_STACK_SIZE=64KB"
        "SHELL:-s TOTAL_STACK=64KB" # TODO: Once the minimum version of em++ > 3.1.25, please change this to STACK_SIZE
        "SHELL:-s WASM=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH"
        "SHELL:-s FORCE_FILESYSTEM=1"
        "SHELL:-s ENVIRONMENT=web,worker"
        "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "SHELL:--preload-file spim/CPU/exceptions.s@${DEFAULT_EXCEPTION_HANDLER}"
    )

    if (CMAKE_BUILD_TYPE IN_LIST DEBUG_PROFILES)
        message("Adding additional Emscripten debug flags")
        target_link_options(wasm PRIVATE
            -gdwarf-5
            "SHELL:-s ASSERTIONS=2"
            "SHELL:${EXTRA_DEBUG_LINKER_FLAGS}"
        )
    endif()

    target_compile_options(wasm PRIVATE
        -pthread
        "SHELL:-s NO_DISABLE_EXCEPTION_CATCHING"
    )

    add_custom_command(TARGET wasm
        POST_BUILD
        COMMAND mv wasm.* ../${DIST_DIR}/
    )
else()
    target_link_options(wasm PRIVATE "-pthread")
    if (CMAKE_BUILD_TYPE IN_LIST DEBUG_PROFILES)
        target_link_options(wasm PRIVATE "${EXTRA_DEBUG_LINKER_FLAGS}")
    endif()
    add_custom_command(TARGET wasm
        POST_BUILD
        COMMAND mv wasm* ../
    )
endif()
