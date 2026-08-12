#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "src/core/Engine.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// Window Settings
constexpr unsigned int SCR_WIDTH = 1920;
constexpr unsigned int SCR_HEIGHT = 1080;

#ifdef __EMSCRIPTEN__
namespace {
    void emscriptenStep(void* arg) {
        static_cast<Engine*>(arg)->step();
    }
}
#endif

int main() {
#ifdef __EMSCRIPTEN__
    // The browser owns the loop, so the engine has to outlive main
    auto* engine = new Engine();
    if (!engine->initialize(SCR_WIDTH, SCR_HEIGHT, "OpenGL")) {
        return -1;
    }
    emscripten_set_main_loop_arg(emscriptenStep, engine, 0, 1);
    return 0;
#else
    int exitCode = 0;
    {
        Engine engine;
        if (engine.initialize(SCR_WIDTH, SCR_HEIGHT, "OpenGL")) {
            engine.run();
        } else {
            exitCode = -1;
        }
    } // engine (and with it the window) is destroyed here, before GLFW shuts down

    glfwTerminate();
    return exitCode;
#endif
}
