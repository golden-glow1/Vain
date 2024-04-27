#include "world_manager.h"

#include "function/global/global_context.h"
#include "function/render/render_system.h"
#include "resource/asset_manager.h"

namespace Vain {

WorldManager::~WorldManager() { clear(); }

void WorldManager::initialize() {}

void WorldManager::clear() {}

void WorldManager::addObject(const std::string &object_url) {
    auto asset_manager = g_runtime_global_context.asset_manager.get();

    GameObjectDesc gobject{};
    gobject.go_id = ObjectIDAllocator::alloc();
    asset_manager->loadAsset(object_url, gobject);

    m_gobjects[gobject.go_id] = gobject;

    auto swap_data =
        g_runtime_global_context.render_system->swap_context.getLogicSwapData();
    if (!swap_data->dirty_game_objects.has_value()) {
        swap_data->dirty_game_objects.emplace();
    }
    swap_data->dirty_game_objects->push_back(gobject);
}

void WorldManager::updateObject(GObjectID go_id) {
    auto iter = m_gobjects.find(go_id);
    if (iter == m_gobjects.end()) {
        return;
    }

    auto swap_data =
        g_runtime_global_context.render_system->swap_context.getLogicSwapData();
    if (!swap_data->dirty_game_objects.has_value()) {
        swap_data->dirty_game_objects.emplace();
    }
    swap_data->dirty_game_objects->push_back(iter->second);
}

void WorldManager::deleteObject(GObjectID go_id) {
    auto iter = m_gobjects.find(go_id);
    if (iter == m_gobjects.end()) {
        return;
    }

    auto swap_data =
        g_runtime_global_context.render_system->swap_context.getLogicSwapData();
    if (!swap_data->game_objects_to_delete.has_value()) {
        swap_data->game_objects_to_delete.emplace();
    }
    swap_data->game_objects_to_delete->push_back(iter->second);

    m_gobjects.erase(iter);
}

}  // namespace Vain