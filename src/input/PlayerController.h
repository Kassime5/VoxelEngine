//
// Created by maxim on 12/02/2026.
//

#ifndef GLFWVOXEL_PLAYERCONTROLLER_H
#define GLFWVOXEL_PLAYERCONTROLLER_H

#include "InputManager.h"
#include "src/world/World.h"
#include "src/game/Player.h"

class PlayerController {
public:
    PlayerController(Player* player, World* world);

    void processInput(float deltaTime);

    void setControlEnabled(bool enabled) { controlEnabled = enabled; }
    bool isControlEnabled() const { return controlEnabled; }

private:
    Player* player;
    World* world;
    bool controlEnabled;

    void processMovementInput(float deltaTime);
    void processCameraInput(float deltaTime);
    void processInteractionInput();
    void processUIInput();
    void processDebugInput();

    RaycastResult getTargetedBlock() const;
};


#endif //GLFWVOXEL_PLAYERCONTROLLER_H