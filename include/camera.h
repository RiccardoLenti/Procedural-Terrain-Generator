#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>


#include <glm/glm.hpp>

class Camera {
   public:
    glm::vec3 position = {0.f, 100.f, 0.f};
    float yaw = -90.f;
    float pitch = -30.f;

    float move_speed = 60.f;
    float mouse_sens = 0.05f;

    void update(GLFWwindow* window, float dt);

    void process_mouse(float dx, float dy);

    glm::mat4 view_matrix() const;
    glm::mat4 proj_matrix(float aspect) const;

   private:
    glm::vec3 forward() const;
    glm::vec3 right() const;
};