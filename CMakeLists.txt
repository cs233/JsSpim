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
    set(LINKER_FLAGS "-s EXIT_RUNTIME=0 -s PROXY_TO_PTHREAD -s PTHREAD_POOL_SIZE_STRICT=0 -s NO_DISABLE_EXCEPTION_CATCHING -s INITIAL_MEMORY=256MB -s DEFAULT_PTHREAD_STACK_SIZE=64KB -s TOTAL_STACK=64KB -pthread -lm -lembind -s WASM=1 -s ALLOW_MEMORY_GROWTH -s FORCE_FILESYSTEM=1 -s ENVIRONMENT=web,worker -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] --preload-file spim/CPU/exceptions.s@${DEFAULT_EXCEPTION_HANDLER}") # -s INITIAL_MEMORY=576mb -fsanitize=undefined -s MAXIMUM_MEMORY=4gb -s IMPORTED_MEMORY -s SAFE_HEAP=1 -s SAFE_HEAP_LOG=1 -s INITIAL_MEMORY=512mb -fsanitize=address # --post-js ${CMAKE_SOURCE_DIR}/src/JS/JavaScriptLoader.js --no-entry
    # When we switch to ...
    # emcc version 3.1.25, change TOTAL_STACK to STACK_SIZE

    if (CMAKE_BUILD_TYPE AND ${CMAKE_BUILD_TYPE} IN_LIST DEBUG_PROFILES)
        message("Adding additional Emscripten debug flags")
        string(JOIN " " ${LINKER_FLAGS} ${LINKER_FLAGS} "-gdwarf-5 -s ASSERTIONS=2" ${EXTRA_DEBUG_LINKER_FLAGS})
    endif()

    set(CMAKE_REQUIRED_FLAGS "${LINKER_FLAGS}")
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
    set(DEFAULT_EXCEPTION_HANDLER "\"spim/CPU/exceptions.s\"")
    set(LINKER_FLAGS "-pthread") # -g
    if (CMAKE_BUILD_TYPE AND ${CMAKE_BUILD_TYPE} IN_LIST DEBUG_PROFILES)
        string(JOIN " " ${LINKER_FLAGS} ${LINKER_FLAGS} ${EXTRA_DEBUG_LINKER_FLAGS})
    endif()
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
    add_executable(wasm spim/spim.cpp spim/worker.cpp)
else()
    add_executable(wasm spim/spim.cpp spim/worker.cpp) # main.cpp
endif()
target_link_directories(wasm PUBLIC spim/CPU/)
add_subdirectory(spim/CPU)

target_link_libraries(wasm spim)

add_compile_options(-I. -I$CPU -Wall -pedantic -Wextra -Wunused -Wno-write-strings -x c++)
set_target_properties(wasm PROPERTIES LINK_FLAGS "${LINKER_FLAGS}")

if (EMSCRIPTEN)
    add_custom_command(TARGET wasm
        POST_BUILD
        # COMMAND sed -i"" "s/throw 'getitimer\(\) is not implemented yet'//g" wasm.js
        # COMMAND sed -i"" "s/throw 'setitimer\(\) is not implemented yet'//g" wasm.js
        COMMAND mv wasm.* ../${DIST_DIR}/
    )
else()
    add_custom_command(TARGET wasm
        POST_BUILD
        COMMAND mv wasm* ../
    )
endif()
