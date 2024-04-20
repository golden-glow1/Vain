#pragma once

#include "function/render/passes/combine_ui_pass.h"
#include "function/render/passes/tone_mapping_pass.h"
#include "function/render/passes/ui_pass.h"
#include "function/render/render_pass.h"

namespace Vain {

class RenderScene;

struct MainPassInitInfo : public RenderPassInitInfo {
    VkImageView point_light_shadow_color_image_view{};
    VkImageView directional_light_shadow_color_image_view{};
};

class MainPass : public RenderPass {
  public:
    enum LayoutType : uint8_t {
        _layout_type_mesh_global = 0,
        _layout_type_mesh_per_material,
        _layout_type_skybox,
        _layout_type_deferred_lighting,
        _layout_type_count
    };

    enum RenderPipeLineType : uint8_t {
        _pipeline_type_mesh_gbuffer = 0,
        _pipeline_type_deferred_lighting,
        _pipeline_type_mesh_lighting,
        _pipeline_type_skybox,
        _pipeline_type_count
    };

    MainPass() = default;
    ~MainPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;

    void draw(
        const RenderScene &scene,
        ToneMappingPass &tone_mapping_pass,
        UIPass &ui_pass,
        CombineUIPass &combine_ui_pass
    );

    void drawForward(
        const RenderScene &scene,
        ToneMappingPass &tone_mapping_pass,
        UIPass &ui_pass,
        CombineUIPass &combine_ui_pass
    );

    void onResize();

  private:
    VkImageView m_point_light_shadow_color_image_view{};
    VkImageView m_directional_light_shadow_color_image_view{};
    std::vector<VkFramebuffer> m_swapchain_framebuffers{};

    void createAttachments();
    void createRenderPass();
    void createDescriptorSetLayouts();
    void createPipelines();
    void allocateDecriptorSets();
    void updateFramebufferDescriptors();
    void createSwapchainFramebuffers();

    void allocateMeshGlobalDescriptorSet();
    void allocateSkyBoxDescriptorSet();
    void allocateGbufferLightingDescriptorSet();

    void clearAttachmentsAndFramebuffers();

    void drawMeshGbuffer(const RenderScene &scene);
    void drawDeferredLighting();
    void drawMeshLighting(const RenderScene &scene);
    void drawSkybox();
};

}  // namespace Vain