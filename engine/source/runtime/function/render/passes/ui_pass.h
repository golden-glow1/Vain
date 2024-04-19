#pragma once

#include "function/render/render_pass.h"

namespace Vain {

struct UIPassInitInfo : public RenderPassInitInfo {
    VkRenderPass render_pass;
};

class WindowUI;

class UIPass : public RenderPass {
  public:
    UIPass() = default;
    ~UIPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;
    void draw();

    void initializeUIRenderBackend(WindowUI *window_ui);

  private:
    WindowUI *m_window_ui{};
};

}  // namespace Vain