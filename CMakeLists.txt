cmake_minimum_required(VERSION 3.12)

project(Pixelbox)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions("-Wall" "-Wextra")
include_directories(${PROJECT_SOURCE_DIR})

# lib Base
file(GLOB Base_SRC CONFIGURE_DEPENDS "base/*.cpp")
add_library(libBase ${Base_SRC})
target_include_directories(libBase INTERFACE "${PROJECT_BINARY_DIR}/base")

#lib External
file(GLOB External_SRC CONFIGURE_DEPENDS "external/*.cpp" "external/*.c")
add_library(libExternal ${External_SRC})

# lib Engine
file(GLOB Engine_SRC CONFIGURE_DEPENDS "engine/*.cpp" "engine/*/*.cpp")
add_library(libEngine ${Engine_SRC})
target_link_libraries(libEngine PUBLIC libBase libExternal)

#lib Graphics
file(GLOB Graphics_SRC CONFIGURE_DEPENDS "game/*.cpp" "game/*/*.cpp")
add_library(libGraphics ${Graphics_SRC})
target_link_libraries(libGraphics PUBLIC libBase)

#lib Game
file(GLOB Game_SRC CONFIGURE_DEPENDS "game/*.cpp" "game/*/*.cpp")
add_library(libGame ${Game_SRC})
target_link_libraries(libGame PUBLIC libBase libEngine libGraphics)

message("${Base_SRC}")

add_executable(pixelbox "main.cpp")
target_link_libraries(pixelbox PUBLIC libGame raylib)
target_include_directories(pixelbox PUBLIC "${PROJECT_BINARY_DIR}")