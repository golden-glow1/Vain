#include "engine.h"

#include "core/base/macro.h"
#include "function/global/global_context.h"
#include "function/render/render_system.h"
#include "function/render/window_system.h"

namespace Vain {

void VainEngine::startEngine(const std::filesystem::path &config_file_path) {
    g_runtime_global_context.startSystems(config_file_path);
    VAIN_INFO("engine start");
}

void VainEngine::shutdownEngine() {
    VAIN_INFO("engine shutdown");
    g_runtime_global_context.shutdownSystems();
}

void VainEngine::run() {
    while (!g_runtime_global_context.window_system->shouldClose()) {
        const float delta_time = calculateDeltaTime();

        tickOneFrame(delta_time);
    }
}

void VainEngine::tickOneFrame(float delta_time) {
    g_runtime_global_context.window_system->pollEvents();

    g_runtime_global_context.render_system->tick(delta_time);
}

float VainEngine::calculateDeltaTime() {
    float delta_time;
    {
        using namespace std::chrono;

        steady_clock::time_point tick_time_point = steady_clock::now();
        duration<float> time_span =
            duration_cast<duration<float>>(tick_time_point - m_last_tick_time_point);
        delta_time = time_span.count();

        m_last_tick_time_point = tick_time_point;
    }
    return delta_time;
}

}  // namespace Vain