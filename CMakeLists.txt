cmake_minimum_required(VERSION 4.1)
project(GLFWVoxel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten emits the page, the JS glue, the wasm and the asset bundle from one link
# step, so the target's "executable" is an .html file.
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# -- Third-party --

# GLFW - Emscripten ships its own implementation of the GLFW 3 API on top of the DOM
if(NOT EMSCRIPTEN)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(thirdparty/glfw-3.4)
endif()

# GLM -- header only, same on both targets
add_subdirectory(thirdparty/glm)

# GLAD -- desktop only. On the web the GL symbols come from the toolchain with no loader
# involved; see src/core/GL.h.
if(NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    add_library(glad thirdparty/glad/src/glad.c)
    target_include_directories(glad PUBLIC thirdparty/glad/include)
    target_link_libraries(glad PUBLIC OpenGL::GL)
endif()

# OpenAL -- desktop uses the prebuilt import library in thirdparty/. Emscripten has its
# own OpenAL implementation backed by Web Audio, linked in with -lopenal below.
if(NOT EMSCRIPTEN)
    set(OPENAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/AL")
    add_library(openal SHARED IMPORTED)
    set_target_properties(openal PROPERTIES
            IMPORTED_LOCATION "${OPENAL_DIR}/OpenAL32.dll"
            IMPORTED_IMPLIB "${OPENAL_DIR}/OpenAL32.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENAL_DIR}"
    )
endif()

# ImGui
add_library(imgui
        thirdparty/imgui/imgui.cpp
        thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_tables.cpp
        thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/backends/imgui_impl_glfw.cpp
        thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
        thirdparty/imgui
        thirdparty/imgui/backends
)
if(NOT EMSCRIPTEN)
    target_link_libraries(imgui PUBLIC glfw glad)
endif()

# -- GLFWVoxel --
# The web build has no pthreads, so chunk generation and meshing run on the main thread
if(EMSCRIPTEN)
    set(GLFWVOXEL_SINGLE_THREADED_DEFAULT ON)
else()
    set(GLFWVOXEL_SINGLE_THREADED_DEFAULT OFF)
endif()
option(GLFWVOXEL_SINGLE_THREADED
        "Run chunk generation and meshing on the main thread instead of worker pools"
        ${GLFWVOXEL_SINGLE_THREADED_DEFAULT})

file(GLOB_RECURSE GLFWVOXEL_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(GLFWVoxel main.cpp ${GLFWVOXEL_SOURCES})

target_include_directories(GLFWVoxel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        thirdparty
)

target_link_libraries(GLFWVoxel PRIVATE
        glm::glm
        imgui
)

if(GLFWVOXEL_SINGLE_THREADED)
    target_compile_definitions(GLFWVoxel PRIVATE GLFWVOXEL_SINGLE_THREADED)
endif()

if(EMSCRIPTEN)
    target_link_options(GLFWVoxel PRIVATE
            "-sUSE_GLFW=3"
            "-lopenal"

            # WebGL2 only -- the shaders are GLSL ES 3.00 and there is no WebGL1 fallback.
            "-sMIN_WEBGL_VERSION=2"
            "-sMAX_WEBGL_VERSION=2"
            # Fills in the ES3 entry points the engine expects. Can likely be dropped
            # later to shrink the binary, since everything already goes through VBOs.
            "-sFULL_ES3=1"

            # A voxel world chews through memory, and growth is measurably slower than a
            # fixed heap, so take it all up front.
            "-sALLOW_MEMORY_GROWTH=0"
            "-sINITIAL_MEMORY=768MB"
            # Emscripten's default 64KB stack is not enough for the meshing code.
            "-sSTACK_SIZE=4MB"

            # emscripten_set_main_loop returns immediately; the runtime has to stay alive.
            "-sEXIT_RUNTIME=0"
            "-sASSERTIONS=1"

            # Baked into a .data bundle. The runtime CWD is /, so the engine's relative
            # "assets/..." paths resolve unchanged.
            "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    )
else()
    target_link_libraries(GLFWVoxel PRIVATE glfw glad openal)

    # Configure-time copy: edited assets only reach the build directory on the next CMake
    # re-run, not on the next build.
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

    add_custom_command(TARGET GLFWVoxel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:openal>"
            "$<TARGET_FILE_DIR:GLFWVoxel>"
            COMMENT "Copying OpenAL32.dll to build directory"
    )
endif()
