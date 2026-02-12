//
// Created by maxim on 12/02/2026.
//

#include "PlayerController.h"


PlayerController::PlayerController(Player* player, World* world)
    : player(player), world(world), controlEnabled(true) {
}

void PlayerController::processInput(float deltaTime) {
    if (!controlEnabled) return;

    InputManager& input = InputManager::getInstance();

    processUIInput();

    if (input.getCurrentContext() != InputContext::Gameplay) {
        return;
    }

    processMovementInput(deltaTime);
    processCameraInput(deltaTime);
    processInteractionInput();
    processDebugInput();
}

void PlayerController::processMovementInput(float deltaTime) {
    InputManager& input = InputManager::getInstance();

    bool forward = input.isActionHeld(GameAction::MoveForward);
    bool backward = input.isActionHeld(GameAction::MoveBackward);
    bool left = input.isActionHeld(GameAction::MoveLeft);
    bool right = input.isActionHeld(GameAction::MoveRight);
    bool sprinting = input.isActionHeld(GameAction::Sprint);

    player->processMovement(forward, backward, left, right, sprinting, deltaTime);

    if (input.isActionPressed(GameAction::ToggleFly)) {
        if (player->isFlying()) {
            player->deactivateFlying();
        } else {
            player->activateFlying();
        }
    }

    if (player->isFlying()) {
        bool ascending = input.isActionHeld(GameAction::Ascend);
        bool descending = input.isActionHeld(GameAction::Descend);
        player->processVerticalInput(ascending, descending, deltaTime);
    } else {
        if (input.isActionPressed(GameAction::Jump)) {
            player->jump();
        }

        if (input.isActionPressed(GameAction::Crouch)) {
            // TODO: Implement crouching
        }
    }
}

void PlayerController::processCameraInput(float deltaTime) {
    InputManager& input = InputManager::getInstance();

    if (!input.isCursorVisible()) {
        // Mouse look
        glm::vec2 mouseDelta = input.getMouseDelta();
        if (glm::length(mouseDelta) > 0.0f) {
            player->getCamera().ProcessMouseMovement(mouseDelta.x, mouseDelta.y);
        }

        // Zoom
        float scrollDelta = input.getScrollDelta();
        if (scrollDelta != 0.0f) {
            player->getCamera().ProcessMouseScroll(scrollDelta);
        }
    }
}

void PlayerController::processInteractionInput() {
    InputManager& input = InputManager::getInstance();

    // Only interact when cursor is captured (not in menu)
    if (input.isCursorVisible()) return;

    RaycastResult target = getTargetedBlock();

    if (input.isActionPressed(GameAction::PrimaryAction)) {
        if (target.hit) {
            // Place block adjacent to hit surface
            glm::ivec3 placePos = target.hitPos - target.hitNormal;
            world->setBlock(placePos.x, placePos.y, placePos.z, BlockType::Grass);
            std::cout << "Placed block at: " << placePos.x << ", " << placePos.y << ", " << placePos.z << std::endl;
        }
    }

    if (input.isActionPressed(GameAction::SecondaryAction)) {
        if (target.hit) {
            // Break the targeted block
            world->setBlock(target.hitPos.x, target.hitPos.y, target.hitPos.z, BlockType::Air);
            std::cout << "Broke block at: " << target.hitPos.x << ", " << target.hitPos.y << ", " << target.hitPos.z << std::endl;
        }
    }

    if (input.isActionPressed(GameAction::TertiaryAction)) {
        if (target.hit) {
            BlockType targetBlock = world->getBlock(target.hitPos.x, target.hitPos.y, target.hitPos.z);
            // TODO: Set this as the active block in inventory
            std::cout << "Picked block type: " << (int)targetBlock << std::endl;
        }
    }
}

void PlayerController::processUIInput() {
    InputManager& input = InputManager::getInstance();

    // Toggle pause/menu (works in any context)
    if (input.isActionPressed(GameAction::TogglePause)) {
        if (input.getCurrentContext() == InputContext::Gameplay) {
            input.pushContext(InputContext::UI);
            std::cout << "Paused - cursor visible" << std::endl;
        } else if (input.getCurrentContext() == InputContext::UI) {
            input.popContext();
            std::cout << "Unpaused - cursor hidden" << std::endl;
        }
    }

    // Toggle inventory
    if (input.isActionPressed(GameAction::ToggleInventory)) {
        if (input.getCurrentContext() == InputContext::Gameplay) {
            input.pushContext(InputContext::UI);
            std::cout << "Opened inventory" << std::endl;
            // TODO: Show inventory UI
        }
    }
}

void PlayerController::processDebugInput() {
    InputManager& input = InputManager::getInstance();

    // Toggle debug overlay
    if (input.isActionPressed(GameAction::ToggleDebug)) {
        // TODO: Toggle debug HUD
        std::cout << "Toggled debug overlay" << std::endl;
    }

    // Take screenshot
    if (input.isActionPressed(GameAction::Screenshot)) {
        // TODO: Capture and save screenshot
        std::cout << "Screenshot saved!" << std::endl;
    }

    // Reload chunks (with modifier)
    if (input.isActionPressed(GameAction::ReloadChunks)) {
        // TODO: Force chunk reload
        std::cout << "Reloading chunks..." << std::endl;
    }
}

RaycastResult PlayerController::getTargetedBlock() const {
    return world->raycastBlock(player->getCamera().Position,player->getFront(),5.0f);
}
