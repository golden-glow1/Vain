#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Vain {

class RenderCamera {
  public:
    static constexpr float k_min_fov{10.0};
    static constexpr float k_max_fov{89.0};
    static constexpr int k_main_view_matrix_index{0};

    glm::vec3 position{};
    glm::quat rotation{1.0, 0.0, 0.0, 0.0};
    glm::vec3 world_up{0.0, 1.0, 0.0};

    float znear{0.1};
    float zfar{1000.0};
    float fovy{45.0};
    float aspect{1.0};

    void lookAt(
        const glm::vec3 &position, const glm::vec3 &target, const glm::vec3 &world_up
    );

    void move(const glm::vec3 &delta);
    void rotate(const glm::vec2 &delta);
    void zoom(float offset);

    glm::vec3 front() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    glm::mat4 view() const;
    glm::mat4 projection() const;
};

}  // namespace Vain
