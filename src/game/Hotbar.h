//
// Created by maxim on 23/08/2026.
//

#ifndef GLFWVOXEL_HOTBAR_H
#define GLFWVOXEL_HOTBAR_H

#include "src/world/Block.h"

#include <array>

class Hotbar {
public:
    static constexpr int SLOT_COUNT = 10;

    BlockType getSlot(int index) const {
        return isValidIndex(index) ? slots[index] : BlockType::Air;
    }

    void setSlot(int index, BlockType block) {
        if (isValidIndex(index)) slots[index] = block;
    }

    int getSelectedIndex() const { return selectedIndex; }

    void setSelectedIndex(int index) {
        if (isValidIndex(index)) selectedIndex = index;
    }

    BlockType getSelected() const { return slots[selectedIndex]; }

    void setSelected(BlockType block) {
        // Block picking first checks if the block is already in the hotbar
        for (int i = 0; i < SLOT_COUNT; i++) {
            if (slots[i] == block) {
                selectedIndex = i;
                return;
            }
        }
        // otherwise it assigns it to the current slot
        slots[selectedIndex] = block;
    }

    void cycle(int delta) {
        if (delta == 0) return;
        selectedIndex = ((selectedIndex + delta) % SLOT_COUNT + SLOT_COUNT) % SLOT_COUNT;
    }

private:
    static bool isValidIndex(int index) { return index >= 0 && index < SLOT_COUNT; }

    // Every placeable block for now
    std::array<BlockType, SLOT_COUNT> slots{
        BlockType::Grass, BlockType::Dirt,   BlockType::Stone,  BlockType::Sand,
        BlockType::Snow,  BlockType::Wood,   BlockType::Leaves, BlockType::TallGrass,
        BlockType::Air,   BlockType::Air,
    };
    int selectedIndex = 0;
};

#endif //GLFWVOXEL_HOTBAR_H
