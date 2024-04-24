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

enum SwapDataType : uint8_t {
    LOGIC = 0,
    RENDER,
};

class RenderSwapContext {
  public:
    RenderSwapData *getLogicSwapData();
    RenderSwapData *getRenderSwapData();

    void swapData();

  private:
    uint8_t m_logic_swap_data_index{LOGIC};
    uint8_t m_render_swap_data_index{RENDER};
    RenderSwapData m_swap_data[2]{};
};

}  // namespace Vain