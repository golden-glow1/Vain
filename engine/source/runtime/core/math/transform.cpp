#include "transform.h"

namespace Vain {

glm::mat4 Transform::matrix() const {
    glm::mat4 transform{1.0};

    transform = glm::scale(transform, scale);
    transform *= glm::mat4_cast(rotation);
    transform = glm::translate(transform, translation);

    return transform;
}

}  // namespace Vain
