#include "global_context.h"

#include "core/log/log_system.h"
#include "core/meta/auto_register.h"
#include "function/render/render_system.h"
#include "function/render/window_system.h"
#include "resource/asset_manager.h"
#include "resource/config_manager.h"

namespace Vain {

RuntimeGlobalContext g_runtime_global_context;

void RuntimeGlobalContext::startSystems(const std::filesystem::path &config_file_path) {
    log_system = std::make_unique<LogSystem>();

    config_manager = std::make_unique<ConfigManager>();
    config_manager->initialize(config_file_path);

    asset_manager = std::make_unique<AssetManager>();

    m_auto_reflection_register = std::make_unique<AutoReflectionRegister>();

    window_system = std::make_unique<WindowSystem>();
    WindowCreateInfo create_info;
    window_system->initialize(create_info);

    render_system = std::make_unique<RenderSystem>();
    render_system->initialize(window_system.get());
}

void RuntimeGlobalContext::shutdownSystems() {
    render_system.reset();
    window_system.reset();
    m_auto_reflection_register.reset();
    asset_manager.reset();
    config_manager.reset();
    log_system.reset();
}

}  // namespace Vain