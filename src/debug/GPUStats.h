//
// Created by maxim on 14/01/2026.
//

#ifndef GLFWVOXEL_GPUSTATS_H
#define GLFWVOXEL_GPUSTATS_H

#include <glad/glad.h>

class GPUStats {
public:
    static GPUStats& getInstance() {
        static GPUStats instance;
        return instance;
    }

    void init() {
        glGenQueries(1, &m_primitivesQuery);
    }

    void cleanup() {
        glDeleteQueries(1, &m_primitivesQuery);
    }

    void beginFrame() {
        glBeginQuery(GL_PRIMITIVES_GENERATED, m_primitivesQuery);
    }

    void endFrame() {
        glEndQuery(GL_PRIMITIVES_GENERATED);

        // Get results (might stall if not ready)
        GLuint primitives;
        glGetQueryObjectuiv(m_primitivesQuery, GL_QUERY_RESULT, &primitives);
        m_trianglesRendered = primitives;
    }

    int getTrianglesRendered() const {
        return m_trianglesRendered;
    }

    // Get GPU memory info (NVIDIA only)
    void getGPUMemoryInfo(int& totalMB, int& availableMB) {
#ifdef GL_NVX_gpu_memory_info
        GLint total, available;
        glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total);
        glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &available);
        totalMB = total / 1024;
        availableMB = available / 1024;
#else
        totalMB = -1;
        availableMB = -1;
#endif
    }

private:
    GPUStats() : m_primitivesQuery(0), m_trianglesRendered(0) {}

    GLuint m_primitivesQuery;
    int m_trianglesRendered;
};

#endif //GLFWVOXEL_GPUSTATS_H