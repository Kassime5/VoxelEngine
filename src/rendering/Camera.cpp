//
// Created by maxim on 01/01/2026.
//

#include "./Camera.h"

void Camera::ProcessKeyboard(Camera_Movement direction, bool sprinting, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;
    velocity = sprinting ? velocity * 5 : velocity;
    if (direction == FORWARD)
        Position += Front * velocity;
    if (direction == BACKWARD)
        Position -= Front * velocity;
    if (direction == LEFT)
        Position -= Right * velocity;
    if (direction == RIGHT)
        Position += Right * velocity;
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    if (Yaw > 180.0f)
        Yaw -= 360.0f;
    if (Yaw < 0.0f)
        Yaw += 360.0f;

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    Zoom -= (float)yoffset;
    if (Zoom < 1.0f)
        Zoom = 1.0f;
    if (Zoom > 45.0f)
        Zoom = 45.0f;
}

void Camera::updateCameraVectors() {
    // calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    // also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(Front, WorldUp));
    // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    Up = glm::normalize(glm::cross(Right, Front));
}

std::string Camera::facingCardinalDirection(){
    if (Yaw >= 337.5f || Yaw < 22.5f)
        return "North";
    if (Yaw >= 22.5f && Yaw < 67.5f)
        return "North-East";
    if (Yaw >= 67.5f && Yaw < 112.5f)
        return "East";
    if (Yaw >= 112.5f && Yaw < 157.5f)
        return "South-East";
    if (Yaw >= 157.5f && Yaw < 202.5f)
        return "South";
    if (Yaw >= 202.5f && Yaw < 247.5f)
        return "South-West";
    if (Yaw >= 247.5f && Yaw < 292.5f)
        return "West";
    return "North-West";
}
