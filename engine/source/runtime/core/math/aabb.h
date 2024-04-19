#pragma once

#include <glm/glm.hpp>
#include <limits>

namespace Vain {

class AxisAlignedBoundingBox {
  public:
    glm::vec3 center{};
    glm::vec3 half_extent{
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };

    glm::vec3 minCorner() const;
    glm::vec3 maxCorner() const;

    void merge(const glm::vec3 &point);

    void merge(const AxisAlignedBoundingBox &aabb);

    void join(const AxisAlignedBoundingBox &aabb);

    void transform(const glm::mat4 &mat);

    bool intersect(const glm::vec3 &center, float radius) const;
};

AxisAlignedBoundingBox boundingBoxTransform(
    const AxisAlignedBoundingBox &aabb, const glm::mat4 &transform
);

}  // namespace Vain