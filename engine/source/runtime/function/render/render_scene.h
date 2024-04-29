#pragma once

#include <memory>
#include <unordered_set>

#include "function/render/render_entity.h"
#include "resource/asset_guid_allocator.h"
#include "resource/asset_type.h"

namespace Vain {

class MeshResource;
class PBRMaterialResource;
class RenderCamera;
class RenderResource;

struct RenderNode {
    glm::mat4 model_matrix{1.0};
    const MeshResource *ref_mesh{};
    const PBRMaterialResource *ref_material{};
};

class RenderScene {
  public:
    AssetGuidAllocator<MeshDesc> mesh_guid_allocator{};
    AssetGuidAllocator<PBRMaterialDesc> material_guid_allocator{};

    Color ambient_light{};
    DirectionalLightDesc directional_light{};
    std::vector<PointLightDesc> point_lights{};

    std::unordered_set<std::shared_ptr<RenderEntity>> render_entities{};

    std::vector<RenderNode> directional_light_visible_mesh_nodes{};
    std::vector<RenderNode> point_lights_visible_mesh_nodes{};
    std::vector<RenderNode> main_camera_visible_mesh_nodes{};

    RenderScene() = default;
    ~RenderScene();

    void clear();

    void updateVisibleNodes(RenderResource &resource, RenderCamera &camera);

    void clearForReloading();

  private:
    void updateVisibleNodesDirectionalLight(
        RenderResource &resource, RenderCamera &camera
    );
    void updateVisibleNodesPointLights(RenderResource &resource, RenderCamera &camera);
    void updateVisibleNodesMainCamera(RenderResource &resource, RenderCamera &camera);
};

glm::mat4 calculateDirectionalLightView(
    const RenderScene &scene, const RenderCamera &camera
);

}  // namespace Vain