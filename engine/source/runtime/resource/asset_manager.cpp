#include "asset_manager.h"

#include "config_manager.h"
#include "function/global/global_context.h"

namespace Vain {

std::filesystem::path AssetManager::getFullPath(const std::string& relative_path) const {
    return std::filesystem::absolute(
        g_runtime_global_context.config_manager->getRootFolder() / relative_path
    );
}

}  // namespace Vain