cmake_minimum_required(VERSION 3.25)
project(SegaGolfNative LANGUAGES C CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ChihiroPort.cmake")
chihiro_port(SegaGolf golf.xbe SEGAGOLF_SDL_INPUT
    00dd5d68d50104d67e23965c36245a315732d239228e3c8ccf10f5a75b8fab0d)
if(TARGET SegaGolf)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    file(GLOB language_sprites CONFIGURE_DEPENDS assets/media/sprite/spr_ae_*.xst*)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/generated/game/language_tables.h"
        COMMAND "${Python3_EXECUTABLE}" tools/build_localization.py
        DEPENDS tools/build_localization.py tools/build_support.py localization/en.json assets/golf.xbe ${language_sprites}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
    add_custom_target(segagolf_language_tables DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated/game/language_tables.h")
    add_dependencies(SegaGolf segagolf_language_tables)
endif()
option(SEGAGOLF_INPUT_SMOKE "Inject a bounded SDL mouse/keyboard sequence for input validation" OFF)
if(SEGAGOLF_INPUT_SMOKE AND TARGET SegaGolf)
    target_sources(SegaGolf PRIVATE tests/input_smoke.cpp tests/swing_trace.cpp)
    set_property(SOURCE src/game/touch_ui.cpp APPEND PROPERTY COMPILE_DEFINITIONS SEGAGOLF_INPUT_SMOKE=1)
endif()

add_executable(SegaGolfNetworkTests tests/network.cpp src/game/network.cpp
    "${XBE_SDK_DIR}/runtime/core/log.cpp")
target_include_directories(SegaGolfNetworkTests PRIVATE "${XBE_SDK_DIR}/runtime")
target_compile_options(SegaGolfNetworkTests PRIVATE -UNDEBUG)
add_test(NAME SegaGolfNetworkTests COMMAND SegaGolfNetworkTests)

add_executable(SegaGolfTouchTests tests/touch.cpp src/game/touch.cpp
    "${XBE_SDK_DIR}/runtime/core/log.cpp")
target_include_directories(SegaGolfTouchTests PRIVATE src "${XBE_SDK_DIR}/runtime")
target_compile_options(SegaGolfTouchTests PRIVATE -UNDEBUG)
add_test(NAME SegaGolfTouchTests COMMAND SegaGolfTouchTests)

if(TARGET SegaGolf)
add_executable(SegaGolfLanguageTests tests/language.cpp src/game/language.cpp
    "${XBE_SDK_DIR}/runtime/core/log.cpp")
target_include_directories(SegaGolfLanguageTests PRIVATE src generated/game "${XBE_SDK_DIR}/runtime")
target_compile_options(SegaGolfLanguageTests PRIVATE -UNDEBUG)
add_dependencies(SegaGolfLanguageTests segagolf_language_tables)
add_test(NAME SegaGolfLanguageTests COMMAND SegaGolfLanguageTests "${CMAKE_CURRENT_SOURCE_DIR}/assets/golf.xbe")

add_executable(SegaGolfSwingTests tests/swing.cpp src/game/swing.cpp src/game/mouse_sdl.cpp
    generated/game/x86_recomp.6.cpp generated/game/x86_recomp.18.cpp)
target_include_directories(SegaGolfSwingTests PRIVATE src generated/game "${XBE_SDK_DIR}/runtime")
target_compile_definitions(SegaGolfSwingTests PRIVATE SEGAGOLF_SDL_INPUT=1)
target_compile_options(SegaGolfSwingTests PRIVATE -UNDEBUG -O1 -fno-strict-aliasing -ffunction-sections -fdata-sections)
target_link_options(SegaGolfSwingTests PRIVATE -fuse-ld=lld -Wl,--gc-sections)
target_link_libraries(SegaGolfSwingTests PRIVATE SDL3::SDL3)
add_test(NAME SegaGolfSwingTests COMMAND SegaGolfSwingTests)
set_tests_properties(SegaGolfSwingTests PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
add_dependencies(SegaGolfSwingTests recompile_SegaGolf)

add_executable(SegaGolfResourceTests tests/resources.cpp
    "${XBE_SDK_DIR}/runtime/xdk/d3d8.cpp"
    "${XBE_SDK_DIR}/runtime/gpu/nv2a_texture.cpp"
    "${XBE_SDK_DIR}/runtime/gpu/texture.cpp"
    "${XBE_SDK_DIR}/runtime/gpu/vertex.cpp"
    "${XBE_SDK_DIR}/runtime/core/log.cpp")
target_include_directories(SegaGolfResourceTests PRIVATE generated/game
    "${XBE_SDK_DIR}/runtime" "${XBE_SDK_DIR}/runtime/platform/compat")
target_compile_options(SegaGolfResourceTests PRIVATE -UNDEBUG -ffunction-sections -fdata-sections)
target_link_options(SegaGolfResourceTests PRIVATE -fuse-ld=lld -Wl,--gc-sections)
add_test(NAME SegaGolfResourceTests COMMAND SegaGolfResourceTests)
add_dependencies(SegaGolfResourceTests recompile_SegaGolf)
endif()
