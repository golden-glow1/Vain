#include "render_swap_context.h"

namespace Vain {

RenderSwapData *RenderSwapContext::getLogicSwapData() {
    return &m_swap_data[m_logic_swap_data_index];
}

RenderSwapData *RenderSwapContext::getRenderSwapData() {
    return &m_swap_data[m_render_swap_data_index];
}

void RenderSwapContext::swapData() {
    auto render_swap_data = getRenderSwapData();

    bool not_ready = render_swap_data->ibl_swap_data.has_value() ||
                     render_swap_data->dirty_game_objects.has_value() ||
                     render_swap_data->game_objects_to_delete.has_value();
    if (not_ready) {
        return;
    }

    std::swap(m_logic_swap_data_index, m_render_swap_data_index);
}

}  // namespace Vain