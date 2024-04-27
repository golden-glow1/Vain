#pragma once

#include <array>

#include "core/vulkan/vulkan_context.h"
#include "function/render/render_data.h"
#include "function/render/render_entity.h"
#include "function/render/render_type.h"

namespace Vain {

class RenderCamera;
class RenderScene;

struct IBLResource {
    VkImage brdfLUT_texture_image{};
    VkImageView brdfLUT_texture_image_view{};
    VkSampler brdfLUT_texture_sampler{};
    VmaAllocation brdfLUT_texture_image_allocation{};

    VkImage irradiance_texture_image{};
    VkImageView irradiance_texture_image_view{};
    VkSampler irradiance_texture_sampler{};
    VmaAllocation irradiance_texture_image_allocation{};

    VkImage specular_texture_image{};
    VkImageView specular_texture_image_view{};
    VkSampler specular_texture_sampler{};
    VmaAllocation specular_texture_image_allocation{};
};

struct StorageBuffer {
    // limits
    VkDeviceSize min_uniform_buffer_offset_alignment{256};
    VkDeviceSize min_storage_buffer_offset_alignment{256};
    uint32_t max_storage_buffer_range{1 << 27};
    uint32_t non_coherent_atom_size{256};

    VkBuffer global_upload_ringbuffer{};
    VkDeviceMemory global_upload_ringbuffer_memory{};
    void *global_upload_ringbuffer_memory_pointer{};
    std::vector<uint32_t> global_upload_ringbuffers_begin{};
    std::vector<uint32_t> global_upload_ringbuffers_end{};
    std::vector<uint32_t> global_upload_ringbuffers_size{};
};

struct GlobalRenderResource {
    IBLResource ibl_resource{};
    StorageBuffer storage_buffer{};
};

struct MeshResource {
    uint32_t vertex_count{};
    VkBuffer vertex_buffer{};
    VmaAllocation vertex_buffer_allocation{};

    uint32_t index_count{};
    VkBuffer index_buffer{};
    VmaAllocation index_buffer_allocation{};
};

struct PBRMaterialResource {
    VkImage base_color_texture_image{};
    VkImageView base_color_image_view{};
    VmaAllocation base_color_image_allocation{};

    VkImage metallic_texture_image{};
    VkImageView metallic_image_view{};
    VmaAllocation metallic_image_allocation{};

    VkImage roughness_texture_image{};
    VkImageView roughness_image_view{};
    VmaAllocation roughness_image_allocation{};

    VkImage normal_texture_image{};
    VkImageView normal_image_view{};
    VmaAllocation normal_image_allocation{};

    VkImage occlusion_texture_image{};
    VkImageView occlusion_image_view{};
    VmaAllocation occlusion_image_allocation{};

    VkImage emissive_texture_image{};
    VkImageView emissive_image_view{};
    VmaAllocation emissive_image_allocation{};

    VkBuffer material_uniform_buffer{};
    VmaAllocation material_uniform_buffer_allocation{};

    VkDescriptorSet material_descriptor_set{};
};

class RenderResource {
  public:
    GlobalRenderResource global_render_resource{};
    VkDescriptorSetLayout material_descriptor_set_layout{};

    MeshPerFrameStorageBufferObject mesh_per_frame_storage_buffer_object{};
    PointLightShadowPerFrameStorageBufferObject
        point_light_shadow_per_frame_storage_buffer_object{};
    DirectionalLightShadowPerFrameStorageBufferObject
        directional_light_shadow_per_frame_storage_buffer_object{};

    RenderResource() = default;
    ~RenderResource();

    void initialize(VulkanContext *ctx);
    void clear();

    void updatePerFrame(const RenderScene &scene, const RenderCamera &camera);

    void uploadGlobalRenderResource(const IBLDesc &ibl_desc);
    void uploadEntity(
        const RenderEntity &entity, const MeshData &mesh, const PBRMaterialData &material
    );
    void uploadMesh(const RenderEntity &entity, const MeshData &data);
    void uploadPBRMaterial(const RenderEntity &entity, const PBRMaterialData &data);

    void resetRingBufferOffset();

    const MeshResource *getEntityMesh(const RenderEntity &entity) const;
    const PBRMaterialResource *getEntityMaterial(const RenderEntity &entity) const;

    void freeMeshResource(const MeshResource &mesh);
    void freePBRMaterialResource(const PBRMaterialResource &material);

    void clearMesh();
    void clearMaterial();

  private:
    VulkanContext *m_ctx{};
    bool m_global_uploaded{false};
    std::unordered_map<size_t, MeshResource> m_mesh_map{};
    std::unordered_map<size_t, PBRMaterialResource> m_material_map{};

    void createAndMapStorageBuffer();
    void createIBLSamplers();
    void createIBLTextures(
        std::array<std::shared_ptr<TextureData>, 6> irradiance_maps,
        std::array<std::shared_ptr<TextureData>, 6> specular_maps
    );
    void freeIBLResource();

    void uploadVertexBuffer(
        MeshResource &mesh, const void *vertex_data, size_t vertex_buffer_size
    );
    void uploadIndexBuffer(
        MeshResource &mesh, const void *index_data, size_t index_buffer_size
    );

    void uploadMaterialUniformBuffer(
        PBRMaterialResource &material, const RenderEntity &entity
    );
    void freeTextureResource(VkImage image, VkImageView view, VmaAllocation allocation);
};

}  // namespace Vain
