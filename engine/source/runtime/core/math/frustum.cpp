#include "frustum.h"

#include <assert.h>

namespace Vain {

Frustum::Frustum(
    const glm::mat4 &proj,
    float ndc_left,
    float ndc_right,
    float ndc_top,
    float ndc_bottom,
    float ndc_near,
    float ndc_far
) {
    assert(ndc_left < ndc_right);
    assert(ndc_top < ndc_bottom);
    assert(ndc_near < ndc_far);

    glm::vec4 row[4] = {
        {proj[0][0], proj[1][0], proj[2][0], proj[3][0]},
        {proj[0][1], proj[1][1], proj[2][1], proj[3][1]},
        {proj[0][2], proj[1][2], proj[2][2], proj[3][2]},
        {proj[0][3], proj[1][3], proj[2][3], proj[3][3]},
    };

    // out normal

    right_plane = row[0] - ndc_right * row[3];
    right_plane /= glm::vec3{right_plane}.length();

    left_plane = ndc_left * row[3] - row[0];
    left_plane /= glm::vec3{left_plane}.length();

    bottom_plane = row[1] - ndc_bottom * row[3];
    bottom_plane /= glm::vec3{bottom_plane}.length();

    top_plane = ndc_top * row[3] - row[1];
    top_plane /= glm::vec3{top_plane}.length();

    far_plane = row[2] - ndc_far * row[3];
    far_plane /= glm::vec3{far_plane}.length();

    near_plane = ndc_near * row[3] - row[2];
    near_plane /= glm::vec3{near_plane}.length();
}

bool Frustum::intersect(const AxisAlignedBoundingBox &aabb) {
    {
        float signed_distance = glm::dot(right_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(right_plane.x), fabs(right_plane.y), fabs(right_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    {
        float signed_distance = glm::dot(left_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(left_plane.x), fabs(left_plane.y), fabs(left_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    {
        float signed_distance = glm::dot(bottom_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(bottom_plane.x), fabs(bottom_plane.y), fabs(bottom_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    {
        float signed_distance = glm::dot(top_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(top_plane.x), fabs(top_plane.y), fabs(top_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    {
        float signed_distance = glm::dot(far_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(far_plane.x), fabs(far_plane.y), fabs(far_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    {
        float signed_distance = glm::dot(near_plane, glm::vec4{aabb.center, 1.0});
        float radius_project = glm::dot(
            glm::vec3{fabs(near_plane.x), fabs(near_plane.y), fabs(near_plane.z)},
            aabb.half_extent
        );

        bool intersecting_or_inside = signed_distance < radius_project;
        if (!intersecting_or_inside) {
            return false;
        }
    }

    return true;
}

}  // namespace Vain