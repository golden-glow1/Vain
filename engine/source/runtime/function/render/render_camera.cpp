#include "render_camera.h"

#include <algorithm>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

namespace Vain {

void RenderCamera::lookAt(
    const glm::vec3 &position, const glm::vec3 &target, const glm::vec3 &world_up
) {
    this->position = position;
    this->world_up = world_up;

    glm::mat4 view_mat = glm::lookAt(position, target, world_up);
    this->rotation = glm::quat_cast(view_mat);
}

void RenderCamera::move(const glm::vec3 &delta) { position += delta; }

void RenderCamera::rotate(const glm::vec2 &delta) {
    float x = glm::radians(delta.x);
    float y = glm::radians(delta.y);

    float dot = glm::dot(world_up, front());
    if ((dot < -0.99 && delta.x > 0.0) || (dot > 0.99 && delta.x < 0.0)) {
        x = 0.0;
    }

    glm::quat pitch = glm::angleAxis(x, glm::vec3{1.0, 0.0, 0.0});
    glm::quat yaw = glm::angleAxis(y, glm::vec3{0.0, 1.0, 0.0});

    rotation = pitch * rotation * yaw;
}

void RenderCamera::zoom(float offset) {
    fovy = std::clamp(fovy - offset, k_min_fov, k_max_fov);
}

glm::vec3 RenderCamera::front() const {
    return glm::conjugate(rotation) * glm::vec3{0.0, 0.0, -1.0};
}

glm::vec3 RenderCamera::right() const {
    return glm::conjugate(rotation) * glm::vec3{1.0, 0.0, 0.0};
}

glm::vec3 RenderCamera::up() const {
    return glm::conjugate(rotation) * glm::vec3{0.0, 1.0, 0.0};
}

glm::mat4 RenderCamera::view() const {
    return glm::lookAt(position, position + front(), up());
}

glm::mat4 RenderCamera::projection() const {
    glm::mat4 proj = glm::perspective(glm::radians(fovy), aspect, znear, zfar);
    proj[1][1] *= -1;
    return proj;
}

}  // namespace Vain