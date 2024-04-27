#pragma once

#include <chrono>
#include <filesystem>

namespace Vain {

class VainEngine {
  public:
    void startEngine(const std::filesystem::path &config_file_path);
    void shutdownEngine();

    void run();
    void tickOneFrame(float delta_time);

    /**
     *  Each frame can only be called once
     */
    float calculateDeltaTime();

  protected:
    std::chrono::steady_clock::time_point m_last_tick_time_point{
        std::chrono::steady_clock::now()
    };
};

}  // namespace Vain