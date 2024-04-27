#include "directional_light_pass.h"

#include <assert.h>

#include "core/base/macro.h"
#include "core/vulkan/vulkan_utils.h"
#include "function/render/render_data.h"
#include "function/render/render_scene.h"

namespace Vain {

static std::vector<uint8_t> s_directional_light_shadow_vert = {
#include "mesh_directional_light_shadow.vert.spv.h"
};

static std::vector<uint8_t> s_directional_light_shadow_frag = {
#include "mesh_directional_light_shadow.frag.spv.h"
};

DirectionalLightPass::~DirectionalLightPass() { clear(); }

void DirectionalLightPass::initialize(RenderPassInitInfo *init_info) {
    RenderPass::initialize(init_info);

    createAttachments();
    createRenderPass();
    createDescriptorSetLayouts();
    createPipelines();
    allocateDescriptorSets();
    createFramebuffer();
}

void DirectionalLightPass::clear() {
    vkDestroyFramebuffer(m_ctx->device, framebuffer, nullptr);

    vkDestroyPipeline(m_ctx->device, pipelines[0], nullptr);

    vkDestroyPipelineLayout(m_ctx->device, pipeline_layouts[0], nullptr);

    vkDestroyDescriptorSetLayout(m_ctx->device, descriptor_set_layouts[0], nullptr);

    vkDestroyRenderPass(m_ctx->device, render_pass, nullptr);

    for (auto &attachment : framebuffer_info.attachments) {
        vkDestroyImage(m_ctx->device, attachment.image, nullptr);
        vkDestroyImageView(m_ctx->device, attachment.view, nullptr);
        vkFreeMemory(m_ctx->device, attachment.memory, nullptr);
    }
}

void DirectionalLightPass::draw(const RenderScene &scene) {
    using MeshBatch = std::unordered_map<const MeshResource *, std::vector<glm::mat4>>;

    std::unordered_map<const PBRMaterialResource *, MeshBatch>
        directional_light_mesh_drawcall_batch;

    for (const auto &node : scene.directional_light_visible_mesh_nodes) {
        auto &mesh_batch = directional_light_mesh_drawcall_batch[node.ref_material];
        auto &batch_nodes = mesh_batch[node.ref_mesh];

        batch_nodes.push_back(node.model_matrix);
    }

    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    {
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = framebuffer;
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = {
            k_directional_light_shadow_map_dimension,
            k_directional_light_shadow_map_dimension
        };

        VkClearValue clear_values[2];
        clear_values[0].color = {1.0f};
        clear_values[1].depthStencil = {1.0f, 0};
        render_pass_begin_info.clearValueCount = ARRAY_SIZE(clear_values);
        render_pass_begin_info.pClearValues = clear_values;

        m_ctx->cmdBeginRenderPass(
            command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE
        );

        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_ctx->pushEvent(command_buffer, "Directional Light Shadow", color);
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Mesh", color);

    m_ctx->cmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[0]);

    uint32_t per_frame_dynamic_offset = ROUND_UP(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
        m_res->global_render_resource.storage_buffer.min_storage_buffer_offset_alignment
    );
    m_res->global_render_resource.storage_buffer
        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        per_frame_dynamic_offset +
        sizeof(DirectionalLightShadowPerFrameStorageBufferObject);
    assert(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
        m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
            m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
    );

    DirectionalLightShadowPerFrameStorageBufferObject *per_frame_storage_buffer_object =
        reinterpret_cast<DirectionalLightShadowPerFrameStorageBufferObject *>(
            reinterpret_cast<uintptr_t>(m_res->global_render_resource.storage_buffer
                                            .global_upload_ringbuffer_memory_pointer) +
            per_frame_dynamic_offset
        );
    *per_frame_storage_buffer_object =
        m_res->directional_light_shadow_per_frame_storage_buffer_object;

    for (auto &[material, mesh_batch] : directional_light_mesh_drawcall_batch) {
        for (auto &[mesh, batch_nodes] : mesh_batch) {
            uint32_t total_instance_count = batch_nodes.size();
            if (total_instance_count == 0) {
                continue;
            }

            VkDeviceSize offset = 0;
            m_ctx->cmdBindVertexBuffers(
                command_buffer, 0, 1, &mesh->vertex_buffer, &offset
            );
            m_ctx->cmdBindIndexBuffer(
                command_buffer, mesh->index_buffer, 0, VK_INDEX_TYPE_UINT32
            );

            uint32_t per_drawcall_max_instance = k_mesh_per_drawcall_max_instance_count;
            uint32_t drawcall_count =
                ROUND_UP(total_instance_count, per_drawcall_max_instance) /
                per_drawcall_max_instance;
            for (uint32_t drawcall_index = 0; drawcall_index < drawcall_count;
                 ++drawcall_index) {
                uint32_t current_instance_count = per_drawcall_max_instance;
                if (total_instance_count - per_drawcall_max_instance * drawcall_index <
                    per_drawcall_max_instance) {
                    // rest
                    current_instance_count =
                        total_instance_count - per_drawcall_max_instance * drawcall_index;
                }

                uint32_t per_drawcall_dynamic_offset = ROUND_UP(
                    m_res->global_render_resource.storage_buffer
                        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
                    m_res->global_render_resource.storage_buffer
                        .min_storage_buffer_offset_alignment
                );
                m_res->global_render_resource.storage_buffer
                    .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
                    per_drawcall_dynamic_offset +
                    sizeof(MeshPerDrawcallStorageBufferObject);
                assert(
                    m_res->global_render_resource.storage_buffer
                        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
                    m_res->global_render_resource.storage_buffer
                            .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
                        m_res->global_render_resource.storage_buffer
                            .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
                );

                MeshPerDrawcallStorageBufferObject *per_drawcall_storage_buffer_object =
                    reinterpret_cast<MeshPerDrawcallStorageBufferObject *>(
                        reinterpret_cast<uintptr_t>(
                            m_res->global_render_resource.storage_buffer
                                .global_upload_ringbuffer_memory_pointer
                        ) +
                        per_drawcall_dynamic_offset
                    );
                for (uint32_t i = 0; i < current_instance_count; ++i) {
                    per_drawcall_storage_buffer_object->mesh_instances[i].model_matrix =
                        batch_nodes[per_drawcall_max_instance * drawcall_index + i];
                }

                uint32_t dynamic_offsets[2] = {
                    per_frame_dynamic_offset, per_drawcall_dynamic_offset
                };

                m_ctx->cmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_layouts[0],
                    0,
                    1,
                    &descriptor_sets[0],
                    2,
                    dynamic_offsets
                );

                m_ctx->cmdDrawIndexed(
                    command_buffer, mesh->index_count, current_instance_count, 0, 0, 0
                );
            }
        }
    }

    m_ctx->popEvent(command_buffer);

    {
        m_ctx->popEvent(command_buffer);
        m_ctx->cmdEndRenderPass(command_buffer);
    }
}

void DirectionalLightPass::createAttachments() {
    framebuffer_info.attachments.resize(2);

    // color
    framebuffer_info.attachments[0].format = VK_FORMAT_R32_SFLOAT;
    createImage(
        m_ctx->physical_device,
        m_ctx->device,
        k_directional_light_shadow_map_dimension,
        k_directional_light_shadow_map_dimension,
        framebuffer_info.attachments[0].format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        1,
        1,
        framebuffer_info.attachments[0].image,
        framebuffer_info.attachments[0].memory
    );
    framebuffer_info.attachments[0].view = createImageView(
        m_ctx->device,
        framebuffer_info.attachments[0].image,
        framebuffer_info.attachments[0].format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        1
    );

    // depth
    framebuffer_info.attachments[1].format = m_ctx->depth_image_format;
    createImage(
        m_ctx->physical_device,
        m_ctx->device,
        k_directional_light_shadow_map_dimension,
        k_directional_light_shadow_map_dimension,
        framebuffer_info.attachments[1].format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        1,
        1,
        framebuffer_info.attachments[1].image,
        framebuffer_info.attachments[1].memory
    );
    framebuffer_info.attachments[1].view = createImageView(
        m_ctx->device,
        framebuffer_info.attachments[1].image,
        framebuffer_info.attachments[1].format,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        1
    );
}

void DirectionalLightPass::createRenderPass() {
    VkAttachmentDescription attachments[2] = {};

    // color
    attachments[0].format = framebuffer_info.attachments[0].format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // depth
    attachments[1].format = framebuffer_info.attachments[1].format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};

    VkAttachmentReference shadow_pass_color_attachment_reference{};
    shadow_pass_color_attachment_reference.attachment = 0;
    shadow_pass_color_attachment_reference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference shadow_pass_depth_attachment_reference{};
    shadow_pass_depth_attachment_reference.attachment = 1;
    shadow_pass_depth_attachment_reference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &shadow_pass_color_attachment_reference;
    subpass.pDepthStencilAttachment = &shadow_pass_depth_attachment_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = 0;
    dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = 0;
    dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = ARRAY_SIZE(attachments);
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &dependency;

    VkResult res = vkCreateRenderPass(
        m_ctx->device, &render_pass_create_info, nullptr, &render_pass
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create render pass");
    }
}

void DirectionalLightPass::createDescriptorSetLayouts() {
    descriptor_set_layouts.resize(1);

    VkDescriptorSetLayoutBinding directional_light_shadow_global_layout_bindings[2]{};
    directional_light_shadow_global_layout_bindings[0].binding = 0;
    directional_light_shadow_global_layout_bindings[0].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    directional_light_shadow_global_layout_bindings[0].descriptorCount = 1;
    directional_light_shadow_global_layout_bindings[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT;

    directional_light_shadow_global_layout_bindings[1].binding = 1;
    directional_light_shadow_global_layout_bindings[1].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    directional_light_shadow_global_layout_bindings[1].descriptorCount = 1;
    directional_light_shadow_global_layout_bindings[1].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo directional_light_shadow_global_layout_create_info{};
    directional_light_shadow_global_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    directional_light_shadow_global_layout_create_info.flags = 0;
    directional_light_shadow_global_layout_create_info.bindingCount =
        ARRAY_SIZE(directional_light_shadow_global_layout_bindings);
    directional_light_shadow_global_layout_create_info.pBindings =
        directional_light_shadow_global_layout_bindings;

    VkResult res = vkCreateDescriptorSetLayout(
        m_ctx->device,
        &directional_light_shadow_global_layout_create_info,
        nullptr,
        &descriptor_set_layouts[0]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create descriptor set layout");
    }
}

void DirectionalLightPass::createPipelines() {
    pipelines.resize(1);
    pipeline_layouts.resize(1);

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &descriptor_set_layouts[0];
    VkResult res = vkCreatePipelineLayout(
        m_ctx->device, &pipeline_layout_create_info, nullptr, &pipeline_layouts[0]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create pipeline layout");
    }

    VkShaderModule vert_shader_module =
        createShaderModule(m_ctx->device, s_directional_light_shadow_vert);
    VkShaderModule frag_shader_module =
        createShaderModule(m_ctx->device, s_directional_light_shadow_frag);

    VkPipelineShaderStageCreateInfo vert_pipeline_shader_stage_create_info{};
    vert_pipeline_shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_pipeline_shader_stage_create_info.module = vert_shader_module;
    vert_pipeline_shader_stage_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_pipeline_shader_stage_create_info{};
    frag_pipeline_shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_pipeline_shader_stage_create_info.module = frag_shader_module;
    frag_pipeline_shader_stage_create_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_pipeline_shader_stage_create_info, frag_pipeline_shader_stage_create_info
    };

    auto vertex_binding_descriptions = MeshVertex::getBindingDescriptions();
    auto vertex_attribute_descriptions = MeshVertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
    vertex_input_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    vertex_input_state_create_info.pVertexBindingDescriptions =
        &vertex_binding_descriptions[0];
    vertex_input_state_create_info.vertexAttributeDescriptionCount = 1;
    vertex_input_state_create_info.pVertexAttributeDescriptions =
        &vertex_attribute_descriptions[0];

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
    input_assembly_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {
        0,
        0,
        k_directional_light_shadow_map_dimension,
        k_directional_light_shadow_map_dimension,
        0.0,
        1.0
    };
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {
        k_directional_light_shadow_map_dimension, k_directional_light_shadow_map_dimension
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info{};
    viewport_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.pViewports = &viewport;
    viewport_state_create_info.scissorCount = 1;
    viewport_state_create_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
    rasterization_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.lineWidth = 1.0f;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable = VK_FALSE;
    rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
    rasterization_state_create_info.depthBiasClamp = 0.0f;
    rasterization_state_create_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info{};
    multisample_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.sampleShadingEnable = VK_FALSE;
    multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
    color_blend_attachment_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
    color_blend_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.logicOpEnable = VK_FALSE;
    color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
    color_blend_state_create_info.blendConstants[0] = 0.0f;
    color_blend_state_create_info.blendConstants[1] = 0.0f;
    color_blend_state_create_info.blendConstants[2] = 0.0f;
    color_blend_state_create_info.blendConstants[3] = 0.0f;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info{};
    depth_stencil_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_create_info.stencilTestEnable = VK_FALSE;

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = ARRAY_SIZE(shader_stages);
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_state_create_info;
    pipeline_info.pInputAssemblyState = &input_assembly_create_info;
    pipeline_info.pViewportState = &viewport_state_create_info;
    pipeline_info.pRasterizationState = &rasterization_state_create_info;
    pipeline_info.pMultisampleState = &multisample_state_create_info;
    pipeline_info.pColorBlendState = &color_blend_state_create_info;
    pipeline_info.pDepthStencilState = &depth_stencil_create_info;
    pipeline_info.layout = pipeline_layouts[0];
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.pDynamicState = &dynamic_state_create_info;

    res = vkCreateGraphicsPipelines(
        m_ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipelines[0]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create pipeline");
    }

    vkDestroyShaderModule(m_ctx->device, vert_shader_module, nullptr);
    vkDestroyShaderModule(m_ctx->device, frag_shader_module, nullptr);
}

void DirectionalLightPass::allocateDescriptorSets() {
    descriptor_sets.resize(1);

    VkDescriptorSetAllocateInfo directional_light_shadow_global_descriptor_set_alloc_info;
    directional_light_shadow_global_descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    directional_light_shadow_global_descriptor_set_alloc_info.pNext = NULL;
    directional_light_shadow_global_descriptor_set_alloc_info.descriptorPool =
        m_ctx->descriptor_pool;
    directional_light_shadow_global_descriptor_set_alloc_info.descriptorSetCount = 1;
    directional_light_shadow_global_descriptor_set_alloc_info.pSetLayouts =
        &descriptor_set_layouts[0];

    VkResult res = vkAllocateDescriptorSets(
        m_ctx->device,
        &directional_light_shadow_global_descriptor_set_alloc_info,
        &descriptor_sets[0]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate descriptor set");
    }

    VkDescriptorBufferInfo mesh_directional_light_shadow_per_frame_storage_buffer_info{};
    mesh_directional_light_shadow_per_frame_storage_buffer_info.offset = 0;
    mesh_directional_light_shadow_per_frame_storage_buffer_info.range =
        sizeof(DirectionalLightShadowPerFrameStorageBufferObject);
    mesh_directional_light_shadow_per_frame_storage_buffer_info.buffer =
        m_res->global_render_resource.storage_buffer.global_upload_ringbuffer;
    assert(
        mesh_directional_light_shadow_per_frame_storage_buffer_info.range <
        m_res->global_render_resource.storage_buffer.max_storage_buffer_range
    );

    VkDescriptorBufferInfo mesh_directional_light_shadow_per_drawcall_storage_buffer_info{
    };
    mesh_directional_light_shadow_per_drawcall_storage_buffer_info.offset = 0;
    mesh_directional_light_shadow_per_drawcall_storage_buffer_info.range =
        sizeof(MeshPerDrawcallStorageBufferObject);
    mesh_directional_light_shadow_per_drawcall_storage_buffer_info.buffer =
        m_res->global_render_resource.storage_buffer.global_upload_ringbuffer;
    assert(
        mesh_directional_light_shadow_per_drawcall_storage_buffer_info.range <
        m_res->global_render_resource.storage_buffer.max_storage_buffer_range
    );

    VkWriteDescriptorSet directional_light_shadow_per_frame_storage_buffer_writes[2]{};
    directional_light_shadow_per_frame_storage_buffer_writes[0].sType =
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    directional_light_shadow_per_frame_storage_buffer_writes[0].dstSet =
        descriptor_sets[0];
    directional_light_shadow_per_frame_storage_buffer_writes[0].dstBinding = 0;
    directional_light_shadow_per_frame_storage_buffer_writes[0].dstArrayElement = 0;
    directional_light_shadow_per_frame_storage_buffer_writes[0].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    directional_light_shadow_per_frame_storage_buffer_writes[0].descriptorCount = 1;
    directional_light_shadow_per_frame_storage_buffer_writes[0].pBufferInfo =
        &mesh_directional_light_shadow_per_frame_storage_buffer_info;

    directional_light_shadow_per_frame_storage_buffer_writes[1] =
        directional_light_shadow_per_frame_storage_buffer_writes[0];
    directional_light_shadow_per_frame_storage_buffer_writes[1].dstBinding = 1;
    directional_light_shadow_per_frame_storage_buffer_writes[1].pBufferInfo =
        &mesh_directional_light_shadow_per_drawcall_storage_buffer_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(directional_light_shadow_per_frame_storage_buffer_writes),
        directional_light_shadow_per_frame_storage_buffer_writes,
        0,
        nullptr
    );
}

void DirectionalLightPass::createFramebuffer() {
    VkImageView attachments[2] = {
        framebuffer_info.attachments[0].view, framebuffer_info.attachments[1].view
    };

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.flags = 0;
    framebuffer_create_info.renderPass = render_pass;
    framebuffer_create_info.attachmentCount = ARRAY_SIZE(attachments);
    framebuffer_create_info.pAttachments = attachments;
    framebuffer_create_info.width = k_directional_light_shadow_map_dimension;
    framebuffer_create_info.height = k_directional_light_shadow_map_dimension;
    framebuffer_create_info.layers = 1;

    VkResult res = vkCreateFramebuffer(
        m_ctx->device, &framebuffer_create_info, nullptr, &framebuffer
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create framebuffer");
    }
}

}  // namespace Vain