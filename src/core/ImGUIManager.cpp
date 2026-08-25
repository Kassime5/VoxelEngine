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
                           DayCycle& _dayCycle, int& _fpsLimit, ShadowMap& _shadowMap) :
    world(_world), camera(_camera),
    player(_player), entityManager(*_world.getEntityManager()),
    dayCycle(_dayCycle),
    shadowMap(_shadowMap),
    renderDistance(_renderDistance),
    fpsLimit(_fpsLimit)
{}

ImGUIManager::~ImGUIManager() {}

void ImGUIManager::drawImGUIElements(float deltaTime) {
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
    if (ImGui::SliderFloat("Day length (s)", &dayLength, 0.0f, 1200.0f, "%.0f")) {
        dayCycle.setDayLength(dayLength);
    }

    float timeOfDay = dayCycle.getTimeOfDay();
    if (ImGui::SliderFloat("Time of day", &timeOfDay, 0.0f, 1.0f, "%.3f")) {
        dayCycle.setTimeOfDay(timeOfDay);
    }

    drawShadowSettings();
    drawTestScene();

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

    ImGui::Checkbox("Wireframe", &wireframe);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGUIManager::drawShadowSettings() {
    ShadowSettings& shadow = shadowMap.getSettings();

    ImGui::SeparatorText("Shadows");
    ImGui::Checkbox("Sun shadows", &shadow.enabled);

    if (!shadow.enabled) {
        return;
    }

    const RenderStats& stats = RenderStats::getInstance();
    ImGui::Text("Casters: %d chunks, %d draws",
                stats.getShadowChunks(), stats.getShadowDrawCalls());
    ImGui::Text("Strength: %.2f", shadowMap.getEffectiveStrength());

    static const char* RESOLUTIONS[] = {"512", "1024", "2048", "4096", "8192"};
    int resolutionIndex = 0;
    while (resolutionIndex < 4 && (512 << resolutionIndex) < shadow.resolution) {
        resolutionIndex++;
    }
    if (ImGui::Combo("Resolution", &resolutionIndex, RESOLUTIONS, IM_ARRAYSIZE(RESOLUTIONS))) {
        shadow.resolution = 512 << resolutionIndex;
    }

    // ImGui::SliderInt("Cascades", &shadow.cascadeCount, 1, ShadowMap::MAX_CASCADES);
    // ImGui::SliderFloat("Near radius", &shadow.nearRadius, 4.0f, 128.0f, "%.0f blocks");
    // ImGui::SliderFloat("Radius", &shadow.radius, 32.0f, 512.0f, "%.0f blocks");

    // for (int i = 0; i < shadowMap.getCascadeCount(); ++i) {
    //     ImGui::Text("  cascade %d: to %5.0f blocks, %.4f blocks/texel",
    //                 i, shadowMap.getSplitDistances()[i], shadowMap.getTexelWorldSize(i));
    // }

    // ImGui::SliderFloat("Strength", &shadow.strength, 0.0f, 1.0f, "%.2f");
    // ImGui::SliderFloat("Poly offset factor", &shadow.polygonOffsetFactor, 0.0f, 16.0f, "%.1f");
    // ImGui::SliderFloat("Poly offset units", &shadow.polygonOffsetUnits, 0.0f, 32.0f, "%.1f");
    // ImGui::Checkbox("Cull front faces", &shadow.cullFrontFaces);
    ImGui::Checkbox("Show depth map", &shadow.debugView);
}

void ImGUIManager::goTo(const TestScene::Viewpoint& view) {
    player.respawn(view.position);
    camera.Yaw = view.yaw;
    camera.Pitch = view.pitch;
    camera.ProcessMouseMovement(0.0f, 0.0f);
}

void ImGUIManager::drawTestScene() {
    ImGui::SeparatorText("Test scene");

    if (ImGui::Checkbox("Flat test world", &TestScene::enabled)) {
        world.regenerate(world.getSeed());
        goTo(TestScene::OVERVIEW);
    }

    if (!TestScene::enabled) {
        ImGui::TextDisabled("Fixed props on a flat plane, for reproducible renderer tests.");
        return;
    }

    ImGui::TextDisabled("Freeze the sun with Day length 0 to hold one light angle.");

    if (ImGui::Button("Grazing wall")) goTo(TestScene::WALL_VIEW);
    ImGui::SameLine();
    if (ImGui::Button("Ground")) goTo(TestScene::GROUND_VIEW);
    ImGui::SameLine();
    if (ImGui::Button("Overview")) goTo(TestScene::OVERVIEW);
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
