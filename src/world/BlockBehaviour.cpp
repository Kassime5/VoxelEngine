//
// Created by maxim on 26/08/2026.
//

#include "BlockBehaviour.h"

#include "World.h"

namespace {

// Anything opaque directly above smothers a surface block
bool isCovered(World& world, const glm::ivec3& pos) {
    return isBlockOpaque(world.getBlock(pos.x, pos.y + 1, pos.z));
}

void grassNeighbourChanged(World& world, const glm::ivec3& pos) {
    if (isCovered(world, pos)) {
        world.setBlockLogic(pos, BlockType::Dirt);
    }
}

void dirtRandomTick(World& world, const glm::ivec3& pos) {
    if (isCovered(world, pos)) {
        return;
    }

    const glm::ivec3 offsets[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

    // Spreads from any uncovered grass alongside, including one step up or down
    for (const glm::ivec3& offset : offsets) {
        for (int dy = -1; dy <= 1; dy++) {
            const glm::ivec3 source = pos + offset + glm::ivec3(0, dy, 0);
            if (world.getBlock(source.x, source.y, source.z) != BlockType::Grass) {
                continue;
            }
            if (isCovered(world, source)) {
                continue;
            }
            world.setBlockLogic(pos, BlockType::Grass);
            return;
        }
    }
}

void tallGrassNeighbourChanged(World& world, const glm::ivec3& pos) {
    if (!isGroundBlock(world.getBlock(pos.x, pos.y - 1, pos.z))) {
        world.setBlockLogic(pos, BlockType::Air);
    }
}

constexpr BlockBehaviour INERT{};
constexpr BlockBehaviour GRASS{grassNeighbourChanged, nullptr};
constexpr BlockBehaviour DIRT{nullptr, dirtRandomTick};
constexpr BlockBehaviour TALL_GRASS{tallGrassNeighbourChanged, nullptr};

}

const BlockBehaviour& blockBehaviour(BlockType type) {
    switch (type) {
    case BlockType::Grass:
        return GRASS;
    case BlockType::Dirt:
        return DIRT;
    case BlockType::TallGrass:
        return TALL_GRASS;
    default:
        return INERT;
    }
}
