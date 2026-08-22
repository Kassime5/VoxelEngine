//
// Created by maxim on 12/08/2026.
//

#ifndef GLFWVOXEL_DAYCYCLE_H
#define GLFWVOXEL_DAYCYCLE_H

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <string>

struct SunState {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity; // 0 at night, 1 with the sun up
};

// Walks the sun around the sky on a fixed-length loop
class DayCycle {
public:
    explicit DayCycle(float dayLengthSeconds = 120.0f)
        : dayLength(dayLengthSeconds) {}

    void update(float deltaTime) {
        if (dayLength <= 0.0f) {
            return;
        }
        timeOfDay += deltaTime / dayLength;
        timeOfDay -= std::floor(timeOfDay);
    }

    float getTimeOfDay() const { return timeOfDay; }
    void setTimeOfDay(float fraction) { timeOfDay = fraction - std::floor(fraction); }

    float getDayLength() const { return dayLength; }
    void setDayLength(float seconds) { dayLength = seconds; }

    glm::vec3 getSunDirection() const {
        const float angle = (timeOfDay - 0.25f) * TWO_PI;
        return glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), TILT));
    }

    // The axis both bodies circle. Perpendicular to the direction by construction, so sky
    // billboards can build a basis off it without the degeneracy world up hits near noon.
    static constexpr glm::vec3 getOrbitAxis() { return ORBIT_AXIS; }

    float getSunIntensity() const {
        return glm::smoothstep(-0.10f, 0.25f, getSunDirection().y);
    }

    glm::vec3 getSunColor() const {
        const float elevation = getSunDirection().y;
        const glm::vec3 low = glm::mix(NIGHT_COLOR, HORIZON_COLOR,
                                       glm::smoothstep(-0.25f, 0.0f, elevation));
        return glm::mix(low, DAY_COLOR, glm::smoothstep(0.0f, 0.35f, elevation));
    }

    SunState getSun() const {
        return { getSunDirection(), getSunColor(), getSunIntensity() };
    }

    // 24-hour clock, for the debug overlay
    std::string getClockString() const {
        const int totalMinutes = static_cast<int>(timeOfDay * 24.0f * 60.0f) % (24 * 60);

        char buffer[6];
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", totalMinutes / 60, totalMinutes % 60);
        return buffer;
    }

private:
    static constexpr float TWO_PI = 6.28318530718f;
    // Keeps the sun off the pure XY plane so +Z and -Z faces shade differently.
    static constexpr float TILT = 0.25f;
    // Move the tilt onto another component and this has to follow it.
    static constexpr glm::vec3 ORBIT_AXIS{0.0f, 0.0f, 1.0f};

    static constexpr glm::vec3 DAY_COLOR{1.00f, 1.00f, 0.95f};
    static constexpr glm::vec3 HORIZON_COLOR{0.90f, 0.50f, 0.25f};
    static constexpr glm::vec3 NIGHT_COLOR{0.55f, 0.65f, 0.95f};

    // Starts a little after sunrise so a fresh world opens in daylight.
    float timeOfDay = 0.30f;
    float dayLength;
};

#endif //GLFWVOXEL_DAYCYCLE_H
