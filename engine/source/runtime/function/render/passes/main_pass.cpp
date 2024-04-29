#include "main_pass.h"

#include <assert.h>

#include "core/base/macro.h"
#include "core/vulkan/vulkan_utils.h"
#include "function/render/render_resource.h"
#include "function/render/render_scene.h"

namespace Vain {

static std::vector<uint8_t> s_mesh_vert = {
#include "mesh.vert.spv.h"
};

static std::vector<uint8_t> s_mesh_frag = {
#include "mesh.frag.spv.h"
};

static std::vector<uint8_t> s_mesh_gbuffer_frag = {
#include "mesh_gbuffer.frag.spv.h"
};

static std::vector<uint8_t> s_deferred_lighting_vert = {
#include "deferred_light.vert.spv.h"
};

static std::vector<uint8_t> s_deferred_lighting_frag = {
#include "deferred_light.frag.spv.h"
};

static std::vector<uint8_t> s_skybox_vert = {
#include "skybox.vert.spv.h"
};

static std::vector<uint8_t> s_skybox_frag = {
#include "skybox.frag.spv.h"
};

MainPass::~MainPass() { clear(); }

void MainPass::initialize(RenderPassInitInfo *init_info) {
    RenderPass::initialize(init_info);

    MainPassInitInfo *_init_info = reinterpret_cast<MainPassInitInfo *>(init_info);
    m_point_light_shadow_color_image_view =
        _init_info->point_light_shadow_color_image_view;
    m_directional_light_shadow_color_image_view =
        _init_info->directional_light_shadow_color_image_view;

    createAttachments();
    createRenderPass();
    createDescriptorSetLayouts();
    createPipelines();
    allocateDecriptorSets();
    updateFramebufferDescriptors();
    createSwapchainFramebuffers();
}

void MainPass::clear() {
    for (auto pipeline : pipelines) {
        vkDestroyPipeline(m_ctx->device, pipeline, nullptr);
    }

    for (auto layout : pipeline_layouts) {
        vkDestroyPipelineLayout(m_ctx->device, layout, nullptr);
    }

    for (auto layout : descriptor_set_layouts) {
        vkDestroyDescriptorSetLayout(m_ctx->device, layout, nullptr);
    }

    clearAttachmentsAndFramebuffers();

    vkDestroyRenderPass(m_ctx->device, render_pass, nullptr);
}

void MainPass::draw(
    const RenderScene &scene,
    ToneMappingPass &tone_mapping_pass,
    UIPass &ui_pass,
    CombineUIPass &combine_ui_pass
) {
    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    {
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer =
            m_swapchain_framebuffers[m_ctx->currentSwapchainImageIndex()];
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = m_ctx->swapchain_extent;

        VkClearValue clear_values[_attachment_count];
        clear_values[_gbuffer_a].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_gbuffer_b].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_gbuffer_c].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_backup_buffer_odd].color = {
            {0.0, 0.0, 0.0, 1.0}
        };
        clear_values[_backup_buffer_even].color = {
            {0.0, 0.0, 0.0, 1.0}
        };
        clear_values[_depth_buffer].depthStencil = {1.0, 0};
        clear_values[_swapchain_image].color = {
            {0.0, 0.0, 0.0, 1.0}
        };

        render_pass_begin_info.clearValueCount = ARRAY_SIZE(clear_values);
        render_pass_begin_info.pClearValues = clear_values;

        m_ctx->cmdBeginRenderPass(
            command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE
        );
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    {
        m_ctx->pushEvent(command_buffer, "Base Pass", color);

        drawMeshGbuffer(scene);

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Deferred Lighting", color);

        drawDeferredLighting();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Forward Lighting", color);

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Tone Mapping", color);

        tone_mapping_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "UI", color);

        VkClearAttachment clear_attachments[1]{};
        clear_attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_attachments[0].colorAttachment = 0;
        clear_attachments[0].clearValue.color.float32[0] = 0.0;
        clear_attachments[0].clearValue.color.float32[1] = 0.0;
        clear_attachments[0].clearValue.color.float32[2] = 0.0;
        clear_attachments[0].clearValue.color.float32[3] = 0.0;
        VkClearRect clear_rects[1]{};
        clear_rects[0].baseArrayLayer = 0;
        clear_rects[0].layerCount = 1;
        clear_rects[0].rect.offset.x = 0;
        clear_rects[0].rect.offset.y = 0;
        clear_rects[0].rect.extent = m_ctx->swapchain_extent;

        m_ctx->cmdClearAttachments(command_buffer, 1, clear_attachments, 1, clear_rects);

        ui_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Combine UI", color);

        combine_ui_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdEndRenderPass(command_buffer);
}

void MainPass::drawForward(
    const RenderScene &scene,
    ToneMappingPass &tone_mapping_pass,
    UIPass &ui_pass,
    CombineUIPass &combine_ui_pass
) {
    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    {
        VkRenderPassBeginInfo render_pass_begin_info{};
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer =
            m_swapchain_framebuffers[m_ctx->currentSwapchainImageIndex()];
        render_pass_begin_info.renderArea.offset = {0, 0};
        render_pass_begin_info.renderArea.extent = m_ctx->swapchain_extent;

        VkClearValue clear_values[_attachment_count];
        clear_values[_gbuffer_a].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_gbuffer_b].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_gbuffer_c].color = {
            {0.0, 0.0, 0.0, 0.0}
        };
        clear_values[_backup_buffer_odd].color = {
            {0.0, 0.0, 0.0, 1.0}
        };
        clear_values[_backup_buffer_even].color = {
            {0.0, 0.0, 0.0, 1.0}
        };
        clear_values[_depth_buffer].depthStencil = {1.0, 0};
        clear_values[_swapchain_image].color = {
            {0.0, 0.0, 0.0, 1.0}
        };

        render_pass_begin_info.clearValueCount = ARRAY_SIZE(clear_values);
        render_pass_begin_info.pClearValues = clear_values;

        m_ctx->cmdBeginRenderPass(
            command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE
        );
    }

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    {
        m_ctx->pushEvent(command_buffer, "Base Pass", color);

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Deferred Lighting", color);

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Forward Lighting", color);

        drawMeshLighting(scene);
        drawSkybox();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Tone Mapping", color);

        tone_mapping_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "UI", color);

        VkClearAttachment clear_attachments[1]{};
        clear_attachments[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_attachments[0].colorAttachment = 0;
        clear_attachments[0].clearValue.color.float32[0] = 0.0;
        clear_attachments[0].clearValue.color.float32[1] = 0.0;
        clear_attachments[0].clearValue.color.float32[2] = 0.0;
        clear_attachments[0].clearValue.color.float32[3] = 0.0;
        VkClearRect clear_rects[1]{};
        clear_rects[0].baseArrayLayer = 0;
        clear_rects[0].layerCount = 1;
        clear_rects[0].rect.offset.x = 0;
        clear_rects[0].rect.offset.y = 0;
        clear_rects[0].rect.extent = m_ctx->swapchain_extent;

        m_ctx->cmdClearAttachments(command_buffer, 1, clear_attachments, 1, clear_rects);

        ui_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);

    {
        m_ctx->pushEvent(command_buffer, "Combine UI", color);

        combine_ui_pass.draw();

        m_ctx->popEvent(command_buffer);
    }

    m_ctx->cmdEndRenderPass(command_buffer);
}

void MainPass::onResize() {
    clearAttachmentsAndFramebuffers();

    createAttachments();
    updateFramebufferDescriptors();
    createSwapchainFramebuffers();
}

void MainPass::createAttachments() {
    framebuffer_info.attachments.resize(_custom_attachment_count);

    framebuffer_info.attachments[_gbuffer_a].format = VK_FORMAT_R8G8B8A8_UNORM;
    framebuffer_info.attachments[_gbuffer_b].format = VK_FORMAT_R8G8B8A8_UNORM;
    framebuffer_info.attachments[_gbuffer_c].format = VK_FORMAT_R8G8B8A8_SRGB;
    framebuffer_info.attachments[_backup_buffer_odd].format =
        VK_FORMAT_R16G16B16A16_SFLOAT;
    framebuffer_info.attachments[_backup_buffer_even].format =
        VK_FORMAT_R16G16B16A16_SFLOAT;

    for (uint32_t i = 0; i < _custom_attachment_count; ++i) {
        if (i == _gbuffer_a) {
            createImage(
                m_ctx->physical_device,
                m_ctx->device,
                m_ctx->swapchain_extent.width,
                m_ctx->swapchain_extent.height,
                framebuffer_info.attachments[_gbuffer_a].format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                1,
                1,
                framebuffer_info.attachments[_gbuffer_a].image,
                framebuffer_info.attachments[_gbuffer_a].memory
            );
        } else {
            createImage(
                m_ctx->physical_device,
                m_ctx->device,
                m_ctx->swapchain_extent.width,
                m_ctx->swapchain_extent.height,
                framebuffer_info.attachments[i].format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                1,
                1,
                framebuffer_info.attachments[i].image,
                framebuffer_info.attachments[i].memory
            );
        }

        framebuffer_info.attachments[i].view = createImageView(
            m_ctx->device,
            framebuffer_info.attachments[i].image,
            framebuffer_info.attachments[i].format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            1,
            1
        );
    }
}

void MainPass::createRenderPass() {
    VkAttachmentDescription attachments[_attachment_count]{};

    // normal
    attachments[_gbuffer_a].format = framebuffer_info.attachments[_gbuffer_a].format;
    attachments[_gbuffer_a].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_gbuffer_a].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_gbuffer_a].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[_gbuffer_a].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_gbuffer_a].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_gbuffer_a].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_gbuffer_a].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // metallic roughness, shading mode
    attachments[_gbuffer_b].format = framebuffer_info.attachments[_gbuffer_b].format;
    attachments[_gbuffer_b].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_gbuffer_b].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_gbuffer_b].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[_gbuffer_b].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_gbuffer_b].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_gbuffer_b].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_gbuffer_b].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // albedo
    attachments[_gbuffer_c].format = framebuffer_info.attachments[_gbuffer_c].format;
    attachments[_gbuffer_c].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_gbuffer_c].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_gbuffer_c].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_gbuffer_c].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_gbuffer_c].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_gbuffer_c].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_gbuffer_c].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    attachments[_backup_buffer_odd].format =
        framebuffer_info.attachments[_backup_buffer_odd].format;
    attachments[_backup_buffer_odd].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_backup_buffer_odd].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_backup_buffer_odd].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_backup_buffer_odd].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_backup_buffer_odd].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_backup_buffer_odd].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_backup_buffer_odd].finalLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    attachments[_backup_buffer_even].format =
        framebuffer_info.attachments[_backup_buffer_even].format;
    attachments[_backup_buffer_even].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_backup_buffer_even].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_backup_buffer_even].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_backup_buffer_even].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_backup_buffer_even].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_backup_buffer_even].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_backup_buffer_even].finalLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    attachments[_depth_buffer].format = m_ctx->depth_image_format;
    attachments[_depth_buffer].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_depth_buffer].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_depth_buffer].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[_depth_buffer].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_depth_buffer].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_depth_buffer].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_depth_buffer].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    attachments[_swapchain_image].format = m_ctx->swapchain_image_format;
    attachments[_swapchain_image].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[_swapchain_image].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[_swapchain_image].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[_swapchain_image].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[_swapchain_image].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[_swapchain_image].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[_swapchain_image].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSubpassDescription subpasses[_subpass_count]{};

    VkAttachmentReference base_pass_color_attachments_reference[3] = {};
    base_pass_color_attachments_reference[0].attachment = _gbuffer_a;
    base_pass_color_attachments_reference[0].layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    base_pass_color_attachments_reference[1].attachment = _gbuffer_b;
    base_pass_color_attachments_reference[1].layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    base_pass_color_attachments_reference[2].attachment = _gbuffer_c;
    base_pass_color_attachments_reference[2].layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference base_pass_depth_attachment_reference{};
    base_pass_depth_attachment_reference.attachment = _depth_buffer;
    base_pass_depth_attachment_reference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription &base_pass = subpasses[_subpass_basepass];
    base_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    base_pass.colorAttachmentCount = ARRAY_SIZE(base_pass_color_attachments_reference);
    base_pass.pColorAttachments = &base_pass_color_attachments_reference[0];
    base_pass.pDepthStencilAttachment = &base_pass_depth_attachment_reference;
    base_pass.preserveAttachmentCount = 0;
    base_pass.pPreserveAttachments = nullptr;

    VkAttachmentReference deferred_lighting_pass_input_attachments_reference[4] = {};
    deferred_lighting_pass_input_attachments_reference[0].attachment = _gbuffer_a;
    deferred_lighting_pass_input_attachments_reference[0].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    deferred_lighting_pass_input_attachments_reference[1].attachment = _gbuffer_b;
    deferred_lighting_pass_input_attachments_reference[1].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    deferred_lighting_pass_input_attachments_reference[2].attachment = _gbuffer_c;
    deferred_lighting_pass_input_attachments_reference[2].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    deferred_lighting_pass_input_attachments_reference[3].attachment = _depth_buffer;
    deferred_lighting_pass_input_attachments_reference[3].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference deferred_lighting_pass_color_attachment_reference[1] = {};
    deferred_lighting_pass_color_attachment_reference[0].attachment = _backup_buffer_odd;
    deferred_lighting_pass_color_attachment_reference[0].layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription &deferred_lighting_pass = subpasses[_subpass_deferred_lighting];
    deferred_lighting_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    deferred_lighting_pass.inputAttachmentCount =
        ARRAY_SIZE(deferred_lighting_pass_input_attachments_reference);
    deferred_lighting_pass.pInputAttachments =
        deferred_lighting_pass_input_attachments_reference;
    deferred_lighting_pass.colorAttachmentCount =
        ARRAY_SIZE(deferred_lighting_pass_color_attachment_reference);
    deferred_lighting_pass.pColorAttachments =
        deferred_lighting_pass_color_attachment_reference;

    VkAttachmentReference forward_lighting_pass_color_attachments_reference[1] = {};
    forward_lighting_pass_color_attachments_reference[0].attachment = _backup_buffer_odd;
    forward_lighting_pass_color_attachments_reference[0].layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference forward_lighting_pass_depth_attachment_reference{};
    forward_lighting_pass_depth_attachment_reference.attachment = _depth_buffer;
    forward_lighting_pass_depth_attachment_reference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription &forward_lighting_pass = subpasses[_subpass_forward_lighting];
    forward_lighting_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    forward_lighting_pass.colorAttachmentCount =
        ARRAY_SIZE(forward_lighting_pass_color_attachments_reference);
    forward_lighting_pass.pColorAttachments =
        forward_lighting_pass_color_attachments_reference;
    forward_lighting_pass.pDepthStencilAttachment =
        &forward_lighting_pass_depth_attachment_reference;

    VkAttachmentReference tone_mapping_pass_input_attachment_reference{};
    tone_mapping_pass_input_attachment_reference.attachment = _backup_buffer_odd;
    tone_mapping_pass_input_attachment_reference.layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference tone_mapping_pass_color_attachment_reference{};
    tone_mapping_pass_color_attachment_reference.attachment = _backup_buffer_even;
    tone_mapping_pass_color_attachment_reference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription &tone_mapping_pass = subpasses[_subpass_tone_mapping];
    tone_mapping_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    tone_mapping_pass.inputAttachmentCount = 1;
    tone_mapping_pass.pInputAttachments = &tone_mapping_pass_input_attachment_reference;
    tone_mapping_pass.colorAttachmentCount = 1;
    tone_mapping_pass.pColorAttachments = &tone_mapping_pass_color_attachment_reference;

    VkAttachmentReference ui_pass_color_attachment_reference{};
    ui_pass_color_attachment_reference.attachment = _backup_buffer_odd;
    ui_pass_color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    uint32_t ui_pass_preserve_attachment = _backup_buffer_even;

    VkSubpassDescription &ui_pass = subpasses[_subpass_ui];
    ui_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    ui_pass.colorAttachmentCount = 1;
    ui_pass.pColorAttachments = &ui_pass_color_attachment_reference;
    ui_pass.preserveAttachmentCount = 1;
    ui_pass.pPreserveAttachments = &ui_pass_preserve_attachment;

    VkAttachmentReference combine_ui_pass_input_attachments_reference[2] = {};
    combine_ui_pass_input_attachments_reference[0].attachment = _backup_buffer_even;
    combine_ui_pass_input_attachments_reference[0].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    combine_ui_pass_input_attachments_reference[1].attachment = _backup_buffer_odd;
    combine_ui_pass_input_attachments_reference[1].layout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference combine_ui_pass_color_attachment_reference{};
    combine_ui_pass_color_attachment_reference.attachment = _swapchain_image;
    combine_ui_pass_color_attachment_reference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription &combine_ui_pass = subpasses[_subpass_combine_ui];
    combine_ui_pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    combine_ui_pass.inputAttachmentCount =
        ARRAY_SIZE(combine_ui_pass_input_attachments_reference);
    combine_ui_pass.pInputAttachments = combine_ui_pass_input_attachments_reference;
    combine_ui_pass.colorAttachmentCount = 1;
    combine_ui_pass.pColorAttachments = &combine_ui_pass_color_attachment_reference;

    VkSubpassDependency dependencies[6] = {};

    // deferred lighting pass depend on shadow map pass
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = _subpass_deferred_lighting;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dependencyFlags = 0;  // NOT BY REGION

    // deferred lighting pass depend on base pass
    dependencies[1].srcSubpass = _subpass_basepass;
    dependencies[1].dstSubpass = _subpass_deferred_lighting;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // forward lighting pass depend on deferred lighting pass
    dependencies[2].srcSubpass = _subpass_deferred_lighting;
    dependencies[2].dstSubpass = _subpass_forward_lighting;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].srcAccessMask =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // tone mapping pass depend on lighting pass
    dependencies[3].srcSubpass = _subpass_forward_lighting;
    dependencies[3].dstSubpass = _subpass_tone_mapping;
    dependencies[3].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[3].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[3].srcAccessMask =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[3].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // ui pass depend on tone mapping pass
    dependencies[4].srcSubpass = _subpass_tone_mapping;
    dependencies[4].dstSubpass = _subpass_ui;
    dependencies[4].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[4].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[4].srcAccessMask =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[4].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[4].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // combine ui pass depend on ui pass
    dependencies[5].srcSubpass = _subpass_ui;
    dependencies[5].dstSubpass = _subpass_combine_ui;
    dependencies[5].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[5].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[5].srcAccessMask =
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[5].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[5].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = ARRAY_SIZE(attachments);
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = ARRAY_SIZE(subpasses);
    render_pass_create_info.pSubpasses = subpasses;
    render_pass_create_info.dependencyCount = ARRAY_SIZE(dependencies);
    render_pass_create_info.pDependencies = dependencies;

    VkResult res = vkCreateRenderPass(
        m_ctx->device, &render_pass_create_info, nullptr, &render_pass
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create render pass");
    }
}

void MainPass::createDescriptorSetLayouts() {
    descriptor_set_layouts.resize(_layout_type_count);

    {
        VkDescriptorSetLayoutBinding mesh_global_layout_bindings[7]{};

        VkDescriptorSetLayoutBinding
            &mesh_global_layout_per_frame_storage_buffer_binding =
                mesh_global_layout_bindings[0];
        mesh_global_layout_per_frame_storage_buffer_binding.binding = 0;
        mesh_global_layout_per_frame_storage_buffer_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        mesh_global_layout_per_frame_storage_buffer_binding.descriptorCount = 1;
        mesh_global_layout_per_frame_storage_buffer_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding
            &mesh_global_layout_per_drawcall_storage_buffer_binding =
                mesh_global_layout_bindings[1];
        mesh_global_layout_per_drawcall_storage_buffer_binding.binding = 1;
        mesh_global_layout_per_drawcall_storage_buffer_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        mesh_global_layout_per_drawcall_storage_buffer_binding.descriptorCount = 1;
        mesh_global_layout_per_drawcall_storage_buffer_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding &mesh_global_layout_brdfLUT_texture_binding =
            mesh_global_layout_bindings[2];
        mesh_global_layout_brdfLUT_texture_binding.binding = 2;
        mesh_global_layout_brdfLUT_texture_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mesh_global_layout_brdfLUT_texture_binding.descriptorCount = 1;
        mesh_global_layout_brdfLUT_texture_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding &mesh_global_layout_irradiance_texture_binding =
            mesh_global_layout_bindings[3];
        mesh_global_layout_irradiance_texture_binding =
            mesh_global_layout_brdfLUT_texture_binding;
        mesh_global_layout_irradiance_texture_binding.binding = 3;

        VkDescriptorSetLayoutBinding &mesh_global_layout_specular_texture_binding =
            mesh_global_layout_bindings[4];
        mesh_global_layout_specular_texture_binding =
            mesh_global_layout_brdfLUT_texture_binding;
        mesh_global_layout_specular_texture_binding.binding = 4;

        VkDescriptorSetLayoutBinding
            &mesh_global_layout_point_light_shadow_texture_binding =
                mesh_global_layout_bindings[5];
        mesh_global_layout_point_light_shadow_texture_binding =
            mesh_global_layout_brdfLUT_texture_binding;
        mesh_global_layout_point_light_shadow_texture_binding.binding = 5;

        VkDescriptorSetLayoutBinding
            &mesh_global_layout_directional_light_shadow_texture_binding =
                mesh_global_layout_bindings[6];
        mesh_global_layout_directional_light_shadow_texture_binding =
            mesh_global_layout_brdfLUT_texture_binding;
        mesh_global_layout_directional_light_shadow_texture_binding.binding = 6;

        VkDescriptorSetLayoutCreateInfo mesh_global_layout_create_info{};
        mesh_global_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        mesh_global_layout_create_info.bindingCount =
            ARRAY_SIZE(mesh_global_layout_bindings);
        mesh_global_layout_create_info.pBindings = mesh_global_layout_bindings;

        VkResult res = vkCreateDescriptorSetLayout(
            m_ctx->device,
            &mesh_global_layout_create_info,
            nullptr,
            &descriptor_set_layouts[_layout_type_mesh_global]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create mesh global layout");
        }
    }

    {
        VkDescriptorSetLayoutBinding mesh_material_layout_bindings[6]{};

        // (set = 1, binding = 0 in fragment shader)
        VkDescriptorSetLayoutBinding &mesh_material_layout_uniform_buffer_binding =
            mesh_material_layout_bindings[0];
        mesh_material_layout_uniform_buffer_binding.binding = 0;
        mesh_material_layout_uniform_buffer_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mesh_material_layout_uniform_buffer_binding.descriptorCount = 1;
        mesh_material_layout_uniform_buffer_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;
        mesh_material_layout_uniform_buffer_binding.pImmutableSamplers = nullptr;

        // (set = 1, binding = 1 in fragment shader)
        VkDescriptorSetLayoutBinding &mesh_material_layout_base_color_texture_binding =
            mesh_material_layout_bindings[1];
        mesh_material_layout_base_color_texture_binding.binding = 1;
        mesh_material_layout_base_color_texture_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mesh_material_layout_base_color_texture_binding.descriptorCount = 1;
        mesh_material_layout_base_color_texture_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;
        mesh_material_layout_base_color_texture_binding.pImmutableSamplers = nullptr;

        // (set = 1, binding = 2 in fragment shader)
        VkDescriptorSetLayoutBinding
            &mesh_material_layout_matallic_roughness_texture_binding =
                mesh_material_layout_bindings[2];
        mesh_material_layout_matallic_roughness_texture_binding =
            mesh_material_layout_base_color_texture_binding;
        mesh_material_layout_matallic_roughness_texture_binding.binding = 2;

        // (set = 1, binding = 3 in fragment shader)
        VkDescriptorSetLayoutBinding &mesh_material_layout_normal_texture_binding =
            mesh_material_layout_bindings[3];
        mesh_material_layout_normal_texture_binding =
            mesh_material_layout_base_color_texture_binding;
        mesh_material_layout_normal_texture_binding.binding = 3;

        // (set = 1, binding = 4 in fragment shader)
        VkDescriptorSetLayoutBinding &mesh_material_layout_occlusion_texture_binding =
            mesh_material_layout_bindings[4];
        mesh_material_layout_occlusion_texture_binding =
            mesh_material_layout_base_color_texture_binding;
        mesh_material_layout_occlusion_texture_binding.binding = 4;

        // (set = 1, binding = 5 in fragment shader)
        VkDescriptorSetLayoutBinding &mesh_material_layout_emissive_texture_binding =
            mesh_material_layout_bindings[5];
        mesh_material_layout_emissive_texture_binding =
            mesh_material_layout_base_color_texture_binding;
        mesh_material_layout_emissive_texture_binding.binding = 5;

        VkDescriptorSetLayoutCreateInfo mesh_material_layout_create_info{};
        mesh_material_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        mesh_material_layout_create_info.bindingCount =
            ARRAY_SIZE(mesh_material_layout_bindings);
        mesh_material_layout_create_info.pBindings = mesh_material_layout_bindings;

        VkResult res = vkCreateDescriptorSetLayout(
            m_ctx->device,
            &mesh_material_layout_create_info,
            nullptr,
            &descriptor_set_layouts[_layout_type_mesh_per_material]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create mesh material layout");
        }
    }

    {
        VkDescriptorSetLayoutBinding skybox_layout_bindings[2]{};

        VkDescriptorSetLayoutBinding &skybox_layout_perframe_storage_buffer_binding =
            skybox_layout_bindings[0];
        skybox_layout_perframe_storage_buffer_binding.binding = 0;
        skybox_layout_perframe_storage_buffer_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        skybox_layout_perframe_storage_buffer_binding.descriptorCount = 1;
        skybox_layout_perframe_storage_buffer_binding.stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding &skybox_layout_specular_texture_binding =
            skybox_layout_bindings[1];
        skybox_layout_specular_texture_binding.binding = 1;
        skybox_layout_specular_texture_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        skybox_layout_specular_texture_binding.descriptorCount = 1;
        skybox_layout_specular_texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo skybox_layout_create_info{};
        skybox_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        skybox_layout_create_info.bindingCount = ARRAY_SIZE(skybox_layout_bindings);
        skybox_layout_create_info.pBindings = skybox_layout_bindings;

        VkResult res = vkCreateDescriptorSetLayout(
            m_ctx->device,
            &skybox_layout_create_info,
            nullptr,
            &descriptor_set_layouts[_layout_type_skybox]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create skybox layout");
        }
    }

    {
        VkDescriptorSetLayoutBinding gbuffer_lighting_global_layout_bindings[4]{};

        VkDescriptorSetLayoutBinding
            &gbuffer_normal_global_layout_input_attachment_binding =
                gbuffer_lighting_global_layout_bindings[0];
        gbuffer_normal_global_layout_input_attachment_binding.binding = 0;
        gbuffer_normal_global_layout_input_attachment_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        gbuffer_normal_global_layout_input_attachment_binding.descriptorCount = 1;
        gbuffer_normal_global_layout_input_attachment_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding &
            gbuffer_metallic_roughness_shading_mode_id_global_layout_input_attachment_binding =
                gbuffer_lighting_global_layout_bindings[1];
        gbuffer_metallic_roughness_shading_mode_id_global_layout_input_attachment_binding
            .binding = 1;
        gbuffer_metallic_roughness_shading_mode_id_global_layout_input_attachment_binding
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        gbuffer_metallic_roughness_shading_mode_id_global_layout_input_attachment_binding
            .descriptorCount = 1;
        gbuffer_metallic_roughness_shading_mode_id_global_layout_input_attachment_binding
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding
            &gbuffer_albedo_global_layout_input_attachment_binding =
                gbuffer_lighting_global_layout_bindings[2];
        gbuffer_albedo_global_layout_input_attachment_binding.binding = 2;
        gbuffer_albedo_global_layout_input_attachment_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        gbuffer_albedo_global_layout_input_attachment_binding.descriptorCount = 1;
        gbuffer_albedo_global_layout_input_attachment_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding
            &gbuffer_depth_global_layout_input_attachment_binding =
                gbuffer_lighting_global_layout_bindings[3];
        gbuffer_depth_global_layout_input_attachment_binding.binding = 3;
        gbuffer_depth_global_layout_input_attachment_binding.descriptorType =
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        gbuffer_depth_global_layout_input_attachment_binding.descriptorCount = 1;
        gbuffer_depth_global_layout_input_attachment_binding.stageFlags =
            VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo gbuffer_lighting_global_layout_create_info{};
        gbuffer_lighting_global_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        gbuffer_lighting_global_layout_create_info.bindingCount =
            ARRAY_SIZE(gbuffer_lighting_global_layout_bindings);
        gbuffer_lighting_global_layout_create_info.pBindings =
            gbuffer_lighting_global_layout_bindings;

        VkResult res = vkCreateDescriptorSetLayout(
            m_ctx->device,
            &gbuffer_lighting_global_layout_create_info,
            nullptr,
            &descriptor_set_layouts[_layout_type_deferred_lighting]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create deferred lighting layout");
        }
    }
}

void MainPass::createPipelines() {
    pipelines.resize(_pipeline_type_count);
    pipeline_layouts.resize(_pipeline_type_count);

    // mesh gbuffer
    {
        VkDescriptorSetLayout layouts[2] = {
            descriptor_set_layouts[_layout_type_mesh_global],
            descriptor_set_layouts[_layout_type_mesh_per_material]
        };
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = ARRAY_SIZE(layouts);
        pipeline_layout_create_info.pSetLayouts = layouts;

        VkResult res = vkCreatePipelineLayout(
            m_ctx->device,
            &pipeline_layout_create_info,
            nullptr,
            &pipeline_layouts[_pipeline_type_mesh_gbuffer]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create mesh gbuffer pipeline layout");
        }

        VkShaderModule vert_shader_module =
            createShaderModule(m_ctx->device, s_mesh_vert);
        VkShaderModule frag_shader_module =
            createShaderModule(m_ctx->device, s_mesh_gbuffer_frag);

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
        vertex_input_state_create_info.vertexBindingDescriptionCount =
            vertex_binding_descriptions.size();
        vertex_input_state_create_info.pVertexBindingDescriptions =
            &vertex_binding_descriptions[0];
        vertex_input_state_create_info.vertexAttributeDescriptionCount =
            vertex_attribute_descriptions.size();
        vertex_input_state_create_info.pVertexAttributeDescriptions =
            &vertex_attribute_descriptions[0];

        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
        input_assembly_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

        VkPipelineColorBlendAttachmentState color_blend_attachments[3] = {};
        color_blend_attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[0].blendEnable = VK_FALSE;
        color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[1].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[1].blendEnable = VK_FALSE;
        color_blend_attachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[1].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[1].alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[2].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[2].blendEnable = VK_FALSE;
        color_blend_attachments[2].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[2].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[2].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[2].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[2].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[2].alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount =
            ARRAY_SIZE(color_blend_attachments);
        color_blend_state_create_info.pAttachments = &color_blend_attachments[0];
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
        pipeline_info.layout = pipeline_layouts[_pipeline_type_mesh_gbuffer];
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = _subpass_basepass;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.pDynamicState = &dynamic_state_create_info;

        res = vkCreateGraphicsPipelines(
            m_ctx->device,
            nullptr,
            1,
            &pipeline_info,
            nullptr,
            &pipelines[_pipeline_type_mesh_gbuffer]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create mesh gbuffer graphics pipeline");
        }

        vkDestroyShaderModule(m_ctx->device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_ctx->device, frag_shader_module, nullptr);
    }

    // deferred lighting
    {
        VkDescriptorSetLayout layouts[3] = {
            descriptor_set_layouts[_layout_type_mesh_global],
            descriptor_set_layouts[_layout_type_deferred_lighting],
            descriptor_set_layouts[_layout_type_skybox]
        };
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = ARRAY_SIZE(layouts);
        pipeline_layout_create_info.pSetLayouts = layouts;

        VkResult res = vkCreatePipelineLayout(
            m_ctx->device,
            &pipeline_layout_create_info,
            nullptr,
            &pipeline_layouts[_pipeline_type_deferred_lighting]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create deferred lighting pipeline layout");
        }

        VkShaderModule vert_shader_module =
            createShaderModule(m_ctx->device, s_deferred_lighting_vert);
        VkShaderModule frag_shader_module =
            createShaderModule(m_ctx->device, s_deferred_lighting_frag);

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
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

        VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {};
        color_blend_attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[0].blendEnable = VK_FALSE;
        color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount =
            ARRAY_SIZE(color_blend_attachments);
        color_blend_state_create_info.pAttachments = &color_blend_attachments[0];
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info{};
        depth_stencil_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_create_info.depthTestEnable = VK_FALSE;
        depth_stencil_create_info.depthWriteEnable = VK_FALSE;
        depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
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
        pipeline_info.layout = pipeline_layouts[_pipeline_type_deferred_lighting];
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = _subpass_deferred_lighting;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.pDynamicState = &dynamic_state_create_info;

        res = vkCreateGraphicsPipelines(
            m_ctx->device,
            nullptr,
            1,
            &pipeline_info,
            nullptr,
            &pipelines[_pipeline_type_deferred_lighting]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create deferred lighting graphics pipeline");
        }

        vkDestroyShaderModule(m_ctx->device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_ctx->device, frag_shader_module, nullptr);
    }

    // mesh lighting
    {
        VkDescriptorSetLayout layouts[2] = {
            descriptor_set_layouts[_layout_type_mesh_global],
            descriptor_set_layouts[_layout_type_mesh_per_material]
        };
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = ARRAY_SIZE(layouts);
        pipeline_layout_create_info.pSetLayouts = layouts;

        VkResult res = vkCreatePipelineLayout(
            m_ctx->device,
            &pipeline_layout_create_info,
            nullptr,
            &pipeline_layouts[_pipeline_type_mesh_lighting]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to allocate mesh lighting pipeline layout");
        }

        VkShaderModule vert_shader_module =
            createShaderModule(m_ctx->device, s_mesh_vert);
        VkShaderModule frag_shader_module =
            createShaderModule(m_ctx->device, s_mesh_frag);

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
        vertex_input_state_create_info.vertexBindingDescriptionCount =
            vertex_binding_descriptions.size();
        vertex_input_state_create_info.pVertexBindingDescriptions =
            &vertex_binding_descriptions[0];
        vertex_input_state_create_info.vertexAttributeDescriptionCount =
            vertex_attribute_descriptions.size();
        vertex_input_state_create_info.pVertexAttributeDescriptions =
            &vertex_attribute_descriptions[0];

        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
        input_assembly_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

        VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {};
        color_blend_attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[0].blendEnable = VK_FALSE;
        color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount =
            ARRAY_SIZE(color_blend_attachments);
        color_blend_state_create_info.pAttachments = &color_blend_attachments[0];
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
        pipeline_info.layout = pipeline_layouts[_pipeline_type_mesh_lighting];
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = _subpass_forward_lighting;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.pDynamicState = &dynamic_state_create_info;

        res = vkCreateGraphicsPipelines(
            m_ctx->device,
            nullptr,
            1,
            &pipeline_info,
            nullptr,
            &pipelines[_pipeline_type_mesh_lighting]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create mesh lighting graphics pipeline");
        }

        vkDestroyShaderModule(m_ctx->device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_ctx->device, frag_shader_module, nullptr);
    }

    // skybox
    {
        VkDescriptorSetLayout layouts[1] = {descriptor_set_layouts[_layout_type_skybox]};
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = ARRAY_SIZE(layouts);
        pipeline_layout_create_info.pSetLayouts = layouts;

        VkResult res = vkCreatePipelineLayout(
            m_ctx->device,
            &pipeline_layout_create_info,
            nullptr,
            &pipeline_layouts[_pipeline_type_skybox]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create skybox pipeline layout");
        }

        VkShaderModule vert_shader_module =
            createShaderModule(m_ctx->device, s_skybox_vert);
        VkShaderModule frag_shader_module =
            createShaderModule(m_ctx->device, s_skybox_frag);

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
        vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
        vertex_input_state_create_info.pVertexBindingDescriptions = nullptr;
        vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;
        vertex_input_state_create_info.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info{};
        input_assembly_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

        VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {};
        color_blend_attachments[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachments[0].blendEnable = VK_FALSE;
        color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount =
            ARRAY_SIZE(color_blend_attachments);
        color_blend_state_create_info.pAttachments = &color_blend_attachments[0];
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
        pipeline_info.layout = pipeline_layouts[_pipeline_type_skybox];
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = _subpass_forward_lighting;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.pDynamicState = &dynamic_state_create_info;

        res = vkCreateGraphicsPipelines(
            m_ctx->device,
            nullptr,
            1,
            &pipeline_info,
            nullptr,
            &pipelines[_pipeline_type_skybox]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create skybox graphics pipeline");
        }

        vkDestroyShaderModule(m_ctx->device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_ctx->device, frag_shader_module, nullptr);
    }
}

void MainPass::allocateDecriptorSets() {
    descriptor_sets.resize(_layout_type_count);

    allocateMeshGlobalDescriptorSet();
    allocateSkyBoxDescriptorSet();
    allocateGbufferLightingDescriptorSet();
}

void MainPass::updateFramebufferDescriptors() {
    VkDescriptorImageInfo gbuffer_normal_input_attachment_info{};
    gbuffer_normal_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    gbuffer_normal_input_attachment_info.imageView =
        framebuffer_info.attachments[_gbuffer_a].view;
    gbuffer_normal_input_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo
        gbuffer_metallic_roughness_shading_mode_id_input_attachment_info{};
    gbuffer_metallic_roughness_shading_mode_id_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    gbuffer_metallic_roughness_shading_mode_id_input_attachment_info.imageView =
        framebuffer_info.attachments[_gbuffer_b].view;
    gbuffer_metallic_roughness_shading_mode_id_input_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo gbuffer_albedo_input_attachment_info{};
    gbuffer_albedo_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    gbuffer_albedo_input_attachment_info.imageView =
        framebuffer_info.attachments[_gbuffer_c].view;
    gbuffer_albedo_input_attachment_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depth_input_attachment_info{};
    depth_input_attachment_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    depth_input_attachment_info.imageView = m_ctx->depth_image_view;
    depth_input_attachment_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet deferred_lighting_descriptor_writes[4]{};

    deferred_lighting_descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    deferred_lighting_descriptor_writes[0].dstSet =
        descriptor_sets[_layout_type_deferred_lighting];
    deferred_lighting_descriptor_writes[0].dstBinding = 0;
    deferred_lighting_descriptor_writes[0].dstArrayElement = 0;
    deferred_lighting_descriptor_writes[0].descriptorCount = 1;
    deferred_lighting_descriptor_writes[0].descriptorType =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    deferred_lighting_descriptor_writes[0].pImageInfo =
        &gbuffer_normal_input_attachment_info;

    deferred_lighting_descriptor_writes[1] = deferred_lighting_descriptor_writes[0];
    deferred_lighting_descriptor_writes[1] = deferred_lighting_descriptor_writes[1];
    deferred_lighting_descriptor_writes[1].dstBinding = 1;
    deferred_lighting_descriptor_writes[1].pImageInfo =
        &gbuffer_metallic_roughness_shading_mode_id_input_attachment_info;

    deferred_lighting_descriptor_writes[2] = deferred_lighting_descriptor_writes[0];
    deferred_lighting_descriptor_writes[2] = deferred_lighting_descriptor_writes[1];
    deferred_lighting_descriptor_writes[2].dstBinding = 2;
    deferred_lighting_descriptor_writes[2].pImageInfo =
        &gbuffer_albedo_input_attachment_info;

    deferred_lighting_descriptor_writes[3] = deferred_lighting_descriptor_writes[0];
    deferred_lighting_descriptor_writes[3] = deferred_lighting_descriptor_writes[1];
    deferred_lighting_descriptor_writes[3].dstBinding = 3;
    deferred_lighting_descriptor_writes[3].pImageInfo = &depth_input_attachment_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(deferred_lighting_descriptor_writes),
        deferred_lighting_descriptor_writes,
        0,
        nullptr
    );
}

void MainPass::createSwapchainFramebuffers() {
    m_swapchain_framebuffers.resize(m_ctx->swapchain_image_views.size());

    // create frame buffer for every imageview
    for (size_t i = 0; i < m_swapchain_framebuffers.size(); i++) {
        VkImageView framebuffer_attachments_for_image_view[_attachment_count] = {
            framebuffer_info.attachments[_gbuffer_a].view,
            framebuffer_info.attachments[_gbuffer_b].view,
            framebuffer_info.attachments[_gbuffer_c].view,
            framebuffer_info.attachments[_backup_buffer_odd].view,
            framebuffer_info.attachments[_backup_buffer_even].view,
            m_ctx->depth_image_view,
            m_ctx->swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.flags = 0;
        framebuffer_create_info.renderPass = render_pass;
        framebuffer_create_info.attachmentCount =
            ARRAY_SIZE(framebuffer_attachments_for_image_view);
        framebuffer_create_info.pAttachments = framebuffer_attachments_for_image_view;
        framebuffer_create_info.width = m_ctx->swapchain_extent.width;
        framebuffer_create_info.height = m_ctx->swapchain_extent.height;
        framebuffer_create_info.layers = 1;

        VkResult res = vkCreateFramebuffer(
            m_ctx->device, &framebuffer_create_info, nullptr, &m_swapchain_framebuffers[i]
        );
        if (res != VK_SUCCESS) {
            VAIN_ERROR("failed to create frame buffer");
        }
    }
}

void MainPass::allocateMeshGlobalDescriptorSet() {
    VkDescriptorSetAllocateInfo mesh_global_descriptor_set_alloc_info{};
    mesh_global_descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    mesh_global_descriptor_set_alloc_info.descriptorPool = m_ctx->descriptor_pool;
    mesh_global_descriptor_set_alloc_info.descriptorSetCount = 1;
    mesh_global_descriptor_set_alloc_info.pSetLayouts =
        &descriptor_set_layouts[_layout_type_mesh_global];

    VkResult res = vkAllocateDescriptorSets(
        m_ctx->device,
        &mesh_global_descriptor_set_alloc_info,
        &descriptor_sets[_layout_type_mesh_global]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create mesh global set");
    }

    VkDescriptorBufferInfo mesh_per_frame_storage_buffer_info{};
    mesh_per_frame_storage_buffer_info.buffer =
        m_res->global_render_resource.storage_buffer.global_upload_ringbuffer;
    mesh_per_frame_storage_buffer_info.offset = 0;
    mesh_per_frame_storage_buffer_info.range = sizeof(MeshPerFrameStorageBufferObject);
    assert(
        mesh_per_frame_storage_buffer_info.range <
        m_res->global_render_resource.storage_buffer.max_storage_buffer_range
    );

    VkDescriptorBufferInfo mesh_per_drawcall_storage_buffer_info{};
    mesh_per_drawcall_storage_buffer_info.buffer =
        m_res->global_render_resource.storage_buffer.global_upload_ringbuffer;
    mesh_per_drawcall_storage_buffer_info.offset = 0;
    mesh_per_drawcall_storage_buffer_info.range =
        sizeof(MeshPerDrawcallStorageBufferObject);
    assert(
        mesh_per_drawcall_storage_buffer_info.range <
        m_res->global_render_resource.storage_buffer.max_storage_buffer_range
    );

    VkDescriptorImageInfo brdf_texture_image_info = {};
    brdf_texture_image_info.sampler =
        m_res->global_render_resource.ibl_resource.brdfLUT_texture_sampler;
    brdf_texture_image_info.imageView =
        m_res->global_render_resource.ibl_resource.brdfLUT_texture_image_view;
    brdf_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo irradiance_texture_image_info = {};
    irradiance_texture_image_info.sampler =
        m_res->global_render_resource.ibl_resource.irradiance_texture_sampler;
    irradiance_texture_image_info.imageView =
        m_res->global_render_resource.ibl_resource.irradiance_texture_image_view;
    irradiance_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo specular_texture_image_info{};
    specular_texture_image_info.sampler =
        m_res->global_render_resource.ibl_resource.specular_texture_sampler;
    specular_texture_image_info.imageView =
        m_res->global_render_resource.ibl_resource.specular_texture_image_view;
    specular_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo point_light_shadow_texture_image_info{};
    point_light_shadow_texture_image_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    point_light_shadow_texture_image_info.imageView =
        m_point_light_shadow_color_image_view;
    point_light_shadow_texture_image_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo directional_light_shadow_texture_image_info{};
    directional_light_shadow_texture_image_info.sampler =
        m_ctx->getOrCreateDefaultSampler(DefaultSamplerType::DEFAULT_SAMPLER_NEAREST);
    directional_light_shadow_texture_image_info.imageView =
        m_directional_light_shadow_color_image_view;
    directional_light_shadow_texture_image_info.imageLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet mesh_global_descriptor_writes[7]{};

    mesh_global_descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    mesh_global_descriptor_writes[0].dstSet = descriptor_sets[_layout_type_mesh_global];
    mesh_global_descriptor_writes[0].dstBinding = 0;
    mesh_global_descriptor_writes[0].dstArrayElement = 0;
    mesh_global_descriptor_writes[0].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    mesh_global_descriptor_writes[0].descriptorCount = 1;
    mesh_global_descriptor_writes[0].pBufferInfo = &mesh_per_frame_storage_buffer_info;

    mesh_global_descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    mesh_global_descriptor_writes[1].dstSet = descriptor_sets[_layout_type_mesh_global];
    mesh_global_descriptor_writes[1].dstBinding = 1;
    mesh_global_descriptor_writes[1].dstArrayElement = 0;
    mesh_global_descriptor_writes[1].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    mesh_global_descriptor_writes[1].descriptorCount = 1;
    mesh_global_descriptor_writes[1].pBufferInfo = &mesh_per_drawcall_storage_buffer_info;

    mesh_global_descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    mesh_global_descriptor_writes[2].dstSet = descriptor_sets[_layout_type_mesh_global];
    mesh_global_descriptor_writes[2].dstBinding = 2;
    mesh_global_descriptor_writes[2].dstArrayElement = 0;
    mesh_global_descriptor_writes[2].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    mesh_global_descriptor_writes[2].descriptorCount = 1;
    mesh_global_descriptor_writes[2].pImageInfo = &brdf_texture_image_info;

    mesh_global_descriptor_writes[3] = mesh_global_descriptor_writes[2];
    mesh_global_descriptor_writes[3].dstBinding = 3;
    mesh_global_descriptor_writes[3].pImageInfo = &irradiance_texture_image_info;

    mesh_global_descriptor_writes[4] = mesh_global_descriptor_writes[2];
    mesh_global_descriptor_writes[4].dstBinding = 4;
    mesh_global_descriptor_writes[4].pImageInfo = &specular_texture_image_info;

    mesh_global_descriptor_writes[5] = mesh_global_descriptor_writes[2];
    mesh_global_descriptor_writes[5].dstBinding = 5;
    mesh_global_descriptor_writes[5].pImageInfo = &point_light_shadow_texture_image_info;

    mesh_global_descriptor_writes[6] = mesh_global_descriptor_writes[2];
    mesh_global_descriptor_writes[6].dstBinding = 6;
    mesh_global_descriptor_writes[6].pImageInfo =
        &directional_light_shadow_texture_image_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(mesh_global_descriptor_writes),
        mesh_global_descriptor_writes,
        0,
        nullptr
    );
}

void MainPass::allocateSkyBoxDescriptorSet() {
    VkDescriptorSetAllocateInfo skybox_descriptor_set_alloc_info{};
    skybox_descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    skybox_descriptor_set_alloc_info.descriptorPool = m_ctx->descriptor_pool;
    skybox_descriptor_set_alloc_info.descriptorSetCount = 1;
    skybox_descriptor_set_alloc_info.pSetLayouts =
        &descriptor_set_layouts[_layout_type_skybox];

    VkResult res = vkAllocateDescriptorSets(
        m_ctx->device,
        &skybox_descriptor_set_alloc_info,
        &descriptor_sets[_layout_type_skybox]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate skybox set");
    }

    VkDescriptorBufferInfo mesh_per_frame_storage_buffer_info = {};
    mesh_per_frame_storage_buffer_info.offset = 0;
    mesh_per_frame_storage_buffer_info.range = sizeof(MeshPerFrameStorageBufferObject);
    mesh_per_frame_storage_buffer_info.buffer =
        m_res->global_render_resource.storage_buffer.global_upload_ringbuffer;
    assert(
        mesh_per_frame_storage_buffer_info.range <
        m_res->global_render_resource.storage_buffer.max_storage_buffer_range
    );

    VkDescriptorImageInfo specular_texture_image_info = {};
    specular_texture_image_info.sampler =
        m_res->global_render_resource.ibl_resource.specular_texture_sampler;
    specular_texture_image_info.imageView =
        m_res->global_render_resource.ibl_resource.specular_texture_image_view;
    specular_texture_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet skybox_descriptor_writes[2]{};

    skybox_descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    skybox_descriptor_writes[0].dstSet = descriptor_sets[_layout_type_skybox];
    skybox_descriptor_writes[0].dstBinding = 0;
    skybox_descriptor_writes[0].dstArrayElement = 0;
    skybox_descriptor_writes[0].descriptorType =
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    skybox_descriptor_writes[0].descriptorCount = 1;
    skybox_descriptor_writes[0].pBufferInfo = &mesh_per_frame_storage_buffer_info;

    skybox_descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    skybox_descriptor_writes[1].dstSet = descriptor_sets[_layout_type_skybox];
    skybox_descriptor_writes[1].dstBinding = 1;
    skybox_descriptor_writes[1].dstArrayElement = 0;
    skybox_descriptor_writes[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    skybox_descriptor_writes[1].descriptorCount = 1;
    skybox_descriptor_writes[1].pImageInfo = &specular_texture_image_info;

    vkUpdateDescriptorSets(
        m_ctx->device,
        ARRAY_SIZE(skybox_descriptor_writes),
        skybox_descriptor_writes,
        0,
        nullptr
    );
}

void MainPass::allocateGbufferLightingDescriptorSet() {
    VkDescriptorSetAllocateInfo gbuffer_light_global_descriptor_set_alloc_info{};
    gbuffer_light_global_descriptor_set_alloc_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    gbuffer_light_global_descriptor_set_alloc_info.descriptorPool =
        m_ctx->descriptor_pool;
    gbuffer_light_global_descriptor_set_alloc_info.descriptorSetCount = 1;
    gbuffer_light_global_descriptor_set_alloc_info.pSetLayouts =
        &descriptor_set_layouts[_layout_type_deferred_lighting];

    VkResult res = vkAllocateDescriptorSets(
        m_ctx->device,
        &gbuffer_light_global_descriptor_set_alloc_info,
        &descriptor_sets[_layout_type_deferred_lighting]
    );
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate gbuffer lighting set");
    }
}

void MainPass::clearAttachmentsAndFramebuffers() {
    for (auto &attachment : framebuffer_info.attachments) {
        vkDestroyImage(m_ctx->device, attachment.image, nullptr);
        vkDestroyImageView(m_ctx->device, attachment.view, nullptr);
        vkFreeMemory(m_ctx->device, attachment.memory, nullptr);
    }

    for (auto &framebuffer : m_swapchain_framebuffers) {
        vkDestroyFramebuffer(m_ctx->device, framebuffer, nullptr);
    }
}

void MainPass::drawMeshGbuffer(const RenderScene &scene) {
    using MeshBatch = std::unordered_map<const MeshResource *, std::vector<glm::mat4>>;

    std::unordered_map<const PBRMaterialResource *, MeshBatch>
        main_camera_mesh_drawcall_batch;

    for (const auto &node : scene.main_camera_visible_mesh_nodes) {
        auto &mesh_batch = main_camera_mesh_drawcall_batch[node.ref_material];
        auto &batch_nodes = mesh_batch[node.ref_mesh];

        batch_nodes.push_back(node.model_matrix);
    }

    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Mesh GBuffer", color);

    m_ctx->cmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelines[_pipeline_type_mesh_gbuffer]
    );

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

    m_ctx->cmdSetViewport(command_buffer, 0, 1, &viewport);
    m_ctx->cmdSetScissor(command_buffer, 0, 1, &scissor);

    uint32_t per_frame_dynamic_offset = ROUND_UP(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
        m_res->global_render_resource.storage_buffer.min_storage_buffer_offset_alignment
    );
    m_res->global_render_resource.storage_buffer
        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        per_frame_dynamic_offset + sizeof(MeshPerFrameStorageBufferObject);
    assert(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
        m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
            m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
    );
    MeshPerFrameStorageBufferObject *per_frame_storage_buffer_object =
        reinterpret_cast<MeshPerFrameStorageBufferObject *>(
            reinterpret_cast<uintptr_t>(m_res->global_render_resource.storage_buffer
                                            .global_upload_ringbuffer_memory_pointer) +
            per_frame_dynamic_offset
        );
    *per_frame_storage_buffer_object = m_res->mesh_per_frame_storage_buffer_object;

    for (auto &[material, mesh_batch] : main_camera_mesh_drawcall_batch) {
        m_ctx->cmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layouts[_pipeline_type_mesh_gbuffer],
            1,
            1,
            &material->material_descriptor_set,
            0,
            nullptr
        );

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
                    pipeline_layouts[_pipeline_type_mesh_gbuffer],
                    0,
                    1,
                    &descriptor_sets[_layout_type_mesh_global],
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
}

void MainPass::drawDeferredLighting() {
    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Deferred Lighting", color);

    m_ctx->cmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelines[_pipeline_type_deferred_lighting]
    );

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

    m_ctx->cmdSetViewport(command_buffer, 0, 1, &viewport);
    m_ctx->cmdSetScissor(command_buffer, 0, 1, &scissor);

    uint32_t per_frame_dynamic_offset = ROUND_UP(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
        m_res->global_render_resource.storage_buffer.min_storage_buffer_offset_alignment
    );
    m_res->global_render_resource.storage_buffer
        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        per_frame_dynamic_offset + sizeof(MeshPerFrameStorageBufferObject);
    assert(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
        m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
            m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
    );
    MeshPerFrameStorageBufferObject *per_frame_storage_buffer_object =
        reinterpret_cast<MeshPerFrameStorageBufferObject *>(
            reinterpret_cast<uintptr_t>(m_res->global_render_resource.storage_buffer
                                            .global_upload_ringbuffer_memory_pointer) +
            per_frame_dynamic_offset
        );
    *per_frame_storage_buffer_object = m_res->mesh_per_frame_storage_buffer_object;

    VkDescriptorSet sets[3] = {
        descriptor_sets[_layout_type_mesh_global],
        descriptor_sets[_layout_type_deferred_lighting],
        descriptor_sets[_layout_type_skybox]
    };

    uint32_t dynamic_offsets[3] = {per_frame_dynamic_offset, 0, 0};

    m_ctx->cmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layouts[_pipeline_type_deferred_lighting],
        0,
        3,
        sets,
        3,
        dynamic_offsets
    );

    m_ctx->cmdDraw(command_buffer, 3, 1, 0, 0);

    m_ctx->popEvent(command_buffer);
}

void MainPass::drawMeshLighting(const RenderScene &scene) {
    using MeshBatch = std::unordered_map<const MeshResource *, std::vector<glm::mat4>>;

    std::unordered_map<const PBRMaterialResource *, MeshBatch>
        main_camera_mesh_drawcall_batch;

    for (const auto &node : scene.main_camera_visible_mesh_nodes) {
        auto &mesh_batch = main_camera_mesh_drawcall_batch[node.ref_material];
        auto &batch_nodes = mesh_batch[node.ref_mesh];

        batch_nodes.push_back(node.model_matrix);
    }

    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Mesh Lighting", color);

    m_ctx->cmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelines[_pipeline_type_mesh_lighting]
    );

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

    m_ctx->cmdSetViewport(command_buffer, 0, 1, &viewport);
    m_ctx->cmdSetScissor(command_buffer, 0, 1, &scissor);

    uint32_t per_frame_dynamic_offset = ROUND_UP(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
        m_res->global_render_resource.storage_buffer.min_storage_buffer_offset_alignment
    );
    m_res->global_render_resource.storage_buffer
        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        per_frame_dynamic_offset + sizeof(MeshPerFrameStorageBufferObject);
    assert(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
        m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
            m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
    );
    MeshPerFrameStorageBufferObject *per_frame_storage_buffer_object =
        reinterpret_cast<MeshPerFrameStorageBufferObject *>(
            reinterpret_cast<uintptr_t>(m_res->global_render_resource.storage_buffer
                                            .global_upload_ringbuffer_memory_pointer) +
            per_frame_dynamic_offset
        );
    *per_frame_storage_buffer_object = m_res->mesh_per_frame_storage_buffer_object;

    for (auto &[material, mesh_batch] : main_camera_mesh_drawcall_batch) {
        m_ctx->cmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layouts[_pipeline_type_mesh_gbuffer],
            1,
            1,
            &material->material_descriptor_set,
            0,
            nullptr
        );

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
                    pipeline_layouts[_pipeline_type_mesh_gbuffer],
                    0,
                    1,
                    &descriptor_sets[_layout_type_mesh_global],
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
}

void MainPass::drawSkybox() {
    uint32_t per_frame_dynamic_offset = ROUND_UP(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()],
        m_res->global_render_resource.storage_buffer.min_storage_buffer_offset_alignment
    );
    m_res->global_render_resource.storage_buffer
        .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] =
        per_frame_dynamic_offset + sizeof(MeshPerFrameStorageBufferObject);
    assert(
        m_res->global_render_resource.storage_buffer
            .global_upload_ringbuffers_end[m_ctx->currentFrameIndex()] <=
        m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_begin[m_ctx->currentFrameIndex()] +
            m_res->global_render_resource.storage_buffer
                .global_upload_ringbuffers_size[m_ctx->currentFrameIndex()]
    );
    MeshPerFrameStorageBufferObject *per_frame_storage_buffer_object =
        reinterpret_cast<MeshPerFrameStorageBufferObject *>(
            reinterpret_cast<uintptr_t>(m_res->global_render_resource.storage_buffer
                                            .global_upload_ringbuffer_memory_pointer) +
            per_frame_dynamic_offset
        );
    *per_frame_storage_buffer_object = m_res->mesh_per_frame_storage_buffer_object;

    VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_ctx->pushEvent(command_buffer, "Skybox", color);

    m_ctx->cmdBindPipeline(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[_pipeline_type_skybox]
    );
    m_ctx->cmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layouts[_pipeline_type_skybox],
        0,
        1,
        &descriptor_sets[_layout_type_skybox],
        1,
        &per_frame_dynamic_offset
    );

    m_ctx->cmdDraw(command_buffer, 36, 1, 0, 0);

    m_ctx->popEvent(command_buffer);
}

}  // namespace Vain