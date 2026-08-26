cmake_minimum_required(VERSION 3.20)
project(GLFWVoxel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
            IMPORTED_IMPLIB "${OPENAL_DIR}/libOpenAL32.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENAL_DIR}"
    )
endif()

# ImGui -- desktop only. The web build's debug UI is the page's own DOM panel, fed by
# Engine::publishWebStats, so the library and its two backends are left out of the wasm.
if(NOT EMSCRIPTEN)
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

target_link_libraries(GLFWVoxel PRIVATE glm::glm)

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

            # The raw exports take numbers only; the seed box hands the engine a string.
            "-sEXPORTED_RUNTIME_METHODS=ccall"

            # Baked into a .data bundle. The runtime CWD is /, so the engine's relative
            # "assets/..." paths resolve unchanged.
            "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"

            # Replaces Emscripten's default page. Pointer lock and audio both need a real
            # user gesture, so the shell's click-to-play overlay is load-bearing, not decor.
            "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )

    # Otherwise editing the shell does not relink.
    set_property(TARGET GLFWVoxel APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
else()
    target_link_libraries(GLFWVoxel PRIVATE glfw glad openal imgui)

    # The assets get their own graph node rather than a POST_BUILD command, which would be
    # skipped on an asset-only edit since the target is already linked. Hashes before
    # writing, and never prunes -- files deleted from the source tree linger until cleaned.
    file(GLOB_RECURSE GLFWVOXEL_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
    set(GLFWVOXEL_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/assets.stamp")

    add_custom_command(OUTPUT "${GLFWVOXEL_ASSET_STAMP}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:GLFWVoxel>/assets"
            COMMAND ${CMAKE_COMMAND} -E touch "${GLFWVOXEL_ASSET_STAMP}"
            DEPENDS ${GLFWVOXEL_ASSETS}
            COMMENT "Syncing assets to build directory"
    )

    add_custom_target(GLFWVoxelAssets DEPENDS "${GLFWVOXEL_ASSET_STAMP}")
    add_dependencies(GLFWVoxel GLFWVoxelAssets)

    add_custom_command(TARGET GLFWVoxel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:openal>"
            "$<TARGET_FILE_DIR:GLFWVoxel>"
            COMMENT "Copying OpenAL32.dll to build directory"
    )
endif()
