#pragma once

#include <atomic>
#include <limits>

namespace Vain {

using GObjectID = std::size_t;

constexpr GObjectID k_invalid_go_id = std::numeric_limits<std::size_t>::max();

class ObjectIDAllocator {
  public:
    static GObjectID alloc();

  private:
    static std::atomic<GObjectID> s_next_id;
};

}  // namespace Vain