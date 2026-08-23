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
    processHotbarInput();
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
        if (input.isActionHeld(GameAction::Jump)) {
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
    }
}

void PlayerController::processHotbarInput() {
    InputManager& input = InputManager::getInstance();

    if (input.isCursorVisible()) return;

    Hotbar& hotbar = player->getHotbar();

    const float scroll = input.getScrollDelta();
    if (scroll != 0.0f) {
        // Scrolling up moves toward slot 1
        hotbar.cycle(scroll > 0.0f ? -1 : 1);
    }

    for (int i = 0; i < Hotbar::SLOT_COUNT; ++i) {
        const auto action = static_cast<GameAction>(static_cast<int>(GameAction::HotbarSlot1) + i);
        if (input.isActionPressed(action)) {
            hotbar.setSelectedIndex(i);
        }
    }
}

void PlayerController::processInteractionInput() {
    InputManager& input = InputManager::getInstance();

    // Only interact when cursor is captured (not in menu)
    if (input.isCursorVisible()) return;

    RaycastResult target = getTargetedBlock();

    // TODO: Change back to isActionHeld and add a delay to it
    if (input.isActionPressed(GameAction::PrimaryAction)) {
        if (target.hit) {
            world->setBlock(target.hitPos.x, target.hitPos.y, target.hitPos.z, BlockType::Air);
        }
    }

    if (input.isActionPressed(GameAction::SecondaryAction)) {
        const BlockType held = player->getHotbar().getSelected();
        if (target.hit && held != BlockType::Air) {
            glm::ivec3 placePos = target.hitPos - target.hitNormal;
            world->setBlock(placePos.x, placePos.y, placePos.z, held);
        }
    }

    if (input.isActionPressed(GameAction::TertiaryAction)) {
        if (target.hit) {
            BlockType targetBlock = world->getBlock(target.hitPos.x, target.hitPos.y, target.hitPos.z);
            player->getHotbar().setSelected(targetBlock);
        }
    }
}

void PlayerController::processUIInput() {
    InputManager& input = InputManager::getInstance();

    // Toggle pause/menu (works in any context)
    if (input.isActionPressed(GameAction::TogglePause)) {
        if (input.getCurrentContext() == InputContext::Gameplay) {
            input.pushContext(InputContext::UI);
        } else if (input.getCurrentContext() == InputContext::UI) {
            input.popContext();
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
    if (input.isActionPressed(GameAction::ToggleHitBox)) {
        world->getEntityManager()->toggleHitboxes();
    }

    if (input.isActionPressed(GameAction::ToggleDebug)) {
        // TODO: Toggle debug HUD
        std::cout << "Toggled debug overlay" << std::endl;
    }

    // Take screenshot
    if (input.isActionPressed(GameAction::Screenshot)) {
        // TODO: Capture and save screenshot
        std::cout << "Screenshot saved!" << std::endl;
    }
}

RaycastResult PlayerController::getTargetedBlock() const {
    return world->raycastBlock(player->getCamera().Position,player->getFront(),5.0f);
}
