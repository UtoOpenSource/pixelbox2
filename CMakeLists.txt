cmake_minimum_required(VERSION 3.12)

project(Pixelbox)

#tmp
set (CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions("-Wall" "-Wextra")
include_directories(${PROJECT_SOURCE_DIR})

# lib Base
file(GLOB Base_SRC CONFIGURE_DEPENDS "base/*.cpp" "base/tests/*.cpp")
add_library(libBase OBJECT ${Base_SRC})
target_include_directories(libBase INTERFACE "base/" "external/")

#lib External
file(GLOB External_SRC CONFIGURE_DEPENDS "external/*.cpp" "external/*.c")
add_library(libExternal OBJECT ${External_SRC})

# lib Engine
file(GLOB Engine_SRC CONFIGURE_DEPENDS "engine/*.cpp" "engine/*/*.cpp")
add_library(libEngine OBJECT ${Engine_SRC})
target_link_libraries(libEngine PUBLIC libBase libExternal)

#lib Backend (SDL)
file(GLOB Backend_SRC CONFIGURE_DEPENDS "back_sdl/*.cpp" "back_sdl/*/*.cpp" "back_sdl/*.c")
add_library(libBackend OBJECT ${Backend_SRC})
target_link_libraries(libBackend PUBLIC libBase)

# SDL 
find_package(SDL2 REQUIRED)
add_library(libSDL INTERFACE)
include_directories(libSDL ${SDL2_INCLUDE_DIRS})
target_link_libraries(libSDL INTERFACE ${SDL2_LIBRARIES})

#lib Game
file(GLOB Game_SRC CONFIGURE_DEPENDS "game/*.cpp" "game/*/*.cpp")
add_library(libGame OBJECT ${Game_SRC})
target_include_directories(libGame PUBLIC "base/" "external/")
target_link_libraries(libGame PUBLIC $<TARGET_OBJECTS:libBase> $<TARGET_OBJECTS:libEngine> $<TARGET_OBJECTS:libBackend> $<TARGET_OBJECTS:libExternal>)

message("${Base_SRC}")

add_executable(pixelbox)
target_link_libraries(pixelbox PUBLIC libGame)
target_link_libraries(pixelbox PUBLIC libSDL GL)
target_include_directories(pixelbox PUBLIC "${PROJECT_BINARY_DIR}")

add_compile_options(-fsanitize=undefined -Og)
add_link_options(-fsanitize=undefined)
