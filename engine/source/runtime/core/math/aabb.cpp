#include "aabb.h"

namespace Vain {

glm::vec3 AxisAlignedBoundingBox::minCorner() const { return center - half_extent; }

glm::vec3 AxisAlignedBoundingBox::maxCorner() const { return center + half_extent; }

void AxisAlignedBoundingBox::merge(const glm::vec3 &point) {
    glm::vec3 new_min = glm::min(minCorner(), point);
    glm::vec3 new_max = glm::max(maxCorner(), point);

    center = 0.5f * (new_min + new_max);
    half_extent = center - new_min;
}

void AxisAlignedBoundingBox::merge(const AxisAlignedBoundingBox &aabb) {
    glm::vec3 new_min = glm::min(minCorner(), aabb.minCorner());
    glm::vec3 new_max = glm::max(maxCorner(), aabb.maxCorner());

    center = 0.5f * (new_min + new_max);
    half_extent = center - new_min;
}

void AxisAlignedBoundingBox::join(const AxisAlignedBoundingBox &aabb) {
    glm::vec3 new_min = glm::max(minCorner(), aabb.minCorner());
    glm::vec3 new_max = glm::min(maxCorner(), aabb.maxCorner());

    if (new_min.x > new_max.x || new_min.y > new_max.y || new_min.z > new_max.z) {
        *this = AxisAlignedBoundingBox{};
        return;
    }

    center = 0.5f * (new_min + new_max);
    half_extent = center - new_min;
}

void AxisAlignedBoundingBox::transform(const glm::mat4 &mat) {
    *this = boundingBoxTransform(*this, mat);
}

bool AxisAlignedBoundingBox::intersect(const glm::vec3 &position, float radius) const {
    glm::vec3 min = minCorner();
    glm::vec3 max = minCorner();

    for (size_t i = 0; i < 3; ++i) {
        if (position[i] < min[i] && min[i] - position[i] > radius) {
            return false;
        } else if (position[i] > max[i] && position[i] - max[i] > radius) {
            return false;
        }
    }

    return true;
}

AxisAlignedBoundingBox boundingBoxTransform(
    const AxisAlignedBoundingBox &aabb, const glm::mat4 &transform
) {
    glm::vec3 const k_box_offset[8] = {
        glm::vec3{-1.0f, -1.0f,  1.0f},
        glm::vec3{ 1.0f, -1.0f,  1.0f},
        glm::vec3{ 1.0f,  1.0f,  1.0f},
        glm::vec3{-1.0f,  1.0f,  1.0f},
        glm::vec3{-1.0f, -1.0f, -1.0f},
        glm::vec3{ 1.0f, -1.0f, -1.0f},
        glm::vec3{ 1.0f,  1.0f, -1.0f},
        glm::vec3{-1.0f,  1.0f, -1.0f}
    };

    AxisAlignedBoundingBox transformed_aabb{};
    for (const auto &offset : k_box_offset) {
        glm::vec3 corner = aabb.center + aabb.half_extent * offset;
        glm::vec4 transformed_corner_with_w = transform * glm::vec4{corner, 1.0};
        glm::vec3 transformed_corner =
            glm::vec3{transformed_corner_with_w} / transformed_corner_with_w.w;

        transformed_aabb.merge(transformed_corner);
    }

    return transformed_aabb;
}

}  // namespace Vain