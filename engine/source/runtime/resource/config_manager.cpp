#include "config_manager.h"

#include <fstream>
#include <string>

namespace Vain {

void ConfigManager::initialize(const std::filesystem::path &config_file_path) {
    std::ifstream config_file(config_file_path);
    std::string config_line;

    while (std::getline(config_file, config_line)) {
        size_t seperate_pos = config_line.find_first_of('=');
        if (seperate_pos > 0 && seperate_pos < (config_line.length() - 1)) {
            std::string name = config_line.substr(0, seperate_pos);
            std::string value = config_line.substr(
                seperate_pos + 1, config_line.length() - seperate_pos - 1
            );
            if (name == "RootFolder") {
                m_root_folder = config_file_path.parent_path() / value;
            } else if (name == "AssetFolder") {
                m_asset_folder = m_root_folder / value;
            } else if (name == "GlobalRenderDesc") {
                m_global_render_desc_url = m_root_folder / value;
            }
        }
    }
}

}  // namespace Vain