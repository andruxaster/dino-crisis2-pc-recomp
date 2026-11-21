# Dino Crisis 2 recompilation: the standalone builds (Linux, Windows).
#   cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build/cmake -j
# Windows: Visual Studio (SDL2 from SDL2-devel-*-VC, -DSDL2_DIR=<it>/cmake) or
# mingw-w64 (-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
# -DSDL2_DIR=<SDL2-devel-*-mingw>/x86_64-w64-mingw32/lib/cmake/SDL2).
# Targets: dinocrisis2 (the game in a window: SDL2 + OpenGL), dc2 (headless,
# prints a trace), smoke (rooms and scripts on the memory image; not MSVC).
# The hook DLL and the harness stay on the Makefile (i686 mingw + wine).
cmake_minimum_required(VERSION 3.16)
project(dino_crisis2_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB GAME_SRC CONFIGURE_DEPENDS src/game/*.c src/pc/*.c src/data/*.c)
list(APPEND GAME_SRC src/hook/hooks.c)

add_library(dc2game STATIC ${GAME_SRC})
target_include_directories(dc2game PUBLIC include)
if(MSVC)
  target_compile_options(dc2game PUBLIC /W3 /wd4244 /wd4267 /wd4146 /wd4018 /fp:precise)
  target_compile_definitions(dc2game PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
  # as the Makefile: the game's float maths in single precision, aliasing through U32() & co.
  target_compile_options(dc2game PUBLIC -Wall -Wextra -Wno-unused-parameter
                         -fexcess-precision=standard -fno-strict-aliasing)
endif()
if(NOT WIN32)
  target_link_libraries(dc2game PUBLIC m)
elseif(NOT MSVC)
  # (mingw's intrin-impl.h trips -Warray-bounds at -O2 once windows.h is in)
  set_source_files_properties(src/game/task.c src/port/dc2.c PROPERTIES COMPILE_OPTIONS -Wno-array-bounds)
endif()

# headless: the game's run as a trace
add_executable(dc2 src/port/dc2.c)
target_link_libraries(dc2 PRIVATE dc2game)

if(NOT MSVC)
  add_executable(smoke src/port/smoke.c)
  target_link_libraries(smoke PRIVATE dc2game)
  if(WIN32)
    target_link_options(smoke PRIVATE -static-libgcc)
  endif()
endif()

# the game in a window
find_package(SDL2 CONFIG)
if(NOT SDL2_FOUND)
  find_package(SDL2 MODULE)
endif()
find_package(OpenGL REQUIRED)
if(TARGET SDL2::SDL2)
  add_executable(dinocrisis2 src/port/dc2.c src/port/render_gl.c)
  target_compile_definitions(dinocrisis2 PRIVATE DC2_GL)
  target_link_libraries(dinocrisis2 PRIVATE dc2game OpenGL::GL)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(dinocrisis2 PRIVATE SDL2::SDL2main)
  endif()
  target_link_libraries(dinocrisis2 PRIVATE SDL2::SDL2)
  if(WIN32)
    set_target_properties(dinocrisis2 PROPERTIES WIN32_EXECUTABLE ON)
    # SDL2.dll beside the game
    add_custom_command(TARGET dinocrisis2 POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:dinocrisis2>)
    if(NOT MSVC)
      target_link_options(dinocrisis2 PRIVATE -static-libgcc)
    endif()
  endif()
  add_custom_command(TARGET dinocrisis2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/config.ini.template $<TARGET_FILE_DIR:dinocrisis2>/config.ini.template)
  install(TARGETS dinocrisis2 RUNTIME DESTINATION .)
  install(FILES config.ini.template DESTINATION . RENAME config.ini)
  if(WIN32)
    install(FILES $<TARGET_FILE:SDL2::SDL2> DESTINATION .)
  endif()
else()
  message(WARNING "SDL2 not found: only dc2 (headless) is built")
endif()
if(WIN32 AND NOT MSVC)
  target_link_options(dc2 PRIVATE -static-libgcc)
endif()
