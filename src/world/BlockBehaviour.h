//
// Created by maxim on 26/08/2026.
//

#ifndef GLFWVOXEL_BLOCKBEHAVIOUR_H
#define GLFWVOXEL_BLOCKBEHAVIOUR_H

#include <glm/glm.hpp>

#include "Block.h"

class World;

using BlockUpdateFn = void (*)(World&, const glm::ivec3&);

// onNeighbourChanged fires when a block touching this one changes
// onRandomTick on the random sweep
struct BlockBehaviour {
    BlockUpdateFn onNeighbourChanged = nullptr;
    BlockUpdateFn onRandomTick = nullptr;
};

const BlockBehaviour& blockBehaviour(BlockType type);

#endif //GLFWVOXEL_BLOCKBEHAVIOUR_H
