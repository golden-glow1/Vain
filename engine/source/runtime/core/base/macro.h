#pragma once

#pragma once

#include "core/log/log_system.h"
#include "function/global/global_context.h"

// clang-format off
#define VAIN_LOG(LOG_LEVEL, ...)                                        \
    Vain::g_runtime_global_context.log_system->log(                     \
        LOG_LEVEL, "[" + std::string(__FUNCTION__) + "] " + __VA_ARGS__ \
    );

#define VAIN_DEBUG(...) VAIN_LOG(Vain::LogSystem::LogLevel::debug, __VA_ARGS__)
#define VAIN_INFO(...)  VAIN_LOG(Vain::LogSystem::LogLevel::info, __VA_ARGS__)
#define VAIN_WARN(...)  VAIN_LOG(Vain::LogSystem::LogLevel::warn, __VA_ARGS__)
#define VAIN_ERROR(...) VAIN_LOG(Vain::LogSystem::LogLevel::error, __VA_ARGS__)
#define VAIN_FATAL(...) VAIN_LOG(Vain::LogSystem::LogLevel::fatal, __VA_ARGS__)
// clang-format on

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define ROUND_UP(value, alignment) (((value) + (alignment)-1) / (alignment) * (alignment))