#pragma once

#include <deque>
#include <optional>

#include "function/render/render_object.h"
#include "resource/asset_type.h"

namespace Vain {

struct RenderSwapData {
    std::optional<IBLDesc> ibl_swap_data{};
    std::optional<std::deque<GameObjectDesc>> dirty_game_objects{};
    std::optional<std::deque<GameObjectDesc>> game_objects_to_delete{};
};

}  // namespace Vain