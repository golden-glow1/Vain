#pragma once

#include "function/render/render_pass.h"
#include "function/render/render_type.h"

namespace Vain {

class RenderScene;

class DirectionalLightPass : public RenderPass {
  public:
    DirectionalLightPass() = default;
    ~DirectionalLightPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;

    void preparePassData();
    void draw(const RenderScene &scene);

  private:
    DirectionalLightShadowPerFrameStorageBufferObject
        m_directional_light_shadow_per_frame_storage_buffer_object;

    void createAttachments();
    void createRenderPass();
    void createDescriptorSetLayouts();
    void createPipelines();
    void allocateDescriptorSets();
    void createFramebuffer();
};

}  // namespace Vain