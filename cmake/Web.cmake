# SPDX-License-Identifier: GPL-3.0-or-later
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_PTHREADS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.26 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

add_executable(dhweb src/web_main.cpp)
target_link_libraries(dhweb PRIVATE SDL3::SDL3)
target_compile_options(dhweb PRIVATE -Wall -Wextra -fexceptions)
target_link_options(dhweb PRIVATE
    -fexceptions
    "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2"
    "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=67108864"
    "-sMAXIMUM_MEMORY=1073741824" "-sSTACK_SIZE=2097152"
    "-sFORCE_FILESYSTEM=1" "-sMODULARIZE=1" "-sEXPORT_NAME=createDownhill"
    "-sENVIRONMENT=web" "-sNO_EXIT_RUNTIME=1"
    "-sEXPORTED_FUNCTIONS=['_main','_dh_load','_dh_stage','_dh_play','_dh_pause','_dh_resize','_dh_input','_dh_error']"
    "-sEXPORTED_RUNTIME_METHODS=['FS','ccall','UTF8ToString']")
set_target_properties(dhweb PROPERTIES SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/public")
foreach(asset index.html app.js assets.js style.css local-assets.json)
    configure_file("${CMAKE_SOURCE_DIR}/web/${asset}" "${CMAKE_BINARY_DIR}/public/${asset}" COPYONLY)
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_BINARY_DIR}/public/LICENSE" COPYONLY)
configure_file("${sdl3_SOURCE_DIR}/LICENSE.txt" "${CMAKE_BINARY_DIR}/public/SDL-LICENSE.txt" COPYONLY)

# Existing CPU tests also run under Node with the actual WASM toolchain.
option(DH_WEB_TESTS "Build CPU tests for Node" ON)
if(DH_WEB_TESTS)
    enable_testing()
    find_program(DH_NODE node REQUIRED)
    foreach(name ground contact bike race controls text hud loadscreen integrator rider rider_mesh nga web_assets)
        add_executable(${name}_test tests/${name}_test.cpp)
        target_link_options(${name}_test PRIVATE "-sENVIRONMENT=node" "-sALLOW_MEMORY_GROWTH=1")
        add_test(NAME ${name}_test COMMAND "${DH_NODE}" $<TARGET_FILE:${name}_test>)
    endforeach()
endif()
