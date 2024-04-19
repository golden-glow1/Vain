#pragma once

#include <unordered_map>
#include <vector>

namespace Vain {

static inline constexpr size_t k_invalid_guid = 0;

inline bool is_valid_guid(size_t guid) { return guid != k_invalid_guid; }

template <class Asset>
class AssetGuidAllocator {
  public:
    size_t allocateGuid(const Asset &asset) {
        auto it = m_asset_to_guid_map.find(asset);
        if (it != m_asset_to_guid_map.end()) {
            return it->second;
        }

        for (size_t guid = 1; guid <= m_asset_to_guid_map.size() + 1; ++guid) {
            // find unused guid
            if (m_guid_to_asset_map.count(guid) == 0) {
                m_guid_to_asset_map.insert(std::make_pair(guid, asset));
                m_asset_to_guid_map.insert(std::make_pair(asset, guid));
                return guid;
            }
        }

        return k_invalid_guid;
    }

    bool getAsset(size_t guid, Asset &asset) {
        auto it = m_guid_to_asset_map.find(guid);
        if (it != m_guid_to_asset_map.end()) {
            asset = it->second;
            return true;
        }
        return false;
    }

    bool getGuid(const Asset &asset, size_t &guid) {
        auto it = m_asset_to_guid_map.find(asset);
        if (it != m_asset_to_guid_map.end()) {
            guid = it->second;
            return true;
        }

        return false;
    }

    bool hasAsset(const Asset &asset) const { return m_asset_to_guid_map.count(asset); }

    void freeGuid(size_t guid) {
        auto it = m_guid_to_asset_map.find(guid);
        if (it != m_guid_to_asset_map.end()) {
            const auto &asset = it->second;
            m_asset_to_guid_map.erase(asset);
            m_guid_to_asset_map.erase(it);
        }
    }

    void freeAsset(const Asset &asset) {
        auto it = m_asset_to_guid_map.find(asset);
        if (it != m_asset_to_guid_map.end()) {
            size_t guid = it->second;
            m_guid_to_asset_map.erase(guid);
            m_asset_to_guid_map.erase(it);
        }
    }

    std::vector<size_t> getAllocatedGuids() const {
        std::vector<size_t> allocated_guid{};
        for (const auto &[guid, _] : m_guid_to_asset_map) {
            allocated_guid.push_back(guid);
        }
        return allocated_guid;
    }

    void clear() {
        m_asset_to_guid_map.clear();
        m_guid_to_asset_map.clear();
    }

  private:
    std::unordered_map<Asset, size_t> m_asset_to_guid_map{};
    std::unordered_map<size_t, Asset> m_guid_to_asset_map{};
};

}  // namespace Vain