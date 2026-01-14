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

# ImGui
add_library(imgui
        thirdparty/imgui/imgui.cpp
        thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/imgui_tables.cpp
        thirdparty/imgui/backends/imgui_impl_glfw.cpp
        thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(GLFWVoxel main.cpp
        src/rendering/Shader.cpp
        src/rendering/Shader.h
        src/stb_image.h
        src/rendering/Camera.cpp
        src/rendering/Camera.h
        src/world/block.h
        src/rendering/Texture.cpp
        src/rendering/Texture.h
        src/rendering/Mesh.cpp
        src/rendering/Mesh.h
        src/world/Chunk.cpp
        src/world/Chunk.h
        src/world/World.cpp
        src/world/World.h
        src/rendering/TextureAltas.cpp
        src/rendering/TextureAltas.h
        src/core/Window.cpp
        src/core/Window.h
        src/rendering/Profiler.h)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(imgui PUBLIC
        thirdparty/imgui
        thirdparty/imgui/backends
)

target_link_libraries(GLFWVoxel PRIVATE
        glfw
        glad
        OpenGL::GL
        glm::glm
        imgui
)

target_link_libraries(imgui PUBLIC glfw glad)


target_include_directories(GLFWVoxel PRIVATE thirdparty)
