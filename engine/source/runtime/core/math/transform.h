#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Vain {

class Transform {
  public:
    glm::vec3 translation{};
    glm::vec3 scale{1.0};
    glm::quat rotation{glm::quat::wxyz(1.0, 0.0, 0.0, 0.0)};

    glm::mat4 matrix() const;

    bool operator==(const Transform &rhs) const {
        return translation == rhs.translation && scale == rhs.scale &&
               rotation == rhs.rotation;
    }

    bool operator!=(const Transform &rhs) const { return !this->operator==(rhs); }
};

}  // namespace Vain