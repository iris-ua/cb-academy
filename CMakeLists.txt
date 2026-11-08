cmake_minimum_required(VERSION 3.20)
project(ciberrato_jr C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# raylib/glfw still declare old cmake minimums
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

# --- dependencies -----------------------------------------------------------
# SOURCE_SUBDIR points at a missing dir so FetchContent only downloads (we build the sources ourselves).
FetchContent_Declare(cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG v1.7.18 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_Declare(lua
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG v5.4.7 GIT_SHALLOW TRUE SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(cjson lua)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5 GIT_SHALLOW TRUE
  PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/patch_raylib.cmake)

# --- Lua 5.4 -----------------------------------------------------------------
set(LUA_SRC lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump
  lfunc lgc linit liolib llex lmathlib lmem loadlib lobject lopcodes loslib
  lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio)
list(TRANSFORM LUA_SRC PREPEND ${lua_SOURCE_DIR}/)
list(TRANSFORM LUA_SRC APPEND .c)
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
if(UNIX AND NOT EMSCRIPTEN)
  target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
endif()

# --- simulation core (no graphics) -------------------------------------------
add_library(sim STATIC
  src/sim/rng.c src/sim/geom.c src/sim/sim.c src/sim/level.c
  ${cjson_SOURCE_DIR}/cJSON.c)
target_include_directories(sim PUBLIC src PRIVATE ${cjson_SOURCE_DIR})
if(NOT MSVC)
  target_link_libraries(sim PUBLIC m)
  set_source_files_properties(src/sim/rng.c src/sim/geom.c src/sim/sim.c src/sim/level.c
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic")
endif()

# --- robot scripts (Lua, no graphics) ----------------------------------------
add_library(bot STATIC src/bot/bot.c)
target_link_libraries(bot PUBLIC sim lua)
if(NOT MSVC)
  target_compile_options(bot PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- app ---------------------------------------------------------------------
if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()
FetchContent_MakeAvailable(raylib)

add_executable(ciberrato src/main.c
  src/gfx/view.c src/gfx/scene.c src/gfx/fx.c src/gfx/hud.c src/gfx/text.c)
target_include_directories(ciberrato PRIVATE src)
target_link_libraries(ciberrato PRIVATE bot raylib)
if(NOT MSVC)
  target_compile_options(ciberrato PRIVATE -Wall -Wextra)
endif()

if(EMSCRIPTEN)
  set_target_properties(ciberrato PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(ciberrato PRIVATE
    -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1mb
    -sEXPORTED_RUNTIME_METHODS=ccall,FS,UTF8ToString
    --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/levels@/levels"
    "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets/fonts@/assets/fonts")
  set_property(TARGET ciberrato APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/web/shell.html)
  # page files served next to index.html (copied on every build)
  add_custom_target(web_page ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/web/app.js ${CMAKE_SOURCE_DIR}/web/app.css ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/web/vendor ${CMAKE_BINARY_DIR}/vendor
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/fonts ${CMAKE_BINARY_DIR}/fonts)
else()
  # run from build dir: levels/ and assets/ next to the binary
  add_custom_command(TARGET ciberrato POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/levels $<TARGET_FILE_DIR:ciberrato>/levels
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:ciberrato>/assets)

  enable_testing()
  add_executable(test_sim tests/test_sim.c)
  target_link_libraries(test_sim PRIVATE sim)
  target_compile_definitions(test_sim PRIVATE LEVELS_DIR="${CMAKE_SOURCE_DIR}/levels")
  add_test(NAME sim COMMAND test_sim)
  add_executable(test_bot tests/test_bot.c)
  target_link_libraries(test_bot PRIVATE bot)
  add_test(NAME bot COMMAND test_bot)
endif()
