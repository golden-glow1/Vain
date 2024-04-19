#pragma once

namespace Vain {

class WindowSystem;
class RenderSystem;

struct WindowUIInitInfo {
    RenderSystem *render_system;
    WindowSystem *window_system;
};

class WindowUI {
  public:
    virtual ~WindowUI() {}

    virtual void initialize(WindowUIInitInfo init_info) = 0;
    virtual void preRender() = 0;
};

}  // namespace Vain