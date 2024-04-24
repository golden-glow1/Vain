#include "render_resource.h"

#include <unordered_map>
#include <vector>

#include "core/base/macro.h"
#include "core/vulkan/vulkan_utils.h"
#include "function/render/render_camera.h"
#include "function/render/render_scene.h"

namespace Vain {

RenderResource::~RenderResource() { clear(); }

void RenderResource::initialize(VulkanContext *ctx) { m_ctx = ctx; }

void RenderResource::clear() {
    clearMesh();

    clearMaterial();

    freeIBLResource();

    // destroy storage buffer
    vkDestroyBuffer(
        m_ctx->device,
        global_render_resource.storage_buffer.global_upload_ringbuffer,
        nullptr
    );
    vkFreeMemory(
        m_ctx->device,
        global_render_resource.storage_buffer.global_upload_ringbuffer_memory,
        nullptr
    );
}

void RenderResource::updatePerFrame(
    const RenderScene &scene, const RenderCamera &camera
) {
    glm::mat4 proj_view_matrix = camera.projection() * camera.view();

    glm::vec3 ambient_light = {
        scene.ambient_light.r, scene.ambient_light.g, scene.ambient_light.b
    };
    uint32_t point_light_num = static_cast<uint32_t>(scene.point_lights.size());

    mesh_per_frame_storage_buffer_object.proj_view_matrix = proj_view_matrix;
    mesh_per_frame_storage_buffer_object.camera_position = camera.position;
    mesh_per_frame_storage_buffer_object.ambient_light = ambient_light;
    mesh_per_frame_storage_buffer_object.point_light_num = point_light_num;

    for (uint32_t i = 0; i < point_light_num; ++i) {
        glm::vec3 position = scene.point_lights[i].position;
        glm::vec3 intensity = 0.25f * scene.point_lights[i].flux / glm::pi<float>();
        float radius = scene.point_lights[i].getRadius();

        mesh_per_frame_storage_buffer_object.scene_point_lights[i].position = position;
        mesh_per_frame_storage_buffer_object.scene_point_lights[i].intensity = intensity;
        mesh_per_frame_storage_buffer_object.scene_point_lights[i].radius = radius;

        point_light_shadow_per_frame_storage_buffer_object
            .point_lights_position_and_radius[i] = {position, radius};
    }

    mesh_per_frame_storage_buffer_object.scene_directional_light.color = {
        scene.directional_light.color.r,
        scene.directional_light.color.g,
        scene.directional_light.color.b
    };
    mesh_per_frame_storage_buffer_object.scene_directional_light.direction =
        glm::normalize(scene.directional_light.direction);
}

void RenderResource::uploadGlobalRenderResource(const IBLDesc &ibl_desc) {
    if (m_global_uploaded) {
        freeIBLResource();
        vkDestroyBuffer(
            m_ctx->device,
            global_render_resource.storage_buffer.global_upload_ringbuffer,
            nullptr
        );
        vkFreeMemory(
            m_ctx->device,
            global_render_resource.storage_buffer.global_upload_ringbuffer_memory,
            nullptr
        );
        m_global_uploaded = false;
    }

    createAndMapStorageBuffer();

    std::shared_ptr<TextureData> brdf_map = loadTextureHDR(ibl_desc.brdf_map);

    const SkyBoxDesc &skybox_irradiance_map = ibl_desc.skybox_irradiance_map;
    std::shared_ptr<TextureData> irradiace_pos_x_map =
        loadTextureHDR(skybox_irradiance_map.positive_x_map);
    std::shared_ptr<TextureData> irradiace_neg_x_map =
        loadTextureHDR(skybox_irradiance_map.negative_x_map);
    std::shared_ptr<TextureData> irradiace_pos_y_map =
        loadTextureHDR(skybox_irradiance_map.positive_y_map);
    std::shared_ptr<TextureData> irradiace_neg_y_map =
        loadTextureHDR(skybox_irradiance_map.negative_y_map);
    std::shared_ptr<TextureData> irradiace_pos_z_map =
        loadTextureHDR(skybox_irradiance_map.positive_z_map);
    std::shared_ptr<TextureData> irradiace_neg_z_map =
        loadTextureHDR(skybox_irradiance_map.negative_z_map);

    const SkyBoxDesc &skybox_specular_map = ibl_desc.skybox_specular_map;
    std::shared_ptr<TextureData> specular_pos_x_map =
        loadTextureHDR(skybox_specular_map.positive_x_map);
    std::shared_ptr<TextureData> specular_neg_x_map =
        loadTextureHDR(skybox_specular_map.negative_x_map);
    std::shared_ptr<TextureData> specular_pos_y_map =
        loadTextureHDR(skybox_specular_map.positive_y_map);
    std::shared_ptr<TextureData> specular_neg_y_map =
        loadTextureHDR(skybox_specular_map.negative_y_map);
    std::shared_ptr<TextureData> specular_pos_z_map =
        loadTextureHDR(skybox_specular_map.positive_z_map);
    std::shared_ptr<TextureData> specular_neg_z_map =
        loadTextureHDR(skybox_specular_map.negative_z_map);

    std::array<std::shared_ptr<TextureData>, 6> irradiance_maps = {
        irradiace_pos_x_map,
        irradiace_neg_x_map,
        irradiace_pos_z_map,
        irradiace_neg_z_map,
        irradiace_pos_y_map,
        irradiace_neg_y_map
    };
    std::array<std::shared_ptr<TextureData>, 6> specular_maps = {
        specular_pos_x_map,
        specular_neg_x_map,
        specular_pos_z_map,
        specular_neg_z_map,
        specular_pos_y_map,
        specular_neg_y_map
    };

    createIBLSamplers();

    createTexture(
        m_ctx,
        brdf_map->width,
        brdf_map->height,
        brdf_map->pixels,
        brdf_map->format,
        0,
        global_render_resource.ibl_resource.brdfLUT_texture_image,
        global_render_resource.ibl_resource.brdfLUT_texture_image_view,
        global_render_resource.ibl_resource.brdfLUT_texture_image_allocation
    );

    createIBLTextures(irradiance_maps, specular_maps);

    m_global_uploaded = true;
}

void RenderResource::uploadEntity(
    const RenderEntity &entity, const MeshData &mesh, const PBRMaterialData &material
) {
    uploadMesh(entity, mesh);
    uploadPBRMaterial(entity, material);
}

void RenderResource::uploadMesh(const RenderEntity &entity, const MeshData &data) {
    size_t asset_id = entity.mesh_asset_id;

    if (!m_mesh_map.count(asset_id)) {
        return;
    }

    m_mesh_map[asset_id] = {};
    auto &mesh = m_mesh_map[asset_id];

    mesh.index_count = data.indices.size();
    mesh.vertex_count = data.vertices.size();

    uploadVertexBuffer(
        mesh, data.vertices.data(), mesh.vertex_count * sizeof(MeshVertex)
    );
    uploadIndexBuffer(mesh, data.indices.data(), mesh.index_count * sizeof(uint16_t));
}

void RenderResource::uploadPBRMaterial(
    const RenderEntity &entity, const PBRMaterialData &data
) {
    size_t asset_id = entity.material_asset_id;
    if (!m_material_map.count(asset_id)) {
        return;
    }

    m_material_map[asset_id] = {};
    auto &material = m_material_map[asset_id];

    float empty_image[] = {1.0f, 1.0f, 1.0f, 1.0f};

    void *base_color_image_pixels = empty_image;
    uint32_t base_color_image_width = 1;
    uint32_t base_color_image_height = 1;
    VkFormat base_color_image_format = VK_FORMAT_R8G8B8A8_SRGB;
    if (data.base_color_texture) {
        base_color_image_pixels = data.base_color_texture->pixels;
        base_color_image_width = data.base_color_texture->width;
        base_color_image_height = data.base_color_texture->height;
        base_color_image_format = data.base_color_texture->format;
    }

    void *normal_image_pixels = empty_image;
    uint32_t normal_image_width = 1;
    uint32_t normal_image_height = 1;
    VkFormat normal_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    if (data.normal_texture) {
        normal_image_pixels = data.normal_texture->pixels;
        normal_image_width = data.normal_texture->width;
        normal_image_height = data.normal_texture->height;
        normal_image_format = data.normal_texture->format;
    }

    void *metallic_image_pixels = empty_image;
    uint32_t metallic_image_width = 1;
    uint32_t metallic_image_height = 1;
    VkFormat metallic_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    if (data.metallic_texture) {
        metallic_image_pixels = data.metallic_texture->pixels;
        metallic_image_width = data.metallic_texture->width;
        metallic_image_height = data.metallic_texture->height;
        metallic_image_format = data.metallic_texture->format;
    }

    void *roughness_image_pixels = empty_image;
    uint32_t roughness_image_width = 1;
    uint32_t roughness_image_height = 1;
    VkFormat roughness_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    if (data.roughness_texture) {
        roughness_image_pixels = data.roughness_texture->pixels;
        roughness_image_width = data.roughness_texture->width;
        roughness_image_height = data.roughness_texture->height;
        roughness_image_format = data.roughness_texture->format;
    }

    void *occlusion_image_pixels = empty_image;
    uint32_t occlusion_image_width = 1;
    uint32_t occlusion_image_height = 1;
    VkFormat occlusion_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    if (data.occlusion_texture) {
        occlusion_image_pixels = data.occlusion_texture->pixels;
        occlusion_image_width = data.occlusion_texture->width;
        occlusion_image_height = data.occlusion_texture->height;
        occlusion_image_format = data.occlusion_texture->format;
    }

    void *emissive_image_pixels = empty_image;
    uint32_t emissive_image_width = 1;
    uint32_t emissive_image_height = 1;
    VkFormat emissive_image_format = VK_FORMAT_R8G8B8A8_UNORM;
    if (data.emissive_texture) {
        emissive_image_pixels = data.emissive_texture->pixels;
        emissive_image_width = data.emissive_texture->width;
        emissive_image_height = data.emissive_texture->height;
        emissive_image_format = data.emissive_texture->format;
    }

    uploadMaterialUniformBuffer(material, entity);

    createTexture(
        m_ctx,
        base_color_image_width,
        base_color_image_height,
        base_color_image_pixels,
        base_color_image_format,
        0,
        material.base_color_texture_image,
        material.base_color_image_view,
        material.base_color_image_allocation
    );

    createTexture(
        m_ctx,
        normal_image_width,
        normal_image_height,
        normal_image_pixels,
        normal_image_format,
        0,
        material.normal_texture_image,
        material.normal_image_view,
        material.normal_image_allocation
    );

    createTexture(
        m_ctx,
        metallic_image_width,
        metallic_image_height,
        metallic_image_pixels,
        metallic_image_format,
        0,
        material.metallic_texture_image,
        material.metallic_image_view,
        material.metallic_image_allocation
    );

    createTexture(
        m_ctx,
        roughness_image_width,
        roughness_image_height,
        roughness_image_pixels,
        roughness_image_format,
        0,
        material.roughness_texture_image,
        material.roughness_image_view,
        material.roughness_image_allocation
    );

    createTexture(
        m_ctx,
        occlusion_image_width,
        occlusion_image_height,
        occlusion_image_pixels,
        occlusion_image_format,
        0,
        material.occlusion_texture_image,
        material.occlusion_image_view,
        material.occlusion_image_allocation
    );

    createTexture(
        m_ctx,
        emissive_image_width,
        emissive_image_height,
        emissive_image_pixels,
        emissive_image_format,
        0,
        material.emissive_texture_image,
        material.emissive_image_view,
        material.emissive_image_allocation
    );

    VkDescriptorBufferInfo material_uniform_buffer_info = {};
    material_uniform_buffer_info.offset = 0;
    material_uniform_buffer_info.range = sizeof(MeshPerMaterialUniformBufferObject);
    material_uniform_buffer_info.buffer = material.material_uniform_buffer;

    VkDescriptorImageInfo base_color_image_info = {};
    base_color_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    base_color_image_info.imageView = material.base_color_image_view;
    base_color_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(base_color_image_width, base_color_image_height);

    VkDescriptorImageInfo normal_image_info{};
    normal_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normal_image_info.imageView = material.normal_image_view;
    normal_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(normal_image_width, normal_image_height);

    VkDescriptorImageInfo metallic_image_info{};
    metallic_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    metallic_image_info.imageView = material.metallic_image_view;
    metallic_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(metallic_image_width, metallic_image_height);

    VkDescriptorImageInfo roughness_image_info{};
    roughness_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    roughness_image_info.imageView = material.roughness_image_view;
    roughness_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(roughness_image_width, roughness_image_height);

    VkDescriptorImageInfo occlusion_image_info{};
    occlusion_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    occlusion_image_info.imageView = material.occlusion_image_view;
    occlusion_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(occlusion_image_width, occlusion_image_height);

    VkDescriptorImageInfo emissive_image_info{};
    emissive_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    emissive_image_info.imageView = material.emissive_image_view;
    emissive_image_info.sampler =
        m_ctx->getOrCreateMipmapSampler(emissive_image_width, emissive_image_height);

    VkWriteDescriptorSet material_descriptor_writes[7]{};

    material_descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    material_descriptor_writes[0].dstSet = material.material_descriptor_set;
    material_descriptor_writes[0].dstBinding = 0;
    material_descriptor_writes[0].dstArrayElement = 0;
    material_descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    material_descriptor_writes[0].descriptorCount = 1;
    material_descriptor_writes[0].pBufferInfo = &material_uniform_buffer_info;

    material_descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    material_descriptor_writes[1].dstSet = material.material_descriptor_set;
    material_descriptor_writes[1].dstBinding = 1;
    material_descriptor_writes[1].dstArrayElement = 0;
    material_descriptor_writes[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    material_descriptor_writes[1].descriptorCount = 1;
    material_descriptor_writes[1].pImageInfo = &base_color_image_info;

    material_descriptor_writes[2] = material_descriptor_writes[1];
    material_descriptor_writes[2].dstBinding = 2;
    material_descriptor_writes[2].pImageInfo = &normal_image_info;

    material_descriptor_writes[3] = material_descriptor_writes[1];
    material_descriptor_writes[3].dstBinding = 3;
    material_descriptor_writes[3].pImageInfo = &metallic_image_info;

    material_descriptor_writes[4] = material_descriptor_writes[1];
    material_descriptor_writes[4].dstBinding = 4;
    material_descriptor_writes[4].pImageInfo = &roughness_image_info;

    material_descriptor_writes[5] = material_descriptor_writes[1];
    material_descriptor_writes[5].dstBinding = 5;
    material_descriptor_writes[5].pImageInfo = &occlusion_image_info;

    material_descriptor_writes[6] = material_descriptor_writes[1];
    material_descriptor_writes[6].dstBinding = 6;
    material_descriptor_writes[6].pImageInfo = &emissive_image_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(material_descriptor_writes),
        material_descriptor_writes,
        0,
        nullptr
    );
}

void RenderResource::resetRingBufferOffset() {
    StorageBuffer &storage_buffer = global_render_resource.storage_buffer;
    storage_buffer.global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        storage_buffer.global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()];
}

const MeshResource *RenderResource::getEntityMesh(const RenderEntity &entity) const {
    auto it = m_mesh_map.find(entity.mesh_asset_id);
    if (it != m_mesh_map.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

const PBRMaterialResource *RenderResource::getEntityMaterial(const RenderEntity &entity
) const {
    auto it = m_material_map.find(entity.material_asset_id);
    if (it != m_material_map.end()) {
        return &it->second;
    } else {
        return nullptr;
    }
}

void RenderResource::clearMesh() {
    for (auto &[_, mesh] : m_mesh_map) {
        freeMeshResource(mesh);
    }
}

void RenderResource::clearMaterial() {
    for (auto &[_, material] : m_material_map) {
        freePBRMaterialResource(material);
    }
}

void RenderResource::createAndMapStorageBuffer() {
    uint32_t frames_in_flight = m_ctx->k_max_frames_in_flight;
    StorageBuffer &storage_buffer = global_render_resource.storage_buffer;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_ctx->physical_device, &properties);

    storage_buffer.min_uniform_buffer_offset_alignment =
        properties.limits.minUniformBufferOffsetAlignment;
    storage_buffer.min_storage_buffer_offset_alignment =
        properties.limits.minStorageBufferOffsetAlignment;
    storage_buffer.max_storage_buffer_range = properties.limits.maxStorageBufferRange;
    storage_buffer.non_coherent_atom_size = properties.limits.nonCoherentAtomSize;

    VkDeviceSize global_storage_buffer_size = 1024 * 1024 * 128;
    if (storage_buffer.global_upload_ringbuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_ctx->device, storage_buffer.global_upload_ringbuffer, nullptr);
        vkFreeMemory(
            m_ctx->device, storage_buffer.global_upload_ringbuffer_memory, nullptr
        );
    }
    createBuffer(
        m_ctx->physical_device,
        m_ctx->device,
        global_storage_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        storage_buffer.global_upload_ringbuffer,
        storage_buffer.global_upload_ringbuffer_memory
    );

    storage_buffer.global_upload_ringbuffers_begin.resize(frames_in_flight);
    storage_buffer.global_upload_ringbuffers_end.resize(frames_in_flight);
    storage_buffer.global_upload_ringbuffers_size.resize(frames_in_flight);

    for (uint32_t i = 0; i < frames_in_flight; ++i) {
        storage_buffer.global_upload_ringbuffers_begin[i] =
            (global_storage_buffer_size * i) / frames_in_flight;
        storage_buffer.global_upload_ringbuffers_end[i] =
            storage_buffer.global_upload_ringbuffers_begin[i];
        storage_buffer.global_upload_ringbuffers_size[i] =
            (global_storage_buffer_size * (i + 1)) / frames_in_flight -
            (global_storage_buffer_size * i) / frames_in_flight;
    }

    vkMapMemory(
        m_ctx->device,
        storage_buffer.global_upload_ringbuffer_memory,
        0,
        VK_WHOLE_SIZE,
        0,
        &storage_buffer.global_upload_ringbuffer_memory_pointer
    );
}

void RenderResource::createIBLSamplers() {
    VkPhysicalDeviceProperties properties;

    vkGetPhysicalDeviceProperties(m_ctx->physical_device, &properties);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.maxLod = 0.0f;

    VkSampler &brdf_sampler = global_render_resource.ibl_resource.brdfLUT_texture_sampler;
    VkResult res = vkCreateSampler(m_ctx->device, &sampler_info, nullptr, &brdf_sampler);
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create brdf sampler");
    }

    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 8.0f;
    sampler_info.mipLodBias = 0.0f;
    VkSampler &irradiance_sampler =
        global_render_resource.ibl_resource.irradiance_texture_sampler;
    res = vkCreateSampler(m_ctx->device, &sampler_info, nullptr, &irradiance_sampler);
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create irradiance sampler");
    }

    VkSampler &specular_sampler =
        global_render_resource.ibl_resource.specular_texture_sampler;
    res = vkCreateSampler(m_ctx->device, &sampler_info, nullptr, &specular_sampler);
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create specular sampler");
    }
}

void RenderResource::createIBLTextures(
    std::array<std::shared_ptr<TextureData>, 6> irradiance_maps,
    std::array<std::shared_ptr<TextureData>, 6> specular_maps
) {
    uint32_t dimension = std::max(irradiance_maps[0]->width, irradiance_maps[0]->height);
    uint32_t mip_levels = static_cast<uint32_t>(std::floor(log2(dimension))) + 1;
    createCubeMap(
        m_ctx,
        irradiance_maps[0]->width,
        irradiance_maps[0]->height,
        {irradiance_maps[0]->pixels,
         irradiance_maps[1]->pixels,
         irradiance_maps[2]->pixels,
         irradiance_maps[3]->pixels,
         irradiance_maps[4]->pixels,
         irradiance_maps[5]->pixels},
        irradiance_maps[0]->format,
        mip_levels,
        global_render_resource.ibl_resource.irradiance_texture_image,
        global_render_resource.ibl_resource.irradiance_texture_image_view,
        global_render_resource.ibl_resource.irradiance_texture_image_allocation
    );

    dimension = std::max(specular_maps[0]->width, specular_maps[0]->height);
    mip_levels = static_cast<uint32_t>(std::floor(log2(dimension))) + 1;
    createCubeMap(
        m_ctx,
        specular_maps[0]->width,
        specular_maps[0]->height,
        {specular_maps[0]->pixels,
         specular_maps[1]->pixels,
         specular_maps[2]->pixels,
         specular_maps[3]->pixels,
         specular_maps[4]->pixels,
         specular_maps[5]->pixels},
        specular_maps[0]->format,
        mip_levels,
        global_render_resource.ibl_resource.specular_texture_image,
        global_render_resource.ibl_resource.specular_texture_image_view,
        global_render_resource.ibl_resource.specular_texture_image_allocation
    );
}

void RenderResource::freeIBLResource() {
    // destroy texture
    vkDestroyImageView(
        m_ctx->device,
        global_render_resource.ibl_resource.brdfLUT_texture_image_view,
        nullptr
    );
    vmaDestroyImage(
        m_ctx->assets_allocator,
        global_render_resource.ibl_resource.brdfLUT_texture_image,
        global_render_resource.ibl_resource.brdfLUT_texture_image_allocation
    );
    vkDestroyImageView(
        m_ctx->device,
        global_render_resource.ibl_resource.irradiance_texture_image_view,
        nullptr
    );
    vmaDestroyImage(
        m_ctx->assets_allocator,
        global_render_resource.ibl_resource.irradiance_texture_image,
        global_render_resource.ibl_resource.irradiance_texture_image_allocation
    );
    vkDestroyImageView(
        m_ctx->device,
        global_render_resource.ibl_resource.specular_texture_image_view,
        nullptr
    );
    vmaDestroyImage(
        m_ctx->assets_allocator,
        global_render_resource.ibl_resource.specular_texture_image,
        global_render_resource.ibl_resource.specular_texture_image_allocation
    );

    // destroy sampler
    vkDestroySampler(
        m_ctx->device,
        global_render_resource.ibl_resource.brdfLUT_texture_sampler,
        nullptr
    );
    vkDestroySampler(
        m_ctx->device,
        global_render_resource.ibl_resource.irradiance_texture_sampler,
        nullptr
    );
    vkDestroySampler(
        m_ctx->device,
        global_render_resource.ibl_resource.specular_texture_sampler,
        nullptr
    );
}

void RenderResource::uploadVertexBuffer(
    MeshResource &mesh, const void *vertex_data, size_t vertex_buffer_size
) {
    VkBuffer inefficient_staging_buffer;
    VkDeviceMemory inefficient_staging_buffer_memory;
    createBuffer(
        m_ctx->physical_device,
        m_ctx->device,
        vertex_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        inefficient_staging_buffer,
        inefficient_staging_buffer_memory
    );

    void *staging_buffer_data;
    vkMapMemory(
        m_ctx->device,
        inefficient_staging_buffer_memory,
        0,
        vertex_buffer_size,
        0,
        &staging_buffer_data
    );
    memcpy(staging_buffer_data, vertex_data, vertex_buffer_size);
    vkUnmapMemory(m_ctx->device, inefficient_staging_buffer_memory);

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size;
    buffer_info.usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBuffer(
        m_ctx->assets_allocator,
        &buffer_info,
        &alloc_info,
        &mesh.vertex_buffer,
        &mesh.vertex_buffer_allocation,
        nullptr
    );

    copyBuffer(
        m_ctx, inefficient_staging_buffer, mesh.vertex_buffer, 0, 0, vertex_buffer_size
    );

    vkDestroyBuffer(m_ctx->device, inefficient_staging_buffer, nullptr);
    vkFreeMemory(m_ctx->device, inefficient_staging_buffer_memory, nullptr);
}

void RenderResource::uploadIndexBuffer(
    MeshResource &mesh, const void *index_data, size_t index_buffer_size
) {
    VkBuffer inefficient_staging_buffer;
    VkDeviceMemory inefficient_staging_buffer_memory;
    createBuffer(
        m_ctx->physical_device,
        m_ctx->device,
        index_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        inefficient_staging_buffer,
        inefficient_staging_buffer_memory
    );

    void *staging_buffer_data;
    vkMapMemory(
        m_ctx->device,
        inefficient_staging_buffer_memory,
        0,
        index_buffer_size,
        0,
        &staging_buffer_data
    );
    memcpy(staging_buffer_data, index_data, index_buffer_size);
    vkUnmapMemory(m_ctx->device, inefficient_staging_buffer_memory);

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = index_buffer_size;
    buffer_info.usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBuffer(
        m_ctx->assets_allocator,
        &buffer_info,
        &alloc_info,
        &mesh.index_buffer,
        &mesh.index_buffer_allocation,
        nullptr
    );

    copyBuffer(
        m_ctx, inefficient_staging_buffer, mesh.index_buffer, 0, 0, index_buffer_size
    );

    vkDestroyBuffer(m_ctx->device, inefficient_staging_buffer, nullptr);
    vkFreeMemory(m_ctx->device, inefficient_staging_buffer_memory, nullptr);
}

void RenderResource::freeMeshResource(const MeshResource &mesh) {
    vmaDestroyBuffer(
        m_ctx->assets_allocator, mesh.vertex_buffer, mesh.vertex_buffer_allocation
    );
    vmaDestroyBuffer(
        m_ctx->assets_allocator, mesh.index_buffer, mesh.index_buffer_allocation
    );
}

void RenderResource::uploadMaterialUniformBuffer(
    PBRMaterialResource &material, const RenderEntity &entity
) {
    MeshPerMaterialUniformBufferObject material_uniform_data{};

    material_uniform_data.base_color_factor = entity.base_color_factor;
    material_uniform_data.metallic_factor = entity.metallic_factor;
    material_uniform_data.roughness_factor = entity.roughness_factor;
    material_uniform_data.normal_scale = entity.normal_scale;
    material_uniform_data.occlusion_strength = entity.occlusion_strength;
    material_uniform_data.emissive_factor = entity.emissive_factor;
    material_uniform_data.is_blend = entity.blend;
    material_uniform_data.is_double_sided = entity.double_sided;

    size_t buffer_size = sizeof(MeshPerMaterialUniformBufferObject);

    VkBuffer inefficient_staging_buffer;
    VkDeviceMemory inefficient_staging_buffer_memory;
    createBuffer(
        m_ctx->physical_device,
        m_ctx->device,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        inefficient_staging_buffer,
        inefficient_staging_buffer_memory
    );

    void *staging_buffer_data;
    vkMapMemory(
        m_ctx->device,
        inefficient_staging_buffer_memory,
        0,
        buffer_size,
        0,
        &staging_buffer_data
    );
    memcpy(staging_buffer_data, &material_uniform_data, buffer_size);
    vkUnmapMemory(m_ctx->device, inefficient_staging_buffer_memory);

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBufferWithAlignment(
        m_ctx->assets_allocator,
        &buffer_info,
        &alloc_info,
        global_render_resource.storage_buffer.min_uniform_buffer_offset_alignment,
        &material.material_uniform_buffer,
        &material.material_uniform_buffer_allocation,
        nullptr
    );

    copyBuffer(
        m_ctx,
        inefficient_staging_buffer,
        material.material_uniform_buffer,
        0,
        0,
        buffer_size
    );

    vkDestroyBuffer(m_ctx->device, inefficient_staging_buffer, nullptr);
    vkFreeMemory(m_ctx->device, inefficient_staging_buffer_memory, nullptr);
}

void RenderResource::freePBRMaterialResource(const PBRMaterialResource &material) {
    freeTextureResource(
        material.base_color_texture_image,
        material.base_color_image_view,
        material.base_color_image_allocation
    );

    freeTextureResource(
        material.normal_texture_image,
        material.normal_image_view,
        material.normal_image_allocation
    );

    freeTextureResource(
        material.metallic_texture_image,
        material.metallic_image_view,
        material.metallic_image_allocation
    );

    freeTextureResource(
        material.roughness_texture_image,
        material.roughness_image_view,
        material.roughness_image_allocation
    );

    freeTextureResource(
        material.occlusion_texture_image,
        material.occlusion_image_view,
        material.occlusion_image_allocation
    );

    freeTextureResource(
        material.emissive_texture_image,
        material.emissive_image_view,
        material.emissive_image_allocation
    );

    vmaDestroyBuffer(
        m_ctx->assets_allocator,
        material.material_uniform_buffer,
        material.material_uniform_buffer_allocation
    );
}

void RenderResource::freeTextureResource(
    VkImage image, VkImageView view, VmaAllocation allocation
) {
    vkDestroyImageView(m_ctx->device, view, nullptr);
    vmaDestroyImage(m_ctx->assets_allocator, image, allocation);
}

}  // namespace Vain
