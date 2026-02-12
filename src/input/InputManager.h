//
// Created by maxim on 10/02/2026.
//

#ifndef GLFWVOXEL_INPUTMANAGER_H
#define GLFWVOXEL_INPUTMANAGER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>


enum class InputState {
    Released,
    JustPressed,
    Held,
    JustReleased
};

enum class InputContext {
    Gameplay,
    UI,
    Console,
    Any
};

enum class GameAction {
    // Movement
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Crouch,
    Sprint,

    // Flying
    Ascend,
    Descend,
    ToggleFly,

    // Camera
    LookUp,
    LookDown,
    LookLeft,
    LookRight,
    ZoomIn,
    ZoomOut,

    // Interaction
    PrimaryAction,
    SecondaryAction,
    TertiaryAction,

    // UI
    TogglePause,
    ToggleInventory,
    ToggleDebug,

    // Misc
    Screenshot,
    ReloadChunks
};

struct InputBinding {
    int key;
    int modifiers;

    InputBinding(int k, int mods = 0) : key(k), modifiers(mods) {}

    bool operator==(const InputBinding& other) const {
        return key == other.key && modifiers == other.modifiers;
    }
};

struct InputBindingHash {
    std::size_t operator()(const InputBinding& binding) const {
        return std::hash<int>()(binding.key) ^ (std::hash<int>()(binding.modifiers) << 1);
    }
};

class InputManager {
public:
    static InputManager& getInstance() {
        static InputManager instance;
        return instance;
    }

    void initialize(GLFWwindow* window);
    void shutdown();

    void update();

    // Context management
    void pushContext(InputContext context);
    void popContext();
    InputContext getCurrentContext() const { return contextStack.back(); }
    bool isContextActive(InputContext context) const;

    bool isKeyPressed(int key) const;
    bool isKeyHeld(int key) const;
    bool isKeyReleased(int key) const;
    InputState getKeyState(int key) const;

    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonHeld(int button) const;
    bool isMouseButtonReleased(int button) const;
    InputState getMouseButtonState(int button) const;

    glm::vec2 getMousePosition() const { return mousePosition; }
    glm::vec2 getMouseDelta() const { return mouseDelta; }
    float getScrollDelta() const { return scrollDelta; }

    bool isActionPressed(GameAction action) const;
    bool isActionHeld(GameAction action) const;
    bool isActionReleased(GameAction action) const;

    void bindAction(GameAction action, InputBinding binding, InputContext context = InputContext::Gameplay);
    void unbindAction(GameAction action);

    void setCursorVisible(bool visible);
    bool isCursorVisible() const { return cursorVisible; }
    void toggleCursor();
    void resetMouseDelta();

    void onKeyCallback(int key, int scancode, int action, int mods);
    void onMouseButtonCallback(int button, int action, int mods);
    void onCursorPosCallback(double xpos, double ypos);
    void onScrollCallback(double xoffset, double yoffset);

    void endFrame();
private:
    InputManager();
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // State tracking
    std::unordered_map<int, InputState> keyStates;
    std::unordered_map<int, InputState> mouseButtonStates;
    std::unordered_map<int, InputState> previousKeyStates;
    std::unordered_map<int, InputState> previousMouseButtonStates;

    // Mouse state
    glm::vec2 mousePosition;
    glm::vec2 lastMousePosition;
    glm::vec2 mouseDelta;
    float scrollDelta;
    bool firstMouse;
    bool cursorVisible;

    struct ActionBinding {
        InputBinding binding;
        InputContext context;
    };
    std::unordered_map<GameAction, std::vector<ActionBinding>> actionBindings;
    std::vector<InputContext> contextStack;
    GLFWwindow* window;

    // Helper methods
    void updateKeyState(int key, int action);
    void updateMouseButtonState(int button, int action);
    void transitionStates();
    void setupDefaultBindings();
    InputState determineInputState(InputState current, bool isPressed) const;
};

inline void inputKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    InputManager::getInstance().onKeyCallback(key, scancode, action, mods);
}

inline void inputMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    InputManager::getInstance().onMouseButtonCallback(button, action, mods);
}

inline void inputCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    InputManager::getInstance().onCursorPosCallback(xpos, ypos);
}

inline void inputScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    InputManager::getInstance().onScrollCallback(xoffset, yoffset);
}


#endif //GLFWVOXEL_INPUTMANAGER_H