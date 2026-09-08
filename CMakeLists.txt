cmake_minimum_required(VERSION 3.16)
project(VoxelWorld LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)
add_executable(VoxelWorld
    src/main.cpp
    src/Block.cpp
    src/Chunk.cpp
    src/World.cpp
    src/Player.cpp
    src/Noise.cpp
)
target_include_directories(VoxelWorld PRIVATE src)
target_link_libraries(VoxelWorld PRIVATE raylib)
if (WIN32)
    # nothing extra needed
elseif (APPLE)
    target_link_libraries(VoxelWorld PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
else()
    target_link_libraries(VoxelWorld PRIVATE GL m pthread dl rt X11)
endif()
# Copy assets if any
add_custom_command(TARGET VoxelWorld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:VoxelWorld>/assets
)