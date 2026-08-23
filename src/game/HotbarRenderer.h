//
// Created by maxim on 23/08/2026.
//

#ifndef GLFWVOXEL_HOTBARRENDERER_H
#define GLFWVOXEL_HOTBARRENDERER_H

#include "Hotbar.h"
#include "HUDRenderer.h"
#include "src/rendering/Texture.h"
#include "src/rendering/TextureAltas.h"

#include <cmath>

namespace hotbar_ui {
    constexpr float SHEET_W = 192.0f;
    constexpr float SHEET_H = 32.0f;

    // Texture::load flips vertically, so v is measured up from the bottom of the sheet
    constexpr glm::vec4 uv(float x, float y, float w, float h) {
        return glm::vec4(x / SHEET_W, 1.0f - (y + h) / SHEET_H,
                         (x + w) / SHEET_W, 1.0f - y / SHEET_H);
    }
}

// Tiles one slot sprite across the bar and lays the wider selector over the live slot.
class HotbarRenderer {
public:
    HotbarRenderer() {
        if (uiTexture.load("assets/ui/hotbar-ui.png", false)) {
            uiTexture.setFilterMode(Texture::FilterMode::Nearest, Texture::FilterMode::Nearest);
            uiTexture.setWrapMode(Texture::WrapMode::ClampToEdge, Texture::WrapMode::ClampToEdge);
        }
    }

    void draw(const HUDRenderer& hud, const Hotbar& hotbar, const TextureAtlas& atlas) const {
        if (!uiTexture.isValid()) return;

        const int scale = hud.getUIScale();
        const float slot = SLOT_PX * static_cast<float>(scale);
        const float icon = ICON_PX * static_cast<float>(scale);
        const float barWidth = slot * Hotbar::SLOT_COUNT;

        // Snapped to a whole pixel, otherwise the 1px borders land between texels
        const float left = std::floor((static_cast<float>(hud.getScreenWidth()) - barWidth) * 0.5f);
        const float centreY = MARGIN_PX * static_cast<float>(scale) + slot * 0.5f;

        for (int i = 0; i < Hotbar::SLOT_COUNT; ++i) {
            const float centreX = left + (static_cast<float>(i) + 0.5f) * slot;
            hud.drawSprite(centreX, centreY, slot, slot, uiTexture.getID(), SLOT_UV);

            const BlockType block = hotbar.getSlot(i);
            if (block != BlockType::Air && atlas.isLoaded()) {
                hud.drawTile(centreX, centreY, icon, icon, atlas.getTextureId(),
                             atlas.getBlockFaceTileIndex(block, BlockFace::Front));
            }
        }

        // Drawn last: the selector is 1px wider per side and overhangs its neighbours
        const float highlight = HIGHLIGHT_PX * static_cast<float>(scale);
        const float selectedX = left + (static_cast<float>(hotbar.getSelectedIndex()) + 0.5f) * slot;
        hud.drawSprite(selectedX, centreY, highlight, highlight, uiTexture.getID(), HIGHLIGHT_UV);
    }

private:
    // Visible art inside the sheet's 32px cells. The slot's 2px border leaves a 16px well.
    static constexpr float SLOT_PX = 20.0f;
    static constexpr float HIGHLIGHT_PX = 22.0f;
    static constexpr float ICON_PX = 16.0f;
    static constexpr float MARGIN_PX = 6.0f;

    static constexpr glm::vec4 SLOT_UV = hotbar_ui::uv(6.0f, 6.0f, SLOT_PX, SLOT_PX);
    static constexpr glm::vec4 HIGHLIGHT_UV = hotbar_ui::uv(37.0f, 5.0f, HIGHLIGHT_PX, HIGHLIGHT_PX);

    Texture uiTexture;
};

#endif //GLFWVOXEL_HOTBARRENDERER_H
