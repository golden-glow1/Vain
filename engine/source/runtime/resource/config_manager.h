#pragma once

#include <filesystem>

namespace Vain {

class ConfigManager {
  public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    void initialize(const std::filesystem::path &config_file_path);

    const std::filesystem::path &getRootFolder() const { return m_root_folder; }
    const std::filesystem::path &getAssetFolder() const { return m_asset_folder; }
    const std::filesystem::path &getGlobalRenderDescUrl() const {
        return m_global_render_desc_url;
    }

  private:
    std::filesystem::path m_root_folder{};
    std::filesystem::path m_asset_folder{};
    std::filesystem::path m_global_render_desc_url{};
};

}  // namespace Vain