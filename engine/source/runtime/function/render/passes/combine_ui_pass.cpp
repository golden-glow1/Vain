#include "combine_ui_pass.h"

#include "core/base/macro.h"
#include "core/vulkan/vulkan_utils.h"

namespace Vain {

static std::vector<uint8_t> s_post_process_vert = {
#include "post_process.vert.spv.h"
};

static std::vector<uint8_t> s_combine_ui_frag = {
#include "combine_ui.frag.spv.h"
};

CombineUIPass::~CombineUIPass() { clear(); }

void CombineUIPass::initialize(RenderPassInitInfo *init_info) {
    RenderPass::initialize(init_info);

    CombineUIPassInitInfo *_init_info = static_cast<CombineUIPassInitInfo *>(init_info);
    render_pass = _init_info->render_pass;

    createDecriptorSetLayouts();
    createPipelines();
    allocateDecriptorSets();
    onResize(_init_info->scene_input_attachment, _init_info->ui_input_attachment);
}

void CombineUIPass::clear() {
    vkDestroyPipeline(m_ctx->device, pipelines[0], nullptr);
    vkDestroyPipelineLayout(m_ctx->device, pipeline_layouts[0], nullptr);
    vkDestroyDescriptorSetLayout(m_ctx->device, descriptor_set_layouts[0], nullptr);
}

void CombineUIPass::draw() {
    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Combine UI", color);

    VkViewport viewport = {
        0.0,
        0.0,
        static_cast<float>(m_ctx->swapchain_extent.width),
        static_cast<float>(m_ctx->swapchain_extent.height),
        0.0,
        1.0
    };
    VkRect2D scissor = {
        0, 0, m_ctx->swapchain_extent.width, m_ctx->swapchain_extent.height
    };

    m_ctx->cmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[0]);
    m_ctx->cmdSetViewport(command_buffer, 0, 1, &viewport);
    m_ctx->cmdSetScissor(command_buffer, 0, 1, &scissor);
    m_ctx->cmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layouts[0],
        0,
        1,
        &descriptor_sets[0],
        0,
        nullptr
    );

    m_ctx->cmdDraw(command_buffer, 3, 1, 0, 0);
    m_ctx->popEvent(command_buffer);
}

void CombineUIPass::onResize(
    VkImageView scene_input_attachment, VkImageView ui_input_attachment
) {
    VkDescriptorImageInfo per_frame_scene_input_attachment_info{};
    per_frame_scene_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    per_frame_scene_input_attachment_info.imageView = scene_input_attachment;
    per_frame_scene_input_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo per_frame_ui_input_attachment_info{};
    per_frame_ui_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    per_frame_ui_input_attachment_info.imageView = ui_input_attachment;
    per_frame_ui_input_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet post_process_descriptor_writes[2]{};

    post_process_descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    post_process_descriptor_writes[0].dstSet = descriptor_sets[0];
    post_process_descriptor_writes[0].dstBinding = 0;
    post_process_descriptor_writes[0].dstArrayElement = 0;
    post_process_descriptor_writes[0].descriptorType =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    post_process_descriptor_writes[0].descriptorCount = 1;
    post_process_descriptor_writes[0].pImageInfo = &per_frame_scene_input_attachment_info;

    post_process_descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    post_process_descriptor_writes[1].dstSet = descriptor_sets[0];
    post_process_descriptor_writes[1].dstBinding = 1;
    post_process_descriptor_writes[1].dstArrayElement = 0;
    post_process_descriptor_writes[1].descriptorType =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    post_process_descriptor_writes[1].descriptorCount = 1;
    post_process_descriptor_writes[1].pImageInfo = &per_frame_ui_input_attachment_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(post_process_descriptor_writes),
        post_process_descriptor_writes,
        0,
        nullptr
    );
}

void CombineUIPass::createDecriptorSetLayouts() {
    descriptor_set_layouts.resize(1);

    VkDescriptorSetLayoutBinding post_process_global_layout_bindings[2]{};

    // scene
    post_process_global_layout_bindings[0].binding = 0;
    post_process_global_layout_bindings[0].descriptorType =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    post_process_global_layout_bindings[0].descriptorCount = 1;
    post_process_global_layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // ui
    post_process_global_layout_bindings[1].binding = 1;
    post_process_global_layout_bindings[1].descriptorType =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    post_process_global_layout_bindings[1].descriptorCount = 1;
    post_process_global_layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo post_process_global_layout_create_info{};
    post_process_global_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    post_process_global_layout_create_info.bindingCount =
        ARRAY_SIZE(post_process_global_layout_bindings);
    post_process_global_layout_create_info.pBindings =
        post_process_global_layout_bindings;

    VkResult res = vkCreateDescriptorSetLayout(
        m_ctx->device,
        &post_process_global_layout_create_info,
        nullptr,
        &descriptor_set_layouts[0]
    );

    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create descriptor set layout");
    }
}

void CombineUIPass::createPipelines() {
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
        createShaderModule(m_ctx->device, s_post_process_vert);
    VkShaderModule frag_shader_module =
        createShaderModule(m_ctx->device, s_combine_ui_frag);

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

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
    vertex_input_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
    vertex_input_state_create_info.pVertexBindingDescriptions = nullptr;
    vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;
    vertex_input_state_create_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
    input_assembly_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state_create_info{};
    viewport_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
    rasterization_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.lineWidth = 1.0f;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = 2;
    dynamic_state_create_info.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
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
    pipeline_info.subpass = _subpass_combine_ui;
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

void CombineUIPass::allocateDecriptorSets() {
    descriptor_sets.resize(1);

    VkDescriptorSetAllocateInfo post_process_global_descriptor_set_alloc_info{};
    post_process_global_descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    post_process_global_descriptor_set_alloc_info.descriptorPool = m_ctx->descriptor_pool;
    post_process_global_descriptor_set_alloc_info.descriptorSetCount = 1;
    post_process_global_descriptor_set_alloc_info.pSetLayouts =
        &descriptor_set_layouts[0];

    VkResult res = vkAllocateDescriptorSets(
        m_ctx->device, &post_process_global_descriptor_set_alloc_info, &descriptor_sets[0]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate descriptor set");
    }
}

}  // namespace Vain