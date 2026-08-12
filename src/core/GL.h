//
// Created by maxim on 12/08/2026.
//

#ifndef GLFWVOXEL_GL_H
#define GLFWVOXEL_GL_H

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif

#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
#else
    #include <glad/glad.h>
#endif

#endif //GLFWVOXEL_GL_H
