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

#OpenAL
set(OPENAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/AL")
set(OPENAL_LIBRARY "${CMAKE_SOURCE_DIR}/thirdparty/AL/OpenAL32.lib")
set(OPENAL_DLL "${CMAKE_SOURCE_DIR}/thirdparty/AL/OpenAL32.dll")

include_directories(${OPENAL_INCLUDE_DIR})

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
        src/rendering/Profiler.h
        src/debug/RenderStats.h
        src/debug/GPUStats.h
        src/rendering/Skybox.cpp
        src/rendering/Skybox.h
        src/rendering/ShaderManager.cpp
        src/rendering/ShaderManager.h
        src/world/Biome.h
        src/world/WorleyBiome.cpp
        src/world/WorleyBiome.h
        src/core/ImGUIManager.cpp
        src/core/ImGUIManager.h
        src/world/Biome.cpp
        src/game/Player.cpp
        src/game/Player.h
        src/game/SoundManager.cpp
        src/game/SoundManager.h
        src/game/HighlightBox.h
        src/input/InputManager.cpp
        src/input/InputManager.h
        src/input/PlayerController.cpp
        src/input/PlayerController.h)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

# Copy OpenAL32.dll to build directory
add_custom_command(TARGET GLFWVoxel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENAL_DLL}"
        "$<TARGET_FILE_DIR:GLFWVoxel>"
        COMMENT "Copying OpenAL32.dll to build directory"
)

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
        ${OPENAL_LIBRARY}
)

target_link_libraries(imgui PUBLIC glfw glad)


target_include_directories(GLFWVoxel PRIVATE thirdparty)
