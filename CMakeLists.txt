cmake_minimum_required(VERSION 3.16)
project(umk3 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(umk3
    runtime/main.c
    runtime/lime/meshset.c
    runtime/lime/limemath.c
    runtime/lime/pvr.c
    runtime/lime/par.c
    runtime/lime/png.c
    runtime/lime/light.c
)

target_include_directories(umk3 PRIVATE runtime)

# The front end, in a window. `runtime/menu_main.c` and `runtime/draw_gl.c`
# have existed for a while but were never a build target, so the only way to
# see the menu was to remember the gcc line in menu_main.c's header. It is a
# separate executable rather than a mode of `umk3` because it links the whole
# of decomp/gamecode and decomp/lime, which the mesh viewer has no use for.
#
# UMK3_REAL_GL is what swaps the counting platform layer for the drawing one.
file(GLOB UMK3_MENU_GAMECODE decomp/gamecode/*.c)
file(GLOB UMK3_MENU_LIME     decomp/lime/*.c)
file(GLOB UMK3_MENU_RUNLIME  runtime/lime/*.c)

add_executable(umk3-menu
    runtime/menu_main.c
    runtime/menu_splash.c
    runtime/draw_gl.c
    runtime/gamecode_globals.c
    runtime/gamecode_stubs.c
    runtime/lime_menu.c
    runtime/lime_platform.c
    runtime/lime_app.c
    runtime/wav.c
    ${UMK3_MENU_GAMECODE}
    ${UMK3_MENU_LIME}
    ${UMK3_MENU_RUNLIME}
)
target_include_directories(umk3-menu PRIVATE runtime decomp/lime)
target_compile_definitions(umk3-menu PRIVATE UMK3_REAL_GL)

# The fight scene. It links ONLY mk3.c out of decomp/gamecode/logic, because
# that is the one file whose functions this scene actually calls --
# TranslateJoybits for the input and gravity_n_bounds for the physics -- and
# the rest of the directory still has 159 undefined functions and 229
# unextracted data tables between it and a link.
#
# What the scene supplies instead of the missing engine is written down at the
# top of runtime/fight_scene.c, under "WHAT IS REAL HERE, AND WHAT IS NOT".
add_executable(umk3-fight
    runtime/fight_scene.c
    runtime/fight_render.c
    runtime/fight_audio.c
    runtime/wav.c
    runtime/fight_select.c
    runtime/fight_stubs.c
    runtime/lime/meshset.c
    runtime/lime/scene.c
    runtime/lime/events.c
    runtime/lime/skin.c
    runtime/lime/light.c
    runtime/lime/limemath.c
    runtime/lime/pvr.c
    runtime/lime/par.c
    runtime/lime/png.c
    decomp/gamecode/logic/mk3.c
)
target_include_directories(umk3-fight PRIVATE runtime decomp/lime)

# UMK3-Test: the front end and the fight scene in one program.
#
# This is the build to hand someone for testing. It links the whole of
# decomp/gamecode (the front end) AND decomp/gamecode/logic/mk3.c (the fight's
# input translation and physics), with runtime/test_main.c owning which of the
# two is running. UMK3_SHELL tells fight_scene.c to leave out its own main.
add_executable(umk3-test
    runtime/test_main.c
    runtime/menu_splash.c
    runtime/fight_scene.c
    runtime/fight_render.c
    runtime/fight_audio.c
    runtime/wav.c
    runtime/fight_select.c
    runtime/fight_stubs.c
    runtime/draw_gl.c
    runtime/gamecode_globals.c
    runtime/gamecode_stubs.c
    runtime/lime_menu.c
    runtime/lime_platform.c
    runtime/lime_app.c
    runtime/lime/meshset.c
    runtime/lime/scene.c
    runtime/lime/events.c
    runtime/lime/skin.c
    runtime/lime/light.c
    runtime/lime/limemath.c
    runtime/lime/pvr.c
    runtime/lime/par.c
    runtime/lime/png.c
    decomp/gamecode/logic/mk3.c
    ${UMK3_MENU_GAMECODE}
    ${UMK3_MENU_LIME}
)
target_include_directories(umk3-test PRIVATE runtime decomp/lime)
target_compile_definitions(umk3-test PRIVATE UMK3_REAL_GL UMK3_SHELL UMK3_HAVE_MK3)

# win32 needs nothing installed and is what the vertical slice was built on;
# sdl2 is the portable path. Both implement platform.h and nothing else.
if(WIN32)
    set(UMK3_BACKEND "win32" CACHE STRING "Platform backend: win32 or sdl2")
elseif(EMSCRIPTEN)
    set(UMK3_BACKEND "sdl2" CACHE STRING "Platform backend: sdl2")
else()
    set(UMK3_BACKEND "sdl2" CACHE STRING "Platform backend: win32 or sdl2")
endif()
set_property(CACHE UMK3_BACKEND PROPERTY STRINGS win32 sdl2)

if(EMSCRIPTEN)
    if(NOT UMK3_BACKEND STREQUAL "sdl2")
        message(FATAL_ERROR "Emscripten requires UMK3_BACKEND=sdl2.")
    endif()
    configure_file(web/ipa.js ipa.js COPYONLY)
    configure_file(web/ipa_store.js ipa_store.js COPYONLY)
    configure_file(web/index.html index.html COPYONLY)
    target_sources(umk3 PRIVATE runtime/platform/sdl_gl.c)
    target_sources(umk3-menu PRIVATE runtime/platform/sdl_gl.c
                                    runtime/platform/sdl_audio.c)
    target_sources(umk3-fight PRIVATE runtime/platform/sdl_gl.c
                                     runtime/platform/sdl_audio.c)
    target_sources(umk3-test PRIVATE runtime/platform/sdl_gl.c
                                    runtime/platform/sdl_audio.c)
    target_sources(umk3-test PRIVATE runtime/emscripten_entry.c)
    foreach(target umk3 umk3-menu umk3-fight umk3-test)
        target_compile_options(${target} PRIVATE "-sUSE_SDL=2")
        target_compile_options(${target} PRIVATE "-sUSE_SDL_MIXER=2"
                                                  "-sSDL2_MIXER_FORMATS=mp3")
        target_compile_options(${target} PRIVATE -include stddef.h)
        target_compile_definitions(${target} PRIVATE
            _snprintf=snprintf
            _strdup=strdup
        )
        target_link_options(${target} PRIVATE
            "-sUSE_SDL=2"
            "-sUSE_SDL_MIXER=2"
            "-sSDL2_MIXER_FORMATS=mp3"
            "-sLEGACY_GL_EMULATION=1"
            "-sGL_UNSAFE_OPTS=0"
            "-sGL_FFP_ONLY=1"
            "-sASYNCIFY=1"
            "-sINVOKE_RUN=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sMAXIMUM_MEMORY=4294967296"
            "-sFORCE_FILESYSTEM=1"
        )
    endforeach()
    target_link_options(umk3-test PRIVATE
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web/asset_mount.js")
    set_property(TARGET umk3-test APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/web/asset_mount.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/web/cache_bust.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
    set_target_properties(umk3-test PROPERTIES
        SUFFIX ".html"
        OUTPUT_NAME "umk3-browser-v6")
    add_custom_command(TARGET umk3-test POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DHTML_FILE=$<TARGET_FILE:umk3-test>"
            "-DJS_VERSION=20261003-9"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/web/cache_bust.cmake"
        VERBATIM)
elseif(UMK3_BACKEND STREQUAL "win32")
    if(NOT WIN32)
        message(FATAL_ERROR "The win32 backend only builds on Windows. "
                            "Configure with -DUMK3_BACKEND=sdl2.")
    endif()
    # win32_gl_cdecl.c is empty on 64-bit; on i686 it forwards the cdecl GL
    # declarations to opengl32's __stdcall exports (see lime_glapi.h).
    foreach(t umk3 umk3-menu umk3-fight umk3-test)
        target_sources(${t} PRIVATE runtime/platform/win32_gl_cdecl.c)
    endforeach()
    target_sources(umk3 PRIVATE runtime/platform/win32_gl.c)
    target_link_libraries(umk3 PRIVATE opengl32 gdi32)
    target_sources(umk3-menu PRIVATE runtime/platform/win32_gl.c
                                    runtime/platform/win32_audio.c)
    target_link_libraries(umk3-menu PRIVATE opengl32 gdi32 winmm msacm32)
    target_sources(umk3-fight PRIVATE runtime/platform/win32_gl.c
                                     runtime/platform/win32_audio.c)
    target_link_libraries(umk3-fight PRIVATE opengl32 gdi32 winmm msacm32)
    target_sources(umk3-test PRIVATE runtime/platform/win32_gl.c
                                    runtime/platform/win32_audio.c)
    target_link_libraries(umk3-test PRIVATE opengl32 gdi32 winmm msacm32 m)
elseif(UMK3_BACKEND STREQUAL "sdl2")
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
    target_sources(umk3 PRIVATE runtime/platform/sdl_gl.c)
    target_sources(umk3-menu PRIVATE runtime/platform/sdl_gl.c
                                    runtime/platform/sdl_audio.c)
    target_sources(umk3-fight PRIVATE runtime/platform/sdl_gl.c
                                     runtime/platform/sdl_audio.c)
    target_sources(umk3-test PRIVATE runtime/platform/sdl_gl.c
                                    runtime/platform/sdl_audio.c)
    # SDL2_INCLUDE_DIRS is the spelling the Debian/Ubuntu config module uses;
    # the imported target covers the rest.
    foreach(target umk3 umk3-menu umk3-fight umk3-test)
        target_include_directories(${target} PRIVATE ${SDL2_INCLUDE_DIRS})
        target_compile_options(${target} PRIVATE -include stddef.h)
        target_compile_definitions(${target} PRIVATE
            _snprintf=snprintf
            _strdup=strdup
        )
        target_link_libraries(${target} PRIVATE ${SDL2_LIBRARIES}
                                                PkgConfig::SDL2_MIXER
                                                OpenGL::GL m)
    endforeach()
else()
    message(FATAL_ERROR "Unknown UMK3_BACKEND '${UMK3_BACKEND}'.")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(umk3 PRIVATE -Wall -Wextra)
endif()

# The fight engine, with its data tables extracted from the user's own binary
# at build time (tools/logic_tables.py) and verified against it
# (tools/check_logic_tables.py). The tables are game data: they are generated
# into the build directory and never committed.
#
#   cmake -B build32 -G Ninja -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc #         -DUMK3_BINARY=<OUTPUT/armv7/UMK3.armv7 or Payload/UMK3.app/UMK3>
#   cmake --build build32 --target umk3-fight-headless
#   ctest --test-dir build32 -R logic
#
# 32-bit only: the engine's structs and tables hold 32-bit addresses, and the
# generated file refuses to compile anywhere else.
# The launcher: settings, the build from the player's .ipa (launcher/
# build_game.ps1), and Play. Holds no game data, so it builds without one.
if(WIN32)
    add_executable(umk3-launcher WIN32 launcher/launcher.c)
    set_target_properties(umk3-launcher PROPERTIES OUTPUT_NAME UMK3-Launcher)
    target_link_libraries(umk3-launcher PRIVATE comdlg32)
    if(NOT MSVC)
        target_link_options(umk3-launcher PRIVATE -municode)
    endif()
endif()

set(UMK3_BINARY "" CACHE FILEPATH
    "The user's UMK3 binary: OUTPUT/armv7/UMK3.armv7 or Payload/UMK3.app/UMK3")
if(UMK3_BINARY)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(STATUS "umk3-fight-headless needs a 32-bit compiler "
                       "(i686 or wasm32); not built with this one.")
    else()
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        set(UMK3_LOGIC_TABLES ${CMAKE_BINARY_DIR}/logic_tables.c)
        add_custom_command(
            OUTPUT ${UMK3_LOGIC_TABLES}
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/logic_tables.py
                    ${UMK3_BINARY} ${UMK3_LOGIC_TABLES}
                    --report ${CMAKE_BINARY_DIR}/logic_tables_report.txt
            DEPENDS ${CMAKE_SOURCE_DIR}/tools/logic_tables.py
                    ${CMAKE_SOURCE_DIR}/tools/logic_tables.map
                    ${CMAKE_SOURCE_DIR}/tools/macho.py
                    ${UMK3_BINARY}
            COMMENT "Extracting the fight engine's data tables from ${UMK3_BINARY}")

        # The arena names Level_Info points at (tools/level_info.py): game
        # data, generated here like the logic tables, linked into umk3-game.
        set(UMK3_LEVEL_INFO ${CMAKE_BINARY_DIR}/level_info.c)
        add_custom_command(
            OUTPUT ${UMK3_LEVEL_INFO}
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/level_info.py
                    ${UMK3_BINARY} ${UMK3_LEVEL_INFO}
            DEPENDS ${CMAKE_SOURCE_DIR}/tools/level_info.py
                    ${CMAKE_SOURCE_DIR}/tools/logic_tables.py
                    ${CMAKE_SOURCE_DIR}/tools/macho.py
                    ${UMK3_BINARY}
            COMMENT "Extracting Level_Info's arena names from ${UMK3_BINARY}")

        # The special-move playback scripts seq_lookup points at
        # (tools/seq_data.py): game data, generated here like the above.
        set(UMK3_SEQ_DATA ${CMAKE_BINARY_DIR}/seq_data.c)
        add_custom_command(
            OUTPUT ${UMK3_SEQ_DATA}
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/seq_data.py
                    ${UMK3_BINARY} ${UMK3_SEQ_DATA}
            DEPENDS ${CMAKE_SOURCE_DIR}/tools/seq_data.py
                    ${CMAKE_SOURCE_DIR}/tools/logic_tables.py
                    ${CMAKE_SOURCE_DIR}/tools/macho.py
                    ${UMK3_BINARY}
            COMMENT "Extracting the playback scripts from ${UMK3_BINARY}")

        file(GLOB UMK3_LOGIC_SOURCES ${CMAKE_SOURCE_DIR}/decomp/gamecode/logic/*.c)
        add_executable(umk3-fight-headless
            runtime/fight_headless.c
            runtime/fight_runtime.c
            ${UMK3_LOGIC_SOURCES}
            ${UMK3_LOGIC_TABLES}
            ${UMK3_SEQ_DATA})
        set_target_properties(umk3-fight-headless PROPERTIES
            C_STANDARD 11 C_EXTENSIONS ON)
        target_include_directories(umk3-fight-headless PRIVATE
            runtime decomp/gamecode/logic decomp/lime)
        if(NOT MSVC)
            target_link_libraries(umk3-fight-headless PRIVATE m)
        endif()
        # The symbol table of the linked program, for the layout check.
        if(CMAKE_NM)
            add_custom_command(TARGET umk3-fight-headless POST_BUILD
                COMMAND ${CMAKE_NM} $<TARGET_FILE:umk3-fight-headless>
                        > ${CMAKE_BINARY_DIR}/umk3-fight-headless.nm
                VERBATIM)
        endif()

        enable_testing()
        add_test(NAME logic_tables
            COMMAND Python3::Interpreter
                    ${CMAKE_SOURCE_DIR}/tools/check_logic_tables.py ${UMK3_BINARY})
        if(CMAKE_NM)
            add_test(NAME logic_tables_layout
                COMMAND Python3::Interpreter
                        ${CMAKE_SOURCE_DIR}/tools/check_logic_tables.py ${UMK3_BINARY}
                        --nm ${CMAKE_BINARY_DIR}/umk3-fight-headless.nm)
        endif()
        # The whole game: the device's boot (game_main.c: startAppWithOptions,
        # GameCodeInit, then GameCodeMain every tick) with the front end AND
        # the fight engine linked, so TaskFunctionList can hand the menus over
        # to Task_GameInit / Task_GameMain.
        file(GLOB UMK3_GAME_GAMECODE ${CMAKE_SOURCE_DIR}/decomp/gamecode/*.c)
        file(GLOB UMK3_GAME_LIME     ${CMAKE_SOURCE_DIR}/decomp/lime/*.c)
        file(GLOB UMK3_GAME_RUNLIME  ${CMAKE_SOURCE_DIR}/runtime/lime/*.c)
        add_executable(umk3-game
            runtime/game_main.c
            runtime/debug_menu.c
            runtime/settings_menu.c
            runtime/pad_nav.c
            runtime/profiles.c
            runtime/draw_gl.c
            runtime/gamecode_globals.c
            runtime/gamecode_stubs.c
            runtime/lime_menu.c
            runtime/lime_platform.c
            runtime/lime_app.c
            runtime/wav.c
            runtime/fight_runtime.c
            runtime/savestate.c
            ${UMK3_GAME_GAMECODE}
            ${UMK3_GAME_LIME}
            ${UMK3_GAME_RUNLIME}
            ${UMK3_LOGIC_SOURCES}
            ${UMK3_LOGIC_TABLES}
            ${UMK3_LEVEL_INFO}
            ${UMK3_SEQ_DATA})
        set_target_properties(umk3-game PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
        target_include_directories(umk3-game PRIVATE
            runtime decomp/lime decomp/gamecode/logic)
        target_compile_definitions(umk3-game PRIVATE
            UMK3_REAL_GL UMK3_SHELL UMK3_HAVE_MK3)
        # Save states (runtime/savestate.h): every file sees the header; the
        # game's own files keep their globals in .umkstd/.umkstb and their
        # heap in the arena. launcher/build_game.ps1 does the same.
        if(NOT MSVC)
            target_compile_options(umk3-game PRIVATE
                -include ${CMAKE_SOURCE_DIR}/runtime/savestate.h)
            set_source_files_properties(
                runtime/gamecode_globals.c runtime/gamecode_stubs.c
                runtime/lime_menu.c runtime/lime_platform.c runtime/lime_app.c
                runtime/wav.c runtime/fight_runtime.c
                ${UMK3_GAME_GAMECODE} ${UMK3_GAME_LIME} ${UMK3_GAME_RUNLIME}
                ${UMK3_LOGIC_SOURCES} ${UMK3_LOGIC_TABLES} ${UMK3_LEVEL_INFO}
                ${UMK3_SEQ_DATA}
                TARGET_DIRECTORY umk3-game
                PROPERTIES COMPILE_DEFINITIONS UMK3_STATE_FILE)
        endif()
        if(WIN32)
            # 4 GB instead of 2 for the 32-bit exe on 64-bit Windows (see
            # launcher/build_game.ps1)
            if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT MSVC)
                target_link_options(umk3-game PRIVATE -Wl,--large-address-aware)
            endif()
            target_sources(umk3-game PRIVATE
                runtime/platform/win32_gl.c
                runtime/platform/win32_gl_cdecl.c
                runtime/platform/win32_audio.c)
            target_link_libraries(umk3-game PRIVATE opengl32 gdi32 winmm msacm32)
        endif()
        if(NOT MSVC)
            target_link_libraries(umk3-game PRIVATE m)
        endif()

        add_test(NAME logic_headless_runs COMMAND umk3-fight-headless)
        set_tests_properties(logic_headless_runs PROPERTIES
            FAIL_REGULAR_EXPRESSION "abort|ABORT|FAIL")
    endif()
endif()
