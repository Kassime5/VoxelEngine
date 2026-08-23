#ifndef __EMSCRIPTEN__
// ImGui is desktop-only

//
// Created by maxim on 18/01/2026.
//

#include "ImGUIManager.h"
#include "src/core/GL.h"

#include "src/game/Player.h"
#include "src/input/InputManager.h"


ImGUIManager::ImGUIManager(World& _world, Camera& _camera, int& _renderDistance, Player& _player,
                           DayCycle& _dayCycle, int& _fpsLimit) :
    world(_world), camera(_camera),
    player(_player), entityManager(*_world.getEntityManager()),
    dayCycle(_dayCycle),
    renderDistance(_renderDistance),
    fpsLimit(_fpsLimit)
{}

ImGUIManager::~ImGUIManager() {}

void ImGUIManager::drawImGUIElements(float deltaTime)
{
    // make sure ImGUI cannot be interacted with
    ImGuiIO& io = ImGui::GetIO();
    if (InputManager::getInstance().isCursorVisible()) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    }

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
    ImGui::Text("Total Loaded: %d", world.getLoadedChunkCount());

    // More world stuff
    ImGui::SeparatorText("World");
    ImGui::Text("Entity count: %d", entityManager.getEntityCount());

    // Sky
    ImGui::SeparatorText("Sky");
    ImGui::Text("Time: %s", dayCycle.getClockString().c_str());
    ImGui::Text("Sun intensity: %.2f", dayCycle.getSunIntensity());
    const glm::vec3 sun = dayCycle.getSunDirection();
    ImGui::Text("Sun dir: %.2f, %.2f, %.2f", sun.x, sun.y, sun.z);

    float dayLength = dayCycle.getDayLength();
    if (ImGui::SliderFloat("Day length (s)", &dayLength, 5.0f, 600.0f, "%.0f")) {
        dayCycle.setDayLength(dayLength);
    }

    float timeOfDay = dayCycle.getTimeOfDay();
    if (ImGui::SliderFloat("Time of day", &timeOfDay, 0.0f, 1.0f, "%.3f")) {
        dayCycle.setTimeOfDay(timeOfDay);
    }

    // Player stats
    ImGui::SeparatorText("Player");
    ImGui::Text("XYZ: %.1f, %.1f, %.1f", camera.Position.x, camera.Position.y, camera.Position.z);
    ImGui::Text("Facing: %s", camera.facingCardinalDirection().c_str());
    ImGui::Text("Current Biome: %s", world.getCurrentPlayerBiome(camera.Position.x, camera.Position.z)->getName().c_str());

    // Memory estimate
    ImGui::SeparatorText("Memory (Estimate)");
    float vertexMemoryMB = (stats.getVertices() * sizeof(Vertex)) / (1024.0f * 1024.0f);
    float indexMemoryMB = (stats.getTriangles() * 3 * sizeof(unsigned int)) / (1024.0f * 1024.0f);
    ImGui::Text("Vertex Data: %.2f MB", vertexMemoryMB);
    ImGui::Text("Index Data: %.2f MB", indexMemoryMB);
    ImGui::Text("Total GPU: %.2f MB", vertexMemoryMB + indexMemoryMB);

    // Settings
    ImGui::SeparatorText("Settings");
    if (ImGui::SliderInt("Render Distance", &renderDistance, 2, 32)) {
        world.setRenderDistance(renderDistance);
    }

    // Snaps to 0 below the useful range, which the engine reads as uncapped.
    if (ImGui::SliderInt("FPS Limit", &fpsLimit, 0, 240,
                         fpsLimit <= 0 ? "Unlimited" : "%d")) {
        if (fpsLimit > 0 && fpsLimit < 10) {
            fpsLimit = 0;
        }
    }

#ifndef __EMSCRIPTEN__
    // WebGL2 has no glPolygonMode, so the toggle is desktop-only
    ImGui::Checkbox("Wireframe", &wireframe);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
#endif

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

#endif
