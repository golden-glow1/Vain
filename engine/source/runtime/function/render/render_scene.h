#pragma once

#include "function/render/render_entity.h"
#include "function/render/render_object.h"
#include "resource/asset_guid_allocator.h"
#include "resource/asset_type.h"

namespace Vain {

class MeshResource;
class PBRMaterialResource;
class RenderCamera;
class RenderResource;

struct RenderMeshNode {
    size_t entity_id{};
    glm::mat4 model_matrix{1.0};
    const MeshResource *ref_mesh{};
    const PBRMaterialResource *ref_material{};
};

class RenderScene {
  public:
    AssetGuidAllocator<GObjectPartID> entity_id_allocator{};
    AssetGuidAllocator<MeshDesc> mesh_guid_allocator{};
    AssetGuidAllocator<PBRMaterialDesc> pbr_material_guid_allocator{};

    Color ambient_light{};
    DirectionalLightDesc directional_light{};
    std::vector<PointLightDesc> point_lights{};

    std::vector<RenderEntity> render_entities{};

    std::vector<RenderMeshNode> directional_light_visible_mesh_nodes{};
    std::vector<RenderMeshNode> point_lights_visible_mesh_nodes{};
    std::vector<RenderMeshNode> main_camera_visible_mesh_nodes{};

    RenderScene() = default;
    ~RenderScene();

    void clear();

    void updateVisibleNodes(RenderResource &resource, RenderCamera &camera);

    void clearForReloading();

  private:
    std::unordered_map<MeshDesc, AxisAlignedBoundingBox> m_aabb_cache{};

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