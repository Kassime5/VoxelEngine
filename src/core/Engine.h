//
// Created by maxim on 12/08/2026.
//

#ifndef GLFWVOXEL_ENGINE_H
#define GLFWVOXEL_ENGINE_H

#include "src/world/DayCycle.h"

#include <memory>

struct GLFWwindow;

#ifndef __EMSCRIPTEN__
class ImGUIManager;
#endif

class ChunkRenderer;
class HighlightBox;
class HUDRenderer;
class Player;
class PlayerController;
class Skybox;
class SkyBodyRenderer;
class SoundManager;
class Window;
class World;

// Owns every subsystem and advances the game exactly one frame per step().
class Engine {
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize(unsigned int width, unsigned int height, const char* title);
    void step();
    bool shouldClose() const;
    // Desktop entry point. Unused on the web, where the browser owns the loop.
    void run();

    // driven by the browser's stats panel
    void setRenderDistance(int distance);
    DayCycle& getDayCycle() { return dayCycle; }

private:
    int framebufferWidth = 0;
    int framebufferHeight = 0;
#ifdef __EMSCRIPTEN__
    // ~50 chunks rather than ~200
    int renderDistance = 4;
#else
    int renderDistance = 8;
#endif

    bool vsync = false;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

#ifdef __EMSCRIPTEN__
    // ImGui is not compiled into the web build at all
    float statsPublishTimer = 0.0f;
#else
    bool imguiInitialised = false;
    bool showDebugUI = true;
#endif


    DayCycle dayCycle{120.0f};

    // Declaration order is construction order, and members are destroyed in reverse
    std::unique_ptr<Player> player;
    std::unique_ptr<Window> window;
    std::unique_ptr<SoundManager> soundManager;
    std::unique_ptr<HUDRenderer> hudRenderer;
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<SkyBodyRenderer> skyBodyRenderer;
    std::unique_ptr<ChunkRenderer> chunkRenderer;
    std::unique_ptr<World> world;
    std::unique_ptr<PlayerController> playerController;
#ifndef __EMSCRIPTEN__
    std::unique_ptr<ImGUIManager> imGUIManager;
#endif
    std::unique_ptr<HighlightBox> highlightBox;

    void onFramebufferResize(int width, int height);
#ifndef __EMSCRIPTEN__
    void initImGui();
    void shutdownImGui();
#endif

#ifdef __EMSCRIPTEN__
    void publishWebStats();
#endif

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
};

#endif //GLFWVOXEL_ENGINE_H
