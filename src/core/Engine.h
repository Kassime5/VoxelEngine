//
// Created by maxim on 12/08/2026.
//

#ifndef GLFWVOXEL_ENGINE_H
#define GLFWVOXEL_ENGINE_H

#include <memory>

struct GLFWwindow;

class ChunkRenderer;
class HighlightBox;
class HUDRenderer;
class ImGUIManager;
class Player;
class PlayerController;
class Skybox;
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
    bool imguiInitialised = false;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Declaration order is construction order, and members are destroyed in reverse
    std::unique_ptr<Player> player;
    std::unique_ptr<Window> window;
    std::unique_ptr<SoundManager> soundManager;
    std::unique_ptr<HUDRenderer> hudRenderer;
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<ChunkRenderer> chunkRenderer;
    std::unique_ptr<World> world;
    std::unique_ptr<PlayerController> playerController;
    std::unique_ptr<ImGUIManager> imGUIManager;
    std::unique_ptr<HighlightBox> highlightBox;

    void initImGui();
    void shutdownImGui();
    void onFramebufferResize(int width, int height);

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
};

#endif //GLFWVOXEL_ENGINE_H
