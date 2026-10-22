cmake_minimum_required(VERSION 3.16)
project(umk3 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Platform backend selection:
#   win32 -> native Win32 + WGL + WinMM/ACM
#   sdl2  -> portable SDL2 + SDL2_mixer + OpenGL (default on Linux, macOS)
if(WIN32)
    set(UMK3_BACKEND "win32" CACHE STRING "Platform backend: win32 or sdl2")
else()
    set(UMK3_BACKEND "sdl2" CACHE STRING "Platform backend: win32 or sdl2")
endif()
set_property(CACHE UMK3_BACKEND PROPERTY STRINGS win32 sdl2)

if(UMK3_BACKEND STREQUAL "sdl2")
    find_package(SDL2 QUIET)
    find_package(OpenGL QUIET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_MIXER QUIET IMPORTED_TARGET SDL2_mixer)
    endif()
endif()

# Helper function to attach the active platform backend (win32 or sdl2) to any target
function(umk3_target_link_platform target_name is_full_game)
    if(UMK3_BACKEND STREQUAL "win32")
        if(NOT WIN32)
            message(FATAL_ERROR "The win32 backend only builds on Windows. Configure with -DUMK3_BACKEND=sdl2.")
        endif()
        target_sources(${target_name} PRIVATE
            src/runtime/platform/win32_gl.c
            src/runtime/platform/win32_gl_cdecl.c
            src/runtime/platform/win32_audio.c)
        target_link_libraries(${target_name} PRIVATE opengl32 gdi32 winmm msacm32 ws2_32)
        if(is_full_game)
            enable_language(RC)
            target_sources(${target_name} PRIVATE src/runtime/umk3_game.rc)
        endif()
    elseif(UMK3_BACKEND STREQUAL "sdl2")
        target_sources(${target_name} PRIVATE
            src/runtime/platform/sdl_gl.c
            src/runtime/platform/sdl_audio.c)
        target_include_directories(${target_name} PRIVATE ${SDL2_INCLUDE_DIRS})
        target_compile_options(${target_name} PRIVATE -include stddef.h)
        target_compile_definitions(${target_name} PRIVATE
            _snprintf=snprintf _strdup=strdup)
        if(SDL2_MIXER_FOUND)
            target_link_libraries(${target_name} PRIVATE
                ${SDL2_LIBRARIES} PkgConfig::SDL2_MIXER OpenGL::GL m)
        else()
            message(STATUS "SDL2_mixer not found: building ${target_name} with pure SDL2 fallback")
            target_compile_definitions(${target_name} PRIVATE UMK3_NO_SDL_MIXER=1)
            target_link_libraries(${target_name} PRIVATE
                ${SDL2_LIBRARIES} OpenGL::GL m)
        endif()
    else()
        message(FATAL_ERROR "Unknown UMK3_BACKEND '${UMK3_BACKEND}'.")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Full Native Game Build (from .ipa or extracted Mach-O binary)
# ---------------------------------------------------------------------------
set(UMK3_IPA "" CACHE FILEPATH
    "Path to the user's UMK3 1.2.59 .ipa file (extracts binary, tables, and res/)")
set(UMK3_BINARY "" CACHE FILEPATH
    "The user's UMK3 binary: work/UMK3.armv7 or Payload/UMK3.app/UMK3")
set(UMK3_ASSET_DEST "${CMAKE_BINARY_DIR}" CACHE PATH
    "Directory where res/, aditional textures/, and umk3.ini will be placed when unpacking UMK3_IPA")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(UMK3_IPA)
    if(NOT EXISTS "${UMK3_IPA}")
        message(FATAL_ERROR "UMK3_IPA does not exist: ${UMK3_IPA}")
    endif()
    set(UMK3_EXTRACTED_BIN "${CMAKE_BINARY_DIR}/UMK3.bin")
    message(STATUS "Unpacking UMK3_IPA: ${UMK3_IPA}")
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/build/unpack_ipa.py"
                "${UMK3_IPA}"
                --bin "${UMK3_EXTRACTED_BIN}"
                --dest "${UMK3_ASSET_DEST}"
        RESULT_VARIABLE _ipa_res
    )
    if(NOT _ipa_res EQUAL 0)
        message(FATAL_ERROR "Failed to unpack or validate UMK3_IPA: ${UMK3_IPA}")
    endif()
    set(UMK3_BINARY "${UMK3_EXTRACTED_BIN}" CACHE FILEPATH "Extracted UMK3 binary" FORCE)
endif()

if(UMK3_BINARY)
    set(UMK3_USE_LOW32 OFF)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND WIN32)
            message(STATUS "64-bit Clang detected on Windows: enabling UMK3_LOW32 (__ptr32) mode.")
            set(UMK3_USE_LOW32 ON)
        else()
            message(WARNING
                "umk3-game requires 32-bit pointers (compile with -m32 / i686 toolchain, or 64-bit Clang on Windows with UMK3_LOW32). "
                "Use `python3 build.py <file.ipa>` to configure the right 32-bit or cross compiler automatically.")
        endif()
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 4 OR UMK3_USE_LOW32)
        set(UMK3_BUILD_TOOLS "${CMAKE_SOURCE_DIR}/tools/build")

        set(UMK3_LOGIC_TABLES "${CMAKE_BINARY_DIR}/logic_tables.c")
        add_custom_command(
            OUTPUT "${UMK3_LOGIC_TABLES}"
            COMMAND Python3::Interpreter "${UMK3_BUILD_TOOLS}/logic_tables.py"
                    "${UMK3_BINARY}" "${UMK3_LOGIC_TABLES}"
                    --report "${CMAKE_BINARY_DIR}/logic_tables_report.txt"
            DEPENDS "${UMK3_BUILD_TOOLS}/logic_tables.py"
                    "${UMK3_BUILD_TOOLS}/logic_tables.map"
                    "${UMK3_BUILD_TOOLS}/macho.py"
                    "${UMK3_BINARY}"
            COMMENT "Extracting the fight engine's 229 data tables from ${UMK3_BINARY}")

        set(UMK3_LEVEL_INFO "${CMAKE_BINARY_DIR}/level_info.c")
        add_custom_command(
            OUTPUT "${UMK3_LEVEL_INFO}"
            COMMAND Python3::Interpreter "${UMK3_BUILD_TOOLS}/level_info.py"
                    "${UMK3_BINARY}" "${UMK3_LEVEL_INFO}"
            DEPENDS "${UMK3_BUILD_TOOLS}/level_info.py"
                    "${UMK3_BUILD_TOOLS}/logic_tables.py"
                    "${UMK3_BUILD_TOOLS}/macho.py"
                    "${UMK3_BINARY}"
            COMMENT "Extracting Level_Info's arena names from ${UMK3_BINARY}")

        set(UMK3_SEQ_DATA "${CMAKE_BINARY_DIR}/seq_data.c")
        add_custom_command(
            OUTPUT "${UMK3_SEQ_DATA}"
            COMMAND Python3::Interpreter "${UMK3_BUILD_TOOLS}/seq_data.py"
                    "${UMK3_BINARY}" "${UMK3_SEQ_DATA}"
            DEPENDS "${UMK3_BUILD_TOOLS}/seq_data.py"
                    "${UMK3_BUILD_TOOLS}/logic_tables.py"
                    "${UMK3_BUILD_TOOLS}/macho.py"
                    "${UMK3_BINARY}"
            COMMENT "Extracting the playback scripts from ${UMK3_BINARY}")

        set(UMK3_GAMECODE_GLOBALS "${CMAKE_SOURCE_DIR}/src/runtime/gamecode_globals.c")
        if(UMK3_USE_LOW32)
            set(UMK3_GAMECODE_GLOBALS "${CMAKE_BINARY_DIR}/gamecode_globals64.c")
            add_custom_command(
                OUTPUT "${UMK3_GAMECODE_GLOBALS}"
                COMMAND Python3::Interpreter "${UMK3_BUILD_TOOLS}/low32_static.py"
                        "${CMAKE_SOURCE_DIR}/src/runtime/gamecode_globals.h" --
                        "${CMAKE_SOURCE_DIR}/src/runtime/gamecode_globals.c"
                        "${UMK3_GAMECODE_GLOBALS}" umk3_globals_fixup
                DEPENDS "${UMK3_BUILD_TOOLS}/low32_static.py"
                        "${CMAKE_SOURCE_DIR}/src/runtime/gamecode_globals.h"
                        "${CMAKE_SOURCE_DIR}/src/runtime/gamecode_globals.c"
                COMMENT "Generating 64-bit low32 globals fixup")
        endif()

        file(GLOB UMK3_LOGIC_SOURCES "${CMAKE_SOURCE_DIR}/src/decomp/gamecode/logic/*.c")

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            add_executable(umk3-fight-headless
                src/runtime/fight_headless.c
                src/runtime/fight_runtime.c
                src/runtime/savestate.c
                ${UMK3_LOGIC_SOURCES}
                ${UMK3_LOGIC_TABLES}
                ${UMK3_SEQ_DATA})
            target_include_directories(umk3-fight-headless PRIVATE
                src/runtime src/decomp/gamecode/logic src/decomp/lime)
            target_compile_options(umk3-fight-headless PRIVATE
                -include "${CMAKE_SOURCE_DIR}/src/runtime/savestate.h")
            if(NOT MSVC)
                target_link_libraries(umk3-fight-headless PRIVATE m)
            endif()
            if(CMAKE_NM)
                add_custom_command(TARGET umk3-fight-headless POST_BUILD
                    COMMAND ${CMAKE_NM} $<TARGET_FILE:umk3-fight-headless>
                            > "${CMAKE_BINARY_DIR}/umk3-fight-headless.nm"
                    VERBATIM)
            endif()

            enable_testing()
            add_test(NAME logic_tables
                COMMAND Python3::Interpreter
                        "${UMK3_BUILD_TOOLS}/check_logic_tables.py" "${UMK3_BINARY}"
                        --logic-c "${UMK3_LOGIC_TABLES}")
            if(CMAKE_NM)
                add_test(NAME logic_tables_layout
                    COMMAND Python3::Interpreter
                            "${UMK3_BUILD_TOOLS}/check_logic_tables.py" "${UMK3_BINARY}"
                            --logic-c "${UMK3_LOGIC_TABLES}"
                            --nm "${CMAKE_BINARY_DIR}/umk3-fight-headless.nm")
            endif()
            if(NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
                add_test(NAME logic_headless_runs COMMAND umk3-fight-headless)
                set_tests_properties(logic_headless_runs PROPERTIES
                    FAIL_REGULAR_EXPRESSION "abort|ABORT|FAIL")
            endif()
        endif()

        if(UMK3_BACKEND STREQUAL "sdl2" AND NOT EMSCRIPTEN AND NOT (SDL2_FOUND AND OpenGL_FOUND))
            message(STATUS "SDL2 or OpenGL not found for target architecture (${CMAKE_SIZEOF_VOID_P}-byte pointers): skipping umk3-game (umk3-fight-headless is enabled)")
        else()
            file(GLOB UMK3_GAME_GAMECODE "${CMAKE_SOURCE_DIR}/src/decomp/gamecode/*.c")
            file(GLOB UMK3_GAME_LIME     "${CMAKE_SOURCE_DIR}/src/decomp/lime/*.c")
            file(GLOB UMK3_GAME_RUNLIME  "${CMAKE_SOURCE_DIR}/src/runtime/lime/*.c")

            add_executable(umk3-game
                src/runtime/game_main.c
                src/runtime/debug_menu.c
                src/runtime/settings_menu.c
                src/runtime/pad_nav.c
                src/runtime/profiles.c
                src/runtime/draw_gl.c
                ${UMK3_GAMECODE_GLOBALS}
                src/runtime/gamecode_stubs.c
                src/runtime/mp_net.c
                src/runtime/online_ui.c
                src/runtime/lime_menu.c
                src/runtime/lime_platform.c
                src/runtime/lime_app.c
                src/runtime/wav.c
                src/runtime/fight_runtime.c
                src/runtime/savestate.c
                src/runtime/rollback_probe.c
                ${UMK3_GAME_GAMECODE}
                ${UMK3_GAME_LIME}
                ${UMK3_GAME_RUNLIME}
                ${UMK3_LOGIC_SOURCES}
                ${UMK3_LOGIC_TABLES}
                ${UMK3_LEVEL_INFO}
                ${UMK3_SEQ_DATA})

            target_include_directories(umk3-game PRIVATE
                src/runtime src/decomp/lime src/decomp/gamecode/logic)
            target_compile_definitions(umk3-game PRIVATE
                UMK3_REAL_GL UMK3_SHELL UMK3_HAVE_MK3)

            if(NOT MSVC)
                target_compile_options(umk3-game PRIVATE
                    -include "${CMAKE_SOURCE_DIR}/src/runtime/savestate.h")
                if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                    set_source_files_properties(
                        ${UMK3_GAMECODE_GLOBALS} src/runtime/gamecode_stubs.c
                        src/runtime/lime_menu.c src/runtime/lime_platform.c src/runtime/lime_app.c
                        src/runtime/wav.c src/runtime/fight_runtime.c
                        ${UMK3_GAME_GAMECODE} ${UMK3_GAME_LIME} ${UMK3_GAME_RUNLIME}
                        ${UMK3_LOGIC_SOURCES} ${UMK3_LOGIC_TABLES} ${UMK3_LEVEL_INFO}
                        ${UMK3_SEQ_DATA}
                        TARGET_DIRECTORY umk3-game
                        PROPERTIES COMPILE_DEFINITIONS UMK3_STATE_FILE)
                endif()
            endif()

            if(UMK3_USE_LOW32)
                target_compile_definitions(umk3-game PRIVATE UMK3_LOW32)
                target_compile_options(umk3-game PRIVATE
                    -fms-extensions -O2 -fno-strict-aliasing -fno-jump-tables)
                target_link_options(umk3-game PRIVATE
                    -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase -Wl,--disable-high-entropy-va)
            elseif(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT MSVC)
                target_link_options(umk3-game PRIVATE -Wl,--large-address-aware)
            endif()

            umk3_target_link_platform(umk3-game TRUE)
            if(NOT MSVC)
                target_link_libraries(umk3-game PRIVATE m)
            endif()
        endif()
    endif()
endif()
