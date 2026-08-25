//
// Created by maxim on 24/08/2026.
//

#ifndef GLFWVOXEL_SHADOWMAP_H
#define GLFWVOXEL_SHADOWMAP_H

#include <glm/glm.hpp>

#include "src/world/DayCycle.h"

struct ShadowSettings {
    bool enabled = true;

    int resolution = 4096;
    int cascadeCount = 4;
    float nearRadius = 8.0f;
    float radius = 512.0f;


    float strength = 1.0f;

    // Acne is handled entirely by the depth pass's slope-scaled polygon offset
    float polygonOffsetFactor = 4.0f;
    float polygonOffsetUnits = 8.0f;
    bool cullFrontFaces = false;
    bool debugView = false;
};

class ShadowMap {
public:
    static constexpr int MAX_CASCADES = 4;

    ShadowMap();
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    ShadowSettings& getSettings() { return settings; }
    const ShadowSettings& getSettings() const { return settings; }

    // False means skip the depth pass this frame.
    bool update(const SunState& sun, const glm::vec3& viewPos);


    void beginPass();
    void beginCascade(int cascade);
    void endPass(int viewportWidth, int viewportHeight) const;

    void bindForRead(int unit) const;

    int getCascadeCount() const { return activeCascades; }
    const glm::mat4& getLightSpaceMatrix(int cascade) const { return lightSpaceMatrices[cascade]; }
    const glm::mat4* getLightSpaceMatrices() const { return lightSpaceMatrices; }

    // Outer radius of each cascade, which is also where the next one takes over
    const float* getSplitDistances() const { return splitDistances; }

    float getEffectiveStrength() const { return effectiveStrength; }

    float getTexelUV() const { return 1.0f / static_cast<float>(settings.resolution); }
    float getTexelWorldSize(int cascade) const {
        return 2.0f * splitDistances[cascade] / static_cast<float>(settings.resolution);
    }

    float getFadeStart() const { return settings.radius * FADE_START_FRACTION; }
    float getFadeEnd() const { return settings.radius; }
    float getCascadeBlend() const { return CASCADE_BLEND_FRACTION; }

    unsigned int getDepthTexture() const { return depthTexture; }
    void setDepthCompare(bool compareEnabled) const;

private:
    // Shadows fade in across this band of sun elevation and are off below it
    static constexpr float MIN_SUN_ELEVATION = 0.15f;
    static constexpr float FULL_SUN_ELEVATION = 0.25f;
    static constexpr float FADE_START_FRACTION = 0.72f;
    // Fraction of a cascade's radius spent blending into the next one
    static constexpr float CASCADE_BLEND_FRACTION = 0.12f;
    // Ceiling on how far up-sun the pass reaches for casters
    static constexpr float MAX_DEPTH_MARGIN = 512.0f;

    ShadowSettings settings;
    int allocatedResolution = 0;
    int allocatedCascades = 0;
    int activeCascades = 0;

    unsigned int fbo = 0;
    unsigned int depthTexture = 0;

    glm::mat4 lightSpaceMatrices[MAX_CASCADES]{};
    float splitDistances[MAX_CASCADES]{};

    float effectiveStrength = 0.0f;
    bool frontFacesCulled = false;

    void allocate(int resolution, int cascades);
    void destroy();
};

#endif //GLFWVOXEL_SHADOWMAP_H
