#pragma once

#include "core/math/aabb.h"

namespace Vain {

class Frustum {
  public:
    glm::vec4 right_plane{};
    glm::vec4 left_plane{};
    glm::vec4 top_plane{};
    glm::vec4 bottom_plane{};
    glm::vec4 near_plane{};
    glm::vec4 far_plane{};

    Frustum(
        const glm::mat4 &proj,
        float ndc_left,
        float ndc_right,
        float ndc_top,
        float ndc_bottom,
        float ndc_near,
        float ndc_far
    );

    bool intersect(const AxisAlignedBoundingBox &aabb);
};

}  // namespace Vain