//
// Created by maxim on 12/08/2026.
//

#ifndef GLFWVOXEL_ENGINE_H
#define GLFWVOXEL_ENGINE_H

#include "src/world/Block.h"
#include "src/world/DayCycle.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

#ifndef __EMSCRIPTEN__
class ImGUIManager;
#endif

class ChunkRenderer;
class CloudRenderer;
class HighlightBox;
class HotbarRenderer;
class HUDRenderer;
class Player;
class PlayerController;
class ShadowMap;
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

    // 0 is unlimited. Desktop paces itself by sleeping; the web build hands the rate to
    // the browser, which owns the loop and must not be blocked.
    void setFpsLimit(int fps);
    int getFpsLimit() const { return fpsLimit; }

    // Rebuilds the world and drops the player back at spawn
    void regenerateWorld(std::uint32_t seed);
    void regenerateWorld();

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

    int fpsLimit = 60;
#ifndef __EMSCRIPTEN__
    // Absolute deadline rather than "sleep for the leftover", so a frame that runs long
    // does not push every later frame back with it.
    std::chrono::steady_clock::time_point nextFrameTime{};
    void limitFrameRate();
#endif

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
    std::unique_ptr<HotbarRenderer> hotbarRenderer;
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<SkyBodyRenderer> skyBodyRenderer;
    std::unique_ptr<CloudRenderer> cloudRenderer;
    std::unique_ptr<ChunkRenderer> chunkRenderer;
    std::unique_ptr<ShadowMap> shadowMap;
    std::unique_ptr<World> world;
    std::unique_ptr<PlayerController> playerController;
#ifndef __EMSCRIPTEN__
    std::unique_ptr<ImGUIManager> imGUIManager;
#endif
    std::unique_ptr<HighlightBox> highlightBox;

    // samples per material, falling back to one shared set
    struct BlockSoundBank {
        std::unordered_map<BlockType, std::vector<unsigned int>> byBlock;
        std::vector<unsigned int> fallback;

        const std::vector<unsigned int>& variantsFor(BlockType type) const {
            const auto it = byBlock.find(type);
            return it != byBlock.end() ? it->second : fallback;
        }
    };

    BlockSoundBank breakSounds;
    BlockSoundBank placeSounds;
    std::mt19937 soundRng{std::random_device{}()};

    void loadBlockSounds();
    void playBlockSound(const BlockSoundBank& bank, BlockType type, const glm::ivec3& pos);

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
