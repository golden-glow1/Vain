#pragma once

#include "core/vulkan/vulkan_context.h"
#include "function/render/render_resource.h"

namespace Vain {

enum {
    _gbuffer_a = 0,
    _gbuffer_b = 1,
    _gbuffer_c = 2,
    _backup_buffer_odd = 3,
    _backup_buffer_even = 4,
    _depth_buffer = 5,
    _swapchain_image = 6,
    _custom_attachment_count = 5,
    _attachment_count = 7,
};

enum {
    _subpass_basepass = 0,
    _subpass_deferred_lighting,
    _subpass_forward_lighting,
    _subpass_tone_mapping,
    _subpass_ui,
    _subpass_combine_ui,
    _subpass_count
};

struct RenderPassInitInfo {
    VulkanContext *ctx;
    RenderResource *res;
};

class RenderPass {
  public:
    struct Attachment {
        VkImage image{};
        VkImageView view{};
        VkDeviceMemory memory{};
        VkFormat format{};
    };

    struct FramebufferInfo {
        int width{};
        int height{};
        std::vector<Attachment> attachments{};
    };

    VkRenderPass render_pass{};

    std::vector<VkDescriptorSetLayout> descriptor_set_layouts{};
    std::vector<VkDescriptorSet> descriptor_sets{};

    std::vector<VkPipelineLayout> pipeline_layouts{};
    std::vector<VkPipeline> pipelines{};

    VkFramebuffer framebuffer{};
    FramebufferInfo framebuffer_info{};

    RenderPass() = default;
    virtual ~RenderPass();

    virtual void initialize(RenderPassInitInfo *init_info);
    virtual void clear();

  protected:
    VulkanContext *m_ctx{};
    RenderResource *m_res{};
};

}  // namespace Vain