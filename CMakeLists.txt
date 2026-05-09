cmake_minimum_required(VERSION 3.16)
project(terrain_generator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)

add_library(glad STATIC src/glad.c)
target_include_directories(glad PUBLIC include)

add_executable(terrain
    src/main.cpp
    src/camera.cpp
    src/terrain_mesh.cpp
)

target_include_directories(terrain PRIVATE include)

target_link_libraries(terrain
    PRIVATE
        glad
        OpenGL::GL
        glfw
        glm::glm
)

add_custom_command(TARGET terrain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:terrain>/shaders
)
