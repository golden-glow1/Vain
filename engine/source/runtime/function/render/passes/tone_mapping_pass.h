#pragma once

#include "function/render/render_pass.h"

namespace Vain {

struct ToneMappingPassInitInfo : public RenderPassInitInfo {
    VkRenderPass render_pass;
    VkImageView input_attachment;
};

class ToneMappingPass : public RenderPass {
  public:
    ToneMappingPass() = default;
    ~ToneMappingPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;
    void draw();

    void onResize(VkImageView input_attachment);

  private:
    void createDecriptorSetLayouts();
    void createPipelines();
    void allocateDecriptorSets();
};

}  // namespace Vain