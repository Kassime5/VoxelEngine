//
// Created by maxim on 14/01/2026.
//

#ifndef GLFWVOXEL_RENDERSTATS_H
#define GLFWVOXEL_RENDERSTATS_H

class RenderStats {
public:
    static RenderStats& getInstance() {
        static RenderStats instance;
        return instance;
    }

    // Reset counters at start of frame
    void resetFrame() {
        m_drawCalls = 0;
        m_triangles = 0;
        m_vertices = 0;
        m_chunksRendered = 0;
        m_chunksSkipped = 0;
    }

    // Increment counters
    void addDrawCall() { m_drawCalls++; }
    void addTriangles(int count) { m_triangles += count; }
    void addVertices(int count) { m_vertices += count; }
    void addChunkRendered() { m_chunksRendered++; }
    void addChunkSkipped() { m_chunksSkipped++; }

    // Getters
    int getDrawCalls() const { return m_drawCalls; }
    int getTriangles() const { return m_triangles; }
    int getVertices() const { return m_vertices; }
    int getChunksRendered() const { return m_chunksRendered; }
    int getChunksSkipped() const { return m_chunksSkipped; }

    // Calculate derived stats
    float getAvgTrianglesPerDrawCall() const {
        return m_drawCalls > 0 ? (float)m_triangles / m_drawCalls : 0.0f;
    }

    float getAvgVerticesPerDrawCall() const {
        return m_drawCalls > 0 ? (float)m_vertices / m_drawCalls : 0.0f;
    }

private:
    RenderStats() : m_drawCalls(0), m_triangles(0), m_vertices(0),
                    m_chunksRendered(0), m_chunksSkipped(0) {}

    int m_drawCalls;
    int m_triangles;
    int m_vertices;
    int m_chunksRendered;
    int m_chunksSkipped;
};

#endif //GLFWVOXEL_RENDERSTATS_H