//
// Created by maxim on 18/01/2026.
//

#include "ImGUIManager.h"

#include "src/game/Player.h"


ImGUIManager::ImGUIManager(World* _world, Camera* _camera, int* _renderDistance, Player* _player)
{
    world = _world;
    camera = _camera;
    renderDistance = _renderDistance;
    player = _player;
    entityManager = world->getEntityManager();
}

ImGUIManager::~ImGUIManager() {}

void ImGUIManager::drawImGUIElements(float deltaTime)
{
    // ImGUI Rendering
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main debug window
    ImGui::Begin("Debug Info");

    // Performance stats
    ImGui::SeparatorText("Performance");
    ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
    ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);

    // Rendering stats
    ImGui::SeparatorText("Rendering");
    auto& stats = RenderStats::getInstance();
    ImGui::Text("Draw Calls: %d", stats.getDrawCalls());
    ImGui::Text("Triangles: %s", formatNumber(stats.getTriangles()).c_str());
    ImGui::Text("Vertices: %s", formatNumber(stats.getVertices()).c_str());
    ImGui::Text("Avg Tri/Draw: %.1f", stats.getAvgTrianglesPerDrawCall());

    // Chunk stats
    ImGui::SeparatorText("Chunks");
    ImGui::Text("Rendered: %d", stats.getChunksRendered());
    ImGui::Text("Skipped: %d", stats.getChunksSkipped());
    ImGui::Text("Culled: %d", stats.getChunksCulled());
    ImGui::Text("Total Loaded: %d", world->getLoadedChunkCount());

    // More world stuff
    ImGui::SeparatorText("World");
    ImGui::Text("Entity count: %d", entityManager->getEntityCount());

    // Player stats
    ImGui::SeparatorText("Player");
    ImGui::Text("XYZ: %.1f, %.1f, %.1f", camera->Position.x, camera->Position.y, camera->Position.z);
    ImGui::Text("Facing: %s", camera->facingCardinalDirection().c_str());
    ImGui::Text("Current Biome: %s", world->getCurrentPlayerBiome(camera->Position.x, camera->Position.z)->getName().c_str());

    // Memory estimate
    ImGui::SeparatorText("Memory (Estimate)");
    float vertexMemoryMB = (stats.getVertices() * sizeof(Vertex)) / (1024.0f * 1024.0f);
    float indexMemoryMB = (stats.getTriangles() * 3 * sizeof(unsigned int)) / (1024.0f * 1024.0f);
    ImGui::Text("Vertex Data: %.2f MB", vertexMemoryMB);
    ImGui::Text("Index Data: %.2f MB", indexMemoryMB);
    ImGui::Text("Total GPU: %.2f MB", vertexMemoryMB + indexMemoryMB);

    // Settings
    ImGui::SeparatorText("Settings");
    if (ImGui::SliderInt("Render Distance", renderDistance, 2, 32)) {
        world->setRenderDistance(*renderDistance);
    }

    ImGui::Checkbox("Wireframe", &wireframe);
    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // ---
}

std::string ImGUIManager::formatNumber(int number) {
    std::string str = std::to_string(number);
    int insertPosition = str.length() - 3;
    while (insertPosition > 0) {
        str.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return str;
}

