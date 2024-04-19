#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/base/macro.h"
#include "core/serializer/serializer.h"

namespace Vain {

class AssetManager {
  public:
    template <typename AssetType>
    bool loadAsset(const std::string &asset_url, AssetType &in_asset) const {
        std::filesystem::path asset_path = getFullPath(asset_url);
        std::ifstream asset_json_file(asset_path);
        if (!asset_json_file) {
            VAIN_ERROR("open file: {} failed!", asset_path.generic_string());
            return false;
        }

        std::stringstream buffer;
        buffer << asset_json_file.rdbuf();
        std::string asset_json_text{buffer.str()};

        if (!Serializer::read(in_asset, asset_json_text)) {
            VAIN_ERROR("parse json file {} failed!", asset_url);
            return false;
        }

        return true;
    }

    template <typename AssetType>
    bool saveAsset(const std::string &asset_url, const AssetType &out_asset) const {
        std::ofstream asset_json_file(getFullPath(asset_url));
        if (!asset_json_file) {
            VAIN_ERROR("open file {} failed!", asset_url);
            return false;
        }

        std::string asset_json_text = Serializer::write(out_asset);

        asset_json_file << asset_json_text;
        asset_json_file.flush();

        return true;
    }

    std::filesystem::path getFullPath(const std::string &relative_path) const;
};

}  // namespace Vain