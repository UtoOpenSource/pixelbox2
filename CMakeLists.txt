cmake_minimum_required(VERSION 3.12)

project(Pixelbox)

#tmp
set (CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions("-Wall" "-Wextra")
include_directories(${PROJECT_SOURCE_DIR})

# SDL 
find_package(SDL2 REQUIRED)
add_library(libSDL INTERFACE)
include_directories(libSDL ${SDL2_INCLUDE_DIRS})
target_link_libraries(libSDL INTERFACE ${SDL2_LIBRARIES})

#lib Game
file(GLOB Game_SRC CONFIGURE_DEPENDS "client/*.cpp" "client/*/*.cpp" "external/*.cpp" "external/*.c" "engine/*.cpp" "engine/*/*.cpp")
add_library(libGame OBJECT ${Game_SRC})
target_include_directories(libGame PUBLIC "external/" "engine/")
target_link_libraries(libGame PUBLIC)

message("${Base_SRC}")

add_executable(pixelbox)
target_link_libraries(pixelbox PUBLIC libGame)
target_link_libraries(pixelbox PUBLIC libSDL GL)
target_include_directories(pixelbox PUBLIC "${PROJECT_BINARY_DIR}")

add_compile_options(-fsanitize=undefined -Og -g -fno-exceptions)
add_link_options(-fsanitize=undefined)
