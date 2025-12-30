cmake_minimum_required(VERSION 4.1)
project(GLFWVoxel)

set(CMAKE_CXX_STANDARD 20)

# GLFW options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glfw-3.4)

find_package(OpenGL REQUIRED)

# GLAD
add_library(glad thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC thirdparty/glad/include)

# GLM
add_subdirectory(thirdparty/glm)

add_executable(GLFWVoxel main.cpp
        src/Shader.cpp
        src/Shader.h
        src/stb_image.h)

# Remove shader folder before copying
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shader" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/textures" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(GLFWVoxel PRIVATE
        glfw
        glad
        OpenGL::GL
        glm::glm
)

target_include_directories(GLFWVoxel PRIVATE thirdparty)

## Optional: GLM include
#target_include_directories(GLFWVoxel PRIVATE
#        thirdparty/glm
#)
