//
// Created by maxim on 10/02/2026.
//

#include "InputManager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>

namespace
{
    EM_BOOL pointerLockChanged(int, const EmscriptenPointerlockChangeEvent* event, void* userData) {
        static_cast<InputManager*>(userData)->onPointerLockChanged(event->isActive != 0);
        return EM_FALSE;
    }
}
#endif

InputManager::InputManager() : mousePosition(0.0f), lastMousePosition(0.0f), mouseDelta(0.0f), scrollDelta(0.0f),
                               firstMouse(true), cursorVisible(false), uiCapturesMouse(false),
                               uiCapturesKeyboard(false), window(nullptr) {
    contextStack.push_back(InputContext::Gameplay);
}

void InputManager::initialize(GLFWwindow* _window) {
    window = _window;

#ifdef __EMSCRIPTEN__
    // Pointer lock is done on the browser, without this the engine keeps believing the mouse is captured after Escape frees it.
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, this, EM_FALSE, pointerLockChanged);
#endif

    glfwSetKeyCallback(window, inputKeyCallback);
    glfwSetMouseButtonCallback(window, inputMouseButtonCallback);
    glfwSetCursorPosCallback(window, inputCursorPosCallback);
    glfwSetScrollCallback(window, inputScrollCallback);

    setCursorVisible(false);

    setupDefaultBindings();
}

void InputManager::shutdown() {
    keyStates.clear();
    mouseButtonStates.clear();
    actionBindings.clear();
}

void InputManager::update() {
    if (!cursorVisible && !uiCapturesMouse) {
        mouseDelta = mousePosition - lastMousePosition;
        mouseDelta.y = -mouseDelta.y;
    }
    else {
        mouseDelta = glm::vec2(0.0f);
    }
    lastMousePosition = mousePosition;

    if (uiCapturesMouse) {
        scrollDelta = 0.0f; // scrolling an ImGui list should not also cycle the hotbar
    }
}

void InputManager::pushContext(InputContext context) {
    contextStack.push_back(context);

    if (context == InputContext::UI || context == InputContext::Console) {
        setCursorVisible(true);
    }
}

void InputManager::popContext() {
    if (contextStack.size() > 1) {
        InputContext oldContext = contextStack.back();
        contextStack.pop_back();

        if ((oldContext == InputContext::UI || oldContext == InputContext::Console) &&
            getCurrentContext() == InputContext::Gameplay) {
            setCursorVisible(false);
        }
    }
}

bool InputManager::isContextActive(InputContext context) const {
    if (context == InputContext::Any) return true;
    return getCurrentContext() == context;
}

// Same guard as the mouse queries: ctrl-clicking an ImGui slider turns it into a text
// field, and WASD should type into it rather than walk the player away.

bool InputManager::isKeyPressed(int key) const {
    if (uiCapturesKeyboard) return false;
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::JustPressed;
}

bool InputManager::isKeyHeld(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && (it->second == InputState::Held || it->second == InputState::JustPressed);
}

bool InputManager::isKeyReleased(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() && it->second == InputState::JustReleased;
}

InputState InputManager::getKeyState(int key) const {
    auto it = keyStates.find(key);
    return it != keyStates.end() ? it->second : InputState::Released;
}

bool InputManager::isMouseButtonPressed(int button) const {
    if (uiCapturesMouse) return false;
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second == InputState::JustPressed;
}

bool InputManager::isMouseButtonHeld(int button) const {
    if (uiCapturesMouse) return false;
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() &&
        (it->second == InputState::Held || it->second == InputState::JustPressed);
}

bool InputManager::isMouseButtonReleased(int button) const {
    if (uiCapturesMouse) return false;
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() && it->second == InputState::JustReleased;
}

InputState InputManager::getMouseButtonState(int button) const {
    auto it = mouseButtonStates.find(button);
    return it != mouseButtonStates.end() ? it->second : InputState::Released;
}

bool InputManager::isActionPressed(GameAction action) const {
    auto it = actionBindings.find(action);
    if (it == actionBindings.end()) return false;

    for (const auto& binding : it->second) {
        if (!isContextActive(binding.context) && binding.context != InputContext::Any)
            continue;

        // Check if key is pressed (considering modifiers)
        if (isKeyPressed(binding.binding.key)) {
            // TODO: Check modifiers match
            return true;
        }

        if (binding.binding.key >= GLFW_MOUSE_BUTTON_1 &&
            binding.binding.key <= GLFW_MOUSE_BUTTON_LAST) {
            if (isMouseButtonPressed(binding.binding.key)) {
                return true;
            }
        }
    }

    return false;
}

bool InputManager::isActionHeld(GameAction action) const {
    auto it = actionBindings.find(action);
    if (it == actionBindings.end()) return false;

    for (const auto& binding : it->second) {
        if (!isContextActive(binding.context) && binding.context != InputContext::Any)
            continue;

        if (isKeyHeld(binding.binding.key)) {
            return true;
        }

        if (binding.binding.key >= GLFW_MOUSE_BUTTON_1 &&
            binding.binding.key <= GLFW_MOUSE_BUTTON_LAST) {
            if (isMouseButtonHeld(binding.binding.key)) {
                return true;
            }
        }
    }

    return false;
}

bool InputManager::isActionReleased(GameAction action) const {
    auto it = actionBindings.find(action);
    if (it == actionBindings.end()) return false;

    for (const auto& binding : it->second) {
        if (!isContextActive(binding.context) && binding.context != InputContext::Any)
            continue;

        if (isKeyReleased(binding.binding.key)) {
            return true;
        }

        if (binding.binding.key >= GLFW_MOUSE_BUTTON_1 &&
            binding.binding.key <= GLFW_MOUSE_BUTTON_LAST) {
            if (isMouseButtonReleased(binding.binding.key)) {
                return true;
            }
        }
    }

    return false;
}

void InputManager::bindAction(GameAction action, InputBinding binding, InputContext context) {
    actionBindings[action].push_back({binding, context});
}

void InputManager::unbindAction(GameAction action) {
    actionBindings.erase(action);
}

void InputManager::setCursorVisible(bool visible) {
    cursorVisible = visible;
    if (window) {
        glfwSetInputMode(window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }

    if (!visible) {
        resetMouseDelta();
    }
}

void InputManager::toggleCursor() {
    setCursorVisible(!cursorVisible);
}

void InputManager::onPointerLockChanged(bool locked) {
    // Only the flag is mirrored. Calling glfwSetInputMode here would ask the browser to
    // undo what it just did; GLFW's own cursor mode is left alone, which is what lets a
    // later click on the canvas re-acquire the lock.
    cursorVisible = !locked;

    // The virtual cursor GLFW accumulates while locked sits nowhere near the real pointer,
    // so the first sample after a transition would otherwise be one enormous delta and
    // whip the camera around.
    resetMouseDelta();
}

void InputManager::setUICapture(bool mouse, bool keyboard) {
    uiCapturesMouse = mouse;
    uiCapturesKeyboard = keyboard;
}

void InputManager::resetMouseDelta() {
    firstMouse = true;
    mouseDelta = glm::vec2(0.0f);
}

void InputManager::onKeyCallback(int key, int scancode, int action, int mods) {
    updateKeyState(key, action);
}

void InputManager::onMouseButtonCallback(int button, int action, int mods) {
    updateMouseButtonState(button, action);
}

void InputManager::onCursorPosCallback(double xpos, double ypos) {
    mousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    if (firstMouse) {
        lastMousePosition = mousePosition;
        firstMouse = false;
    }
}

void InputManager::onScrollCallback(double xoffset, double yoffset) {
    scrollDelta = static_cast<float>(yoffset);
}

void InputManager::updateKeyState(int key, int action) {
    if (action == GLFW_PRESS) {
        keyStates[key] = InputState::JustPressed;
    }
    else if (action == GLFW_RELEASE) {
        keyStates[key] = InputState::JustReleased;
    }
}

void InputManager::updateMouseButtonState(int button, int action) {
    if (action == GLFW_PRESS) {
        mouseButtonStates[button] = InputState::JustPressed;
    }
    else if (action == GLFW_RELEASE) {
        mouseButtonStates[button] = InputState::JustReleased;
    }
}

void InputManager::transitionStates() {
    for (auto& pair : keyStates) {
        if (pair.second == InputState::JustPressed) {
            pair.second = InputState::Held;
        }
        else if (pair.second == InputState::JustReleased) {
            pair.second = InputState::Released;
        }
    }

    for (auto& pair : mouseButtonStates) {
        if (pair.second == InputState::JustPressed) {
            pair.second = InputState::Held;
        }
        else if (pair.second == InputState::JustReleased) {
            pair.second = InputState::Released;
        }
    }
}

void InputManager::setupDefaultBindings() {
    // Movement
    bindAction(GameAction::MoveForward, InputBinding(GLFW_KEY_W));
    bindAction(GameAction::MoveBackward, InputBinding(GLFW_KEY_S));
    bindAction(GameAction::MoveLeft, InputBinding(GLFW_KEY_A));
    bindAction(GameAction::MoveRight, InputBinding(GLFW_KEY_D));
    bindAction(GameAction::Jump, InputBinding(GLFW_KEY_SPACE));
    bindAction(GameAction::Crouch, InputBinding(GLFW_KEY_LEFT_CONTROL));
    bindAction(GameAction::Sprint, InputBinding(GLFW_KEY_LEFT_SHIFT));

    // Flying
    bindAction(GameAction::Ascend, InputBinding(GLFW_KEY_SPACE));
    bindAction(GameAction::Descend, InputBinding(GLFW_KEY_LEFT_CONTROL));
    bindAction(GameAction::ToggleFly, InputBinding(GLFW_KEY_F));

    // Interaction
    bindAction(GameAction::PrimaryAction, InputBinding(GLFW_MOUSE_BUTTON_LEFT));
    bindAction(GameAction::SecondaryAction, InputBinding(GLFW_MOUSE_BUTTON_RIGHT));
    bindAction(GameAction::TertiaryAction, InputBinding(GLFW_MOUSE_BUTTON_MIDDLE));

    // Hotbar 1..9 then 0 for the tenth slot
    bindAction(GameAction::HotbarSlot1, InputBinding(GLFW_KEY_1));
    bindAction(GameAction::HotbarSlot2, InputBinding(GLFW_KEY_2));
    bindAction(GameAction::HotbarSlot3, InputBinding(GLFW_KEY_3));
    bindAction(GameAction::HotbarSlot4, InputBinding(GLFW_KEY_4));
    bindAction(GameAction::HotbarSlot5, InputBinding(GLFW_KEY_5));
    bindAction(GameAction::HotbarSlot6, InputBinding(GLFW_KEY_6));
    bindAction(GameAction::HotbarSlot7, InputBinding(GLFW_KEY_7));
    bindAction(GameAction::HotbarSlot8, InputBinding(GLFW_KEY_8));
    bindAction(GameAction::HotbarSlot9, InputBinding(GLFW_KEY_9));
    bindAction(GameAction::HotbarSlot10, InputBinding(GLFW_KEY_0));

    // UI
    bindAction(GameAction::ToggleHitBox, InputBinding(GLFW_KEY_F3));
    bindAction(GameAction::TogglePause, InputBinding(GLFW_KEY_ESCAPE), InputContext::Any);
    bindAction(GameAction::ToggleInventory, InputBinding(GLFW_KEY_E));
    bindAction(GameAction::ToggleDebug, InputBinding(GLFW_KEY_F1), InputContext::Any);

    // Misc
    bindAction(GameAction::Screenshot, InputBinding(GLFW_KEY_F2));
    bindAction(GameAction::ReloadChunks, InputBinding(GLFW_KEY_R));
}

InputState InputManager::determineInputState(InputState current, bool isPressed) const {
    if (isPressed) {
        return current == InputState::Released || current == InputState::JustReleased
                   ? InputState::JustPressed
                   : InputState::Held;
    }

    return current == InputState::Held || current == InputState::JustPressed
               ? InputState::JustReleased
               : InputState::Released;
}

void InputManager::endFrame() {
    transitionStates();
    scrollDelta = 0.0f;
}
