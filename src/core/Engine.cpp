//
// Created by maxim on 12/08/2026.
//

#include "Engine.h"

#include "src/core/GL.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdio>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef __EMSCRIPTEN__
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "src/core/ImGUIManager.h"
#endif

#include "src/core/ImGUIManager.h"
#include "src/core/Window.h"
#include "src/debug/RenderStats.h"
#include "src/debug/TestScene.h"
#include "src/game/HighlightBox.h"
#include "src/game/HotbarRenderer.h"
#include "src/game/HUDRenderer.h"
#include "src/game/Player.h"
#include "src/game/SoundManager.h"
#include "src/input/InputManager.h"
#include "src/input/PlayerController.h"
#include "src/rendering/ChunkRenderer.h"
#include "src/rendering/ShadowMap.h"
#include "src/rendering/Skybox.h"
#include "src/rendering/SkyBodyRenderer.h"
#include "src/world/World.h"

namespace {
    // Clamping max delta time for browser stuff
    constexpr float MAX_DELTA_TIME = 0.1f;

#ifndef __EMSCRIPTEN__
    constexpr std::chrono::microseconds FRAME_SPIN_MARGIN{600};

#ifdef _WIN32
    class HighResTimer {
    public:
        HighResTimer() {
            handle = CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                TIMER_ALL_ACCESS);
            if (!handle) {
                handle = CreateWaitableTimerExW(nullptr, nullptr,
                    CREATE_WAITABLE_TIMER_MANUAL_RESET, TIMER_ALL_ACCESS);
            }
        }
        ~HighResTimer() { if (handle) CloseHandle(handle); }

        HighResTimer(const HighResTimer&) = delete;
        HighResTimer& operator=(const HighResTimer&) = delete;

        bool wait(std::chrono::nanoseconds duration) const {
            if (!handle || duration <= std::chrono::nanoseconds::zero()) {
                return false;
            }
            LARGE_INTEGER due;
            // Negative means relative, in 100ns units.
            due.QuadPart = -(duration.count() / 100);
            if (!SetWaitableTimer(handle, &due, 0, nullptr, nullptr, FALSE)) {
                return false;
            }
            return WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0;
        }

    private:
        HANDLE handle = nullptr;
    };
#endif

    void idleUntil(std::chrono::steady_clock::time_point target) {
        const auto now = std::chrono::steady_clock::now();
        if (target <= now) {
            return;
        }
#ifdef _WIN32
        static const HighResTimer timer;
        if (timer.wait(target - now)) {
            return;
        }
#endif
        std::this_thread::sleep_until(target);
    }
#endif

#ifdef __EMSCRIPTEN__
    Engine* g_engine = nullptr;
#endif

    constexpr float NEAR_PLANE = 0.1f;
    constexpr float FAR_PLANE = 1000.0f;
    constexpr float REACH_DISTANCE = 5.0f;

#ifndef __EMSCRIPTEN__
    constexpr const char* IMGUI_GLSL_VERSION = "#version 460";
#endif
}

Engine::Engine() = default;

Engine::~Engine() {
#ifndef __EMSCRIPTEN__
    shutdownImGui();
#endif
}

bool Engine::initialize(unsigned int width, unsigned int height, const char* title) {
#ifdef __EMSCRIPTEN__
    g_engine = this;
#endif

    glfwInit();
#ifdef __EMSCRIPTEN__
    // WebGL2 is GLES 3.0.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    // Pure CPU state, so it can be built before there is a context.
    player = std::make_unique<Player>(Player::SPAWN_POSITION);

    window = std::make_unique<Window>(width, height, title);
    if (window->getWindow() == nullptr) {
        return false;
    }

    soundManager = std::make_unique<SoundManager>(*player);
    if (!soundManager->initialize()) {
        std::cerr << "Failed to initialize sound system!" << std::endl;
    }

    // Sound stuff, TODO: move to a proper game manager
    ALuint ambianceBuffer = soundManager->loadSound("assets/music/MusicAmbianceMono.wav");
    // TODO improve this
    soundManager->playSound3D(ambianceBuffer, 0.0f, 55.0f, 0.0f, true, 0.3f);
    loadBlockSounds();

    glfwMakeContextCurrent(*window);
#ifndef __EMSCRIPTEN__
    // Emscripten routes glfwSwapInterval to emscripten_set_main_loop_timing, which does
    // not exist yet at this point and logs an error. The browser paces us through
    // requestAnimationFrame regardless.
    window->setSwapInterval(vsync ? 1 : 0);
#endif

    InputManager::getInstance().initialize(*window);

    // ImGui's GLFW backend does not touch the user pointer, so the engine can claim it
    // to route the C-style resize callback back to an instance.
    glfwSetWindowUserPointer(*window, this);
    glfwSetFramebufferSizeCallback(*window, framebufferSizeCallback);
    glfwGetFramebufferSize(*window, &framebufferWidth, &framebufferHeight);

#ifndef __EMSCRIPTEN__
    // Emscripten links the opengl function directly, no need for glad
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }
#endif

    hudRenderer = std::make_unique<HUDRenderer>(framebufferWidth, framebufferHeight);
    hotbarRenderer = std::make_unique<HotbarRenderer>();
    skybox = std::make_unique<Skybox>();
    skyBodyRenderer = std::make_unique<SkyBodyRenderer>();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Renderer owns the terrain shader and atlas; World borrows the atlas for meshing,
    // so the renderer has to be constructed first and outlive the world.
    chunkRenderer = std::make_unique<ChunkRenderer>();
    if (!chunkRenderer->loadTextureAtlas("assets/textures/spritesheet_tiles.png", 10)) {
        std::cerr << "Failed to load texture atlas!" << std::endl;
    }

    shadowMap = std::make_unique<ShadowMap>();
    world = std::make_unique<World>(*player, chunkRenderer->getTextureAtlas());
    world->setRenderDistance(renderDistance);

    world->setBlockChangeCallback([this](const glm::ivec3& pos, BlockType from, BlockType to) {
        // Simple way to check if it's destroyed or placed
        if (to == BlockType::Air) {
            playBlockSound(breakSounds, from, pos);
        } else {
            playBlockSound(placeSounds, to, pos);
        }
    });

    playerController = std::make_unique<PlayerController>(player.get(), world.get());

#ifndef __EMSCRIPTEN__
    initImGui();
    imGUIManager = std::make_unique<ImGUIManager>(*world, player->getCamera(), renderDistance,
                                                  *player, dayCycle, fpsLimit, *shadowMap);
#endif

    highlightBox = std::make_unique<HighlightBox>();

    return true;
}

void Engine::step() {
    glfwPollEvents();

    const float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = std::min(currentFrame - lastFrame, MAX_DELTA_TIME);
    lastFrame = currentFrame;

    RenderStats::getInstance().resetFrame();

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifndef __EMSCRIPTEN__
    // Capture flags from the frame ImGui drew last
    if (imguiInitialised && showDebugUI) {
        const ImGuiIO& io = ImGui::GetIO();
        InputManager::getInstance().setUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);
    } else {
        InputManager::getInstance().setUICapture(false, false);
    }
#endif

    InputManager::getInstance().update();

    if (InputManager::getInstance().isActionPressed(GameAction::ReloadChunks)) {
        regenerateWorld();
    }

#ifndef __EMSCRIPTEN__
    if (InputManager::getInstance().isActionPressed(GameAction::ToggleDebug)) {
        showDebugUI = !showDebugUI;
    }
#endif
    player->update(deltaTime, *world);
    playerController->processInput(deltaTime);
    world->update(player->getPosition());

    EntityManager* entityManager = world->getEntityManager();
    entityManager->update(deltaTime, world.get());
    soundManager->update();
    dayCycle.update(deltaTime);

    // Built from the live framebuffer size, not the requested window size: the two
    // diverge on resize, and on the web the canvas is whatever the page gives us.
    const float aspect = framebufferHeight > 0
        ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight)
        : 1.0f;
    const glm::mat4 projection = glm::perspective(glm::radians(player->getCamera().Zoom),
        aspect, NEAR_PLANE, FAR_PLANE);
    const glm::mat4 view = player->getCamera().GetViewMatrix();

    const SunState sun = dayCycle.getSun();

    // One check feeds both the terrain fog and the screen tint, so they cannot disagree.
    const glm::vec3 eye = player->getCamera().Position;
    const bool underwater = world->getBlock(
        static_cast<int>(std::floor(eye.x)),
        static_cast<int>(std::floor(eye.y)),
        static_cast<int>(std::floor(eye.z))) == BlockType::Water;

    if (shadowMap->update(sun, eye)) {
        shadowMap->beginPass();
        chunkRenderer->renderShadowPass(*world, *shadowMap);
        shadowMap->endPass(framebufferWidth, framebufferHeight);
    }

    chunkRenderer->render(*world, projection, view, sun, eye, underwater, *skybox, *shadowMap);
    entityManager->render(projection, view);
    entityManager->renderDebug(projection, view);

    const RaycastResult highlightedBlock =
        world->raycastBlock(player->getCamera().Position, player->getFront(), REACH_DISTANCE);
    if (highlightedBlock.hit) {
        highlightBox->draw(highlightedBlock, projection, view);
    }

    skybox->draw(view, projection, sun);
    // After the skybox: both sit against the far plane, so the later draw is the visible one.
    skyBodyRenderer->draw(view, projection, sun);

    // After the sky so it covers it too, before the HUD so that stays readable.
    if (underwater) {
        hudRenderer->drawRect(framebufferWidth * 0.5f, framebufferHeight * 0.5f,
                              static_cast<float>(framebufferWidth),
                              static_cast<float>(framebufferHeight),
                              glm::vec4(ChunkRenderer::WATER_FOG_COLOR,
                                        ChunkRenderer::WATER_TINT_ALPHA));
    }

    hudRenderer->drawCrosshair();
    hotbarRenderer->draw(*hudRenderer, player->getHotbar(), chunkRenderer->getTextureAtlas());

    if (shadowMap->getSettings().debugView) {
        const int cascades = shadowMap->getCascadeCount();
        const float size = static_cast<float>(std::min(framebufferWidth, framebufferHeight)) * 0.22f;
        shadowMap->setDepthCompare(false);
        for (int i = 0; i < cascades; ++i) {
            hudRenderer->drawTile(size * (0.5f + i) + 8.0f * (i + 1), size * 0.5f + 8.0f,
                                  size, size, shadowMap->getDepthTexture(), i);
        }
        shadowMap->setDepthCompare(true);
    }

#ifndef __EMSCRIPTEN__
    if (showDebugUI) {
        imGUIManager->drawImGUIElements(deltaTime);
    }
#endif

#ifdef __EMSCRIPTEN__
    publishWebStats();
#endif

    glfwSwapBuffers(*window);
    InputManager::getInstance().endFrame();

#ifndef __EMSCRIPTEN__
    // After the swap, so the wait is pure idle rather than time the GPU could have used.
    // step() takes its delta at the top, so the sleep lands inside the next frame's delta
    // and game time stays real.
    limitFrameRate();
#endif
}

void Engine::regenerateWorld() {
    if (!world || !player) {
        return;
    }

    world->regenerate();
    player->respawn(Player::SPAWN_POSITION);

    std::cout << "New world, seed " << world->getSeed() << std::endl;
}

void Engine::setRenderDistance(int distance) {
    renderDistance = std::clamp(distance, 2, 32);
    if (world) {
        world->setRenderDistance(renderDistance);
    }
}

bool Engine::shouldClose() const {
    return window == nullptr || glfwWindowShouldClose(*window);
}

void Engine::run() {
    while (!shouldClose()) {
        step();
    }
}

#ifndef __EMSCRIPTEN__
void Engine::limitFrameRate() {
    using namespace std::chrono;

    if (fpsLimit <= 0) {
        // Reset, so re-enabling the limit does not try to catch up on the idle period.
        nextFrameTime = steady_clock::time_point{};
        return;
    }

    const auto period = duration_cast<steady_clock::duration>(duration<double>(1.0 / fpsLimit));
    const auto now = steady_clock::now();

    // Either the first limited frame or one that ran long enough to fall behind. Catching
    // up would mean a burst of unpaced frames, so restart the cadence from here instead.
    if (nextFrameTime == steady_clock::time_point{} || now > nextFrameTime + period) {
        nextFrameTime = now;
    }
    nextFrameTime += period;

    idleUntil(nextFrameTime - FRAME_SPIN_MARGIN);
    while (steady_clock::now() < nextFrameTime) {
        std::this_thread::yield();
    }
}
#endif

void Engine::setFpsLimit(int fps) {
    fpsLimit = fps <= 0 ? 0 : std::clamp(fps, 10, 1000);

#ifdef __EMSCRIPTEN__
    // The browser owns the loop here, so the rate is a request rather than a sleep.
    // RAF stays vsync-aligned; an arbitrary cap has to fall back to a timer.
    if (fpsLimit <= 0) {
        emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
    } else {
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 1000 / fpsLimit);
    }
#endif
}

#ifndef __EMSCRIPTEN__

void Engine::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(*window, true);
    ImGui_ImplOpenGL3_Init(IMGUI_GLSL_VERSION);
    imguiInitialised = true;
}

void Engine::shutdownImGui() {
    if (!imguiInitialised) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInitialised = false;
}

#endif

void Engine::loadBlockSounds() {
    auto loadSet = [this](const std::string& prefix, int count) {
        std::vector<unsigned int> buffers;
        for (int i = 0; i < count; ++i) {
            const std::string path =
                "assets/sfx/player_hit/" + prefix + "_00" + std::to_string(i) + ".wav";
            if (const ALuint buffer = soundManager->loadSound(path)) {
                buffers.push_back(buffer);
            }
        }
        return buffers;
    };

    breakSounds.fallback = loadSet("player_destroy", 5);
    placeSounds.fallback = loadSet("player_place", 5);
}

void Engine::playBlockSound(const BlockSoundBank& bank, BlockType type, const glm::ivec3& pos) {
    const std::vector<unsigned int>& variants = bank.variantsFor(type);

    if (variants.empty()) {
        return;
    }

    // vary the tune
    std::uniform_int_distribution<std::size_t> pick(0, variants.size() - 1);
    std::uniform_real_distribution<float> detune(0.92f, 1.08f);

    // offset to the block's centre
    const ALuint source = soundManager->playSound3D(
        variants[pick(soundRng)],
        static_cast<float>(pos.x) + 0.5f,
        static_cast<float>(pos.y) + 0.5f,
        static_cast<float>(pos.z) + 0.5f,
        false, 0.8f);

    soundManager->setSourcePitch(source, detune(soundRng));
}

void Engine::onFramebufferResize(int width, int height) {
    framebufferWidth = width;
    framebufferHeight = height;
    glViewport(0, 0, width, height);
    if (hudRenderer) {
        hudRenderer->onResize(width, height);
    }
}

void Engine::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window))) {
        engine->onFramebufferResize(width, height);
    }
}

#ifdef __EMSCRIPTEN__

void Engine::publishWebStats() {
    constexpr float PUBLISH_INTERVAL = 0.25f;

    statsPublishTimer += deltaTime;
    if (statsPublishTimer < PUBLISH_INTERVAL) {
        return;
    }
    statsPublishTimer = 0.0f;

    auto& stats = RenderStats::getInstance();
    const glm::vec3 position = player->getPosition();
    const Biome* biome = world->getCurrentPlayerBiome(position.x, position.z);

    char json[512];
    std::snprintf(json, sizeof(json),
        "{\"fps\":%.0f,\"frameMs\":%.2f,\"drawCalls\":%d,\"triangles\":%d,"
        "\"chunksRendered\":%d,\"chunksLoaded\":%d,\"entities\":%d,"
        "\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"biome\":\"%s\",\"clock\":\"%s\","
        "\"renderDistance\":%d,\"dayLength\":%.0f,\"timeOfDay\":%.4f,\"seed\":%u,"
        "\"fpsLimit\":%d}",
        deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f,
        deltaTime * 1000.0f,
        stats.getDrawCalls(), stats.getTriangles(),
        stats.getChunksRendered(), world->getLoadedChunkCount(),
        world->getEntityManager()->getEntityCount(),
        position.x, position.y, position.z,
        biome ? biome->getName().c_str() : "-",
        dayCycle.getClockString().c_str(),
        renderDistance, dayCycle.getDayLength(), dayCycle.getTimeOfDay(),
        static_cast<unsigned int>(world->getSeed()), fpsLimit);

    EM_ASM({
        if (window.voxelStats) {
            window.voxelStats(UTF8ToString($0));
        }
    }, json);
}

// called from the page's stats panel
extern "C" {

EMSCRIPTEN_KEEPALIVE void voxel_set_render_distance(int distance) {
    if (g_engine) g_engine->setRenderDistance(distance);
}

EMSCRIPTEN_KEEPALIVE void voxel_set_fps_limit(int fps) {
    if (g_engine) g_engine->setFpsLimit(fps);
}

EMSCRIPTEN_KEEPALIVE void voxel_set_time_of_day(float fraction) {
    if (g_engine) g_engine->getDayCycle().setTimeOfDay(fraction);
}

EMSCRIPTEN_KEEPALIVE void voxel_set_day_length(float seconds) {
    if (g_engine) g_engine->getDayCycle().setDayLength(seconds);
}

EMSCRIPTEN_KEEPALIVE void voxel_new_world() {
    if (g_engine) g_engine->regenerateWorld();
}

}

#endif
