#include "render_scene.h"

#include "core/math/frustum.h"
#include "function/render/render_camera.h"
#include "function/render/render_resource.h"

namespace Vain {

RenderScene::~RenderScene() { clear(); }

void RenderScene::clear() {}

void RenderScene::updateVisibleNodes(RenderResource &resource, RenderCamera &camera) {
    updateVisibleNodesDirectionalLight(resource, camera);
    updateVisibleNodesPointLights(resource, camera);
    updateVisibleNodesMainCamera(resource, camera);
}

void RenderScene::clearForReloading() { render_entities.clear(); }

void RenderScene::updateVisibleNodesDirectionalLight(
    RenderResource &resource, RenderCamera &camera
) {
    directional_light_visible_mesh_nodes.clear();

    glm::mat4 light_proj_view = calculateDirectionalLightView(*this, camera);
    resource.mesh_per_frame_storage_buffer_object.directional_light_proj_view =
        light_proj_view;
    resource.directional_light_shadow_per_frame_storage_buffer_object.light_proj_view =
        light_proj_view;

    Frustum frustum{light_proj_view, -1.0, 1.0, -1.0, 1.0, 0.0, 1.0};

    for (const auto &entity : render_entities) {
        if (!frustum.intersect(boundingBoxTransform(entity->aabb, entity->model_matrix))) {
            continue;
        }

        directional_light_visible_mesh_nodes.emplace_back();
        RenderNode &node = directional_light_visible_mesh_nodes.back();

        node.model_matrix = entity->model_matrix;

        node.ref_mesh = resource.getEntityMesh(*entity);
        node.ref_material = resource.getEntityMaterial(*entity);
    }
}

void RenderScene::updateVisibleNodesPointLights(
    RenderResource &resource, RenderCamera &camera
) {
    point_lights_visible_mesh_nodes.clear();

    for (const auto &entity : render_entities) {
        bool intersect = false;
        for (const auto &point_light : point_lights) {
            intersect = boundingBoxTransform(entity->aabb, entity->model_matrix)
                            .intersect(point_light.position, point_light.getRadius());
            if (intersect) {
                break;
            }
        }

        if (!intersect) {
            continue;
        }

        point_lights_visible_mesh_nodes.emplace_back();
        RenderNode &node = point_lights_visible_mesh_nodes.back();

        node.model_matrix = entity->model_matrix;

        node.ref_mesh = resource.getEntityMesh(*entity);
        node.ref_material = resource.getEntityMaterial(*entity);
    }
}

void RenderScene::updateVisibleNodesMainCamera(
    RenderResource &resource, RenderCamera &camera
) {
    main_camera_visible_mesh_nodes.clear();

    glm::mat4 proj_view_matrix = camera.projection() * camera.view();
    Frustum frustum{proj_view_matrix, -1.0, 1.0, -1.0, 1.0, 0.0, 1.0};

    for (const auto &entity : render_entities) {
        if (!frustum.intersect(boundingBoxTransform(entity->aabb, entity->model_matrix))) {
            continue;
        }

        main_camera_visible_mesh_nodes.emplace_back();
        RenderNode &node = main_camera_visible_mesh_nodes.back();

        node.model_matrix = entity->model_matrix;

        node.ref_mesh = resource.getEntityMesh(*entity);
        node.ref_material = resource.getEntityMaterial(*entity);
    }
}

glm::mat4 calculateDirectionalLightView(
    const RenderScene &scene, const RenderCamera &camera
) {
    glm::mat4 proj_view_matrix = camera.projection() * camera.view();

    AxisAlignedBoundingBox frustum_bounding_box{};
    {
        glm::mat4 inverse_proj_view_matrix = glm::inverse(proj_view_matrix);

        constexpr glm::vec3 k_frustum_points_ndc_space[8] = {
            glm::vec3{-1.0f, -1.0f, 1.0f},
            glm::vec3{ 1.0f, -1.0f, 1.0f},
            glm::vec3{ 1.0f,  1.0f, 1.0f},
            glm::vec3{-1.0f,  1.0f, 1.0f},
            glm::vec3{-1.0f, -1.0f, 0.0f},
            glm::vec3{ 1.0f, -1.0f, 0.0f},
            glm::vec3{ 1.0f,  1.0f, 0.0f},
            glm::vec3{-1.0f,  1.0f, 0.0f}
        };

        for (const auto &ndc_point : k_frustum_points_ndc_space) {
            glm::vec4 frustum_point_with_w =
                inverse_proj_view_matrix * glm::vec4{ndc_point, 1.0};
            glm::vec3 frustum_point =
                glm::vec3{frustum_point_with_w} / frustum_point_with_w.w;

            frustum_bounding_box.merge(frustum_point);
        }
    }

    AxisAlignedBoundingBox scene_bounding_box;
    for (const auto &entity : scene.render_entities) {
        auto mesh_bounding_box = boundingBoxTransform(entity->aabb, entity->model_matrix);
        scene_bounding_box.merge(mesh_bounding_box);
    }

    glm::mat4 light_proj{1.0}, light_view{1.0};
    {
        float extent_len = frustum_bounding_box.half_extent.length();
        glm::vec3 eye =
            frustum_bounding_box.center + scene.directional_light.direction * extent_len;
        light_view =
            glm::lookAt(eye, frustum_bounding_box.center, glm::vec3{0.0, 1.0, 0.0});

        AxisAlignedBoundingBox light_view_frustum_bounding_box =
            boundingBoxTransform(frustum_bounding_box, light_view);
        AxisAlignedBoundingBox light_view_scene_bounding_box =
            boundingBoxTransform(scene_bounding_box, light_view);

        glm::vec3 fmin = light_view_frustum_bounding_box.minCorner();
        glm::vec3 fmax = light_view_frustum_bounding_box.maxCorner();

        glm::vec3 smin = light_view_scene_bounding_box.minCorner();
        glm::vec3 smax = light_view_scene_bounding_box.maxCorner();

        glm::mat4 fix{1.0};
        fix[1][1] = -1.0;

        if (!light_view_scene_bounding_box.empty()) {
            light_proj = glm::ortho(
                std::max(fmin.x, smin.x),
                std::min(fmax.x, smax.x),
                std::max(fmin.y, smin.y),
                std::min(fmax.y, smax.y),
                -smax.z,
                -std::max(fmin.z, smin.z)
            );
        } else {
            light_proj = glm::ortho(fmin.x, fmax.x, fmin.y, fmax.y, -fmax.z, -fmin.z);
        }
        light_proj = fix * light_proj;
    }

    return light_proj * light_view;
}

}  // namespace Vain