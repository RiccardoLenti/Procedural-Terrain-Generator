#include "camera.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::forward() const {
    float yr = glm::radians(yaw);
    float pr = glm::radians(pitch);
    return glm::normalize(glm::vec3{std::cos(pr) * std::cos(yr), std::sin(pr), std::cos(pr) * std::sin(yr)});
}

glm::vec3 Camera::right() const { return glm::normalize(glm::cross(forward(), glm::vec3{0, 1, 0})); }

void Camera::process_mouse(float dx, float dy) {
    yaw += dx * mouse_sens;
    pitch -= dy * mouse_sens;
    pitch = std::clamp(pitch, -89.9f, 89.9f);
}

void Camera::update(GLFWwindow* window, float dt) {
    float speed = move_speed * dt;
    glm::vec3 fwd = forward();
    glm::vec3 rt = right();
    glm::vec3 up = {0, 1, 0};

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += fwd * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= fwd * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= rt * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += rt * speed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        position += up * speed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        position -= up * speed;
}

glm::mat4 Camera::view_matrix() const {
    return glm::lookAt(position, position + forward(), glm::vec3{0, 1, 0});
}

glm::mat4 Camera::proj_matrix(float aspect) const {
    return glm::perspective(glm::radians(60.0f), aspect, 0.1f, 2000.0f);
}