//
// Created by maxim on 24/08/2026.
//

#include "ShadowMap.h"

#include "src/core/GL.h"
#include "src/world/Chunk.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>

ShadowMap::ShadowMap() {
    allocate(settings.resolution, settings.cascadeCount);
}

ShadowMap::~ShadowMap() {
    destroy();
}

bool ShadowMap::update(const SunState& sun, const glm::vec3& viewPos) {
    const float elevation = sun.direction.y;

    if (!settings.enabled || elevation <= MIN_SUN_ELEVATION) {
        effectiveStrength = 0.0f;
        return false;
    }

    const int cascades = std::clamp(settings.cascadeCount, 1, MAX_CASCADES);
    if (settings.resolution != allocatedResolution || cascades != allocatedCascades) {
        allocate(settings.resolution, cascades);
    }
    if (fbo == 0) {
        effectiveStrength = 0.0f;
        return false;
    }
    activeCascades = allocatedCascades;

    effectiveStrength =
        settings.strength * glm::smoothstep(MIN_SUN_ELEVATION, FULL_SUN_ELEVATION, elevation);

    glm::vec3 direction = sun.direction;

    const float depthMargin = std::min(static_cast<float>(Chunk::HEIGHT) / direction.y, MAX_DEPTH_MARGIN);

    const glm::vec3 up = DayCycle::getOrbitAxis();
    const glm::mat4 lightRotation = glm::lookAt(glm::vec3(0.0f), -direction, up);
    const glm::mat4 inverseRotation = glm::inverse(lightRotation);

    const float nearRadius = std::max(settings.nearRadius, 1.0f);
    const float farRadius = std::max(settings.radius, nearRadius);

    for (int i = 0; i < activeCascades; ++i) {
        const float t = activeCascades > 1
            ? static_cast<float>(i) / static_cast<float>(activeCascades - 1)
            : 1.0f;
        const float radius = nearRadius * std::pow(farRadius / nearRadius, t);
        splitDistances[i] = radius;

        const float texel = 2.0f * radius / static_cast<float>(settings.resolution);
        glm::vec3 centreLS = glm::vec3(lightRotation * glm::vec4(viewPos, 1.0f));
        centreLS.x = std::floor(centreLS.x / texel) * texel;
        centreLS.y = std::floor(centreLS.y / texel) * texel;

        const glm::vec3 centre = glm::vec3(inverseRotation * glm::vec4(centreLS, 1.0f));
        const float depthRange = 2.0f * radius + depthMargin;

        const glm::mat4 lightView = glm::lookAt(centre + direction * (radius + depthMargin), centre, up);
        const glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, 0.0f, depthRange);

        lightSpaceMatrices[i] = lightProjection * lightView;
    }

    return true;
}

void ShadowMap::beginPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, allocatedResolution, allocatedResolution);

    // off by default
    frontFacesCulled = settings.cullFrontFaces;
    if (frontFacesCulled) {
        glCullFace(GL_FRONT);
    }

    if (settings.polygonOffsetFactor != 0.0f || settings.polygonOffsetUnits != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(settings.polygonOffsetFactor, settings.polygonOffsetUnits);
    }
}

void ShadowMap::beginCascade(int cascade) {
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, cascade);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::endPass(int viewportWidth, int viewportHeight) const {
    if (frontFacesCulled) {
        glCullFace(GL_BACK);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void ShadowMap::bindForRead(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthTexture);
}

void ShadowMap::setDepthCompare(bool compareEnabled) const {
    if (depthTexture == 0) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthTexture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE,
                    compareEnabled ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void ShadowMap::allocate(int resolution, int cascades) {
    destroy();

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, resolution, resolution, cascades,
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0, 0);

    // Depth only. glDrawBuffer is desktop-only; the plural form exists on both targets.
    constexpr GLenum noColour = GL_NONE;
    glDrawBuffers(1, &noColour);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow map incomplete at " << resolution << "px x " << cascades
                  << " cascades (0x" << std::hex << status << std::dec << ")" << std::endl;
        destroy();
        return;
    }

    allocatedResolution = resolution;
    allocatedCascades = cascades;
}

void ShadowMap::destroy() {
    if (fbo != 0) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (depthTexture != 0) {
        glDeleteTextures(1, &depthTexture);
        depthTexture = 0;
    }
    allocatedResolution = 0;
    allocatedCascades = 0;
    activeCascades = 0;
}
