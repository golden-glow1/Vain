#pragma once

#include <unordered_map>

#include "function/render/render_object.h"

namespace Vain {

class WorldManager {
  public:
    WorldManager() = default;
    ~WorldManager();

    void initialize();
    void clear();

    void addObject(const std::string &object_url);
    void updateObject(GObjectID go_id);
    void deleteObject(GObjectID go_id);

  private:
    std::unordered_map<GObjectID, GameObjectDesc> m_gobjects{};
};

}  // namespace Vain