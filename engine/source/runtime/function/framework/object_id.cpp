#include "object_id.h"

#include "core/base/macro.h"

namespace Vain {

std::atomic<GObjectID> ObjectIDAllocator::s_next_id{0};

GObjectID ObjectIDAllocator::alloc() {
    std::atomic<GObjectID> new_id = s_next_id.fetch_add(1);

    if (s_next_id >= k_invalid_go_id) {
        VAIN_FATAL("game object id overflow");
    }

    return new_id;
}

}  // namespace Vain