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
        m_shadowPass = false;
        m_drawCalls = 0;
        m_triangles = 0;
        m_vertices = 0;
        m_chunksRendered = 0;
        m_chunksSkipped = 0;
        m_chunksCulled = 0;
        m_shadowDrawCalls = 0;
        m_shadowTriangles = 0;
        m_shadowChunks = 0;
    }

    void setShadowPass(bool active) { m_shadowPass = active; }

    // Increment counters
    void addDrawCall() { (m_shadowPass ? m_shadowDrawCalls : m_drawCalls)++; }
    void addTriangles(int count) { (m_shadowPass ? m_shadowTriangles : m_triangles) += count; }
    void addVertices(int count) { if (!m_shadowPass) m_vertices += count; }
    void addChunkRendered() { (m_shadowPass ? m_shadowChunks : m_chunksRendered)++; }
    void addChunkSkipped() { m_chunksSkipped++; }
    void addChunkCulled() { if (!m_shadowPass) m_chunksCulled++; }

    // Getters
    int getDrawCalls() const { return m_drawCalls; }
    int getTriangles() const { return m_triangles; }
    int getVertices() const { return m_vertices; }
    int getChunksRendered() const { return m_chunksRendered; }
    int getChunksSkipped() const { return m_chunksSkipped; }
    int getChunksCulled() const { return m_chunksCulled; }
    int getShadowDrawCalls() const { return m_shadowDrawCalls; }
    int getShadowTriangles() const { return m_shadowTriangles; }
    int getShadowChunks() const { return m_shadowChunks; }

    // Calculate derived stats
    float getAvgTrianglesPerDrawCall() const {
        return m_drawCalls > 0 ? (float)m_triangles / m_drawCalls : 0.0f;
    }

    float getAvgVerticesPerDrawCall() const {
        return m_drawCalls > 0 ? (float)m_vertices / m_drawCalls : 0.0f;
    }

private:
    RenderStats() : m_drawCalls(0), m_triangles(0), m_vertices(0),
                    m_chunksRendered(0), m_chunksSkipped(0), m_chunksCulled(0) {}

    bool m_shadowPass = false;
    int m_drawCalls;
    int m_triangles;
    int m_vertices;
    int m_chunksRendered;
    int m_chunksSkipped;
    int m_chunksCulled;
    int m_shadowDrawCalls = 0;
    int m_shadowTriangles = 0;
    int m_shadowChunks = 0;
};

#endif //GLFWVOXEL_RENDERSTATS_H