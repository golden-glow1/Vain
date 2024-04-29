#pragma once

#include <filesystem>
#include <memory>

namespace Vain {

class LogSystem;
class RenderSystem;
class WindowSystem;
class ConfigManager;
class AssetManager;
class AutoReflectionRegister;

class RuntimeGlobalContext {
  public:
    void startSystems(const std::filesystem::path &config_file_path);
    void shutdownSystems();

    std::unique_ptr<LogSystem> log_system{};
    std::unique_ptr<ConfigManager> config_manager{};
    std::unique_ptr<AssetManager> asset_manager{};
    std::unique_ptr<WindowSystem> window_system{};
    std::unique_ptr<RenderSystem> render_system{};

  private:
    std::unique_ptr<AutoReflectionRegister> m_auto_reflection_register{};
};

extern RuntimeGlobalContext g_runtime_global_context;

}  // namespace Vain