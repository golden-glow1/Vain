#pragma once

#include <vector>

#include "core/math/transform.h"
#include "function/framework/object_id.h"
#include "function/render/render_scene.h"
#include "resource/asset_type.h"

struct aiNode;
struct aiScene;

namespace Vain {

class RenderScene;
class RenderResource;

struct GameObjectNode {
    glm::mat4 original_model{};
    std::vector<std::shared_ptr<RenderEntity>> entities{};
    std::vector<std::shared_ptr<GameObjectNode>> children{};

    static std::shared_ptr<GameObjectNode> load(
        const aiNode *node,
        const aiScene *scene,
        const std::string &file,
        RenderScene &render_scene,
        RenderResource &render_resource,
        glm::mat4 parent_model
    );

    void clone(const std::shared_ptr<GameObjectNode> &node);

    void updateTransform(glm::mat4 transform);
};

class GameObject {
  public:
    std::string url{};
    GObjectID go_id{k_invalid_go_id};
    std::shared_ptr<GameObjectNode> root_node{};

    void load(RenderScene &render_scene, RenderResource &render_resource);

    void clone(const GameObject &gobject);

    Transform getTransform() const { return m_transform; }
    void updateTransform(Transform transform);

    bool loaded() const { return m_loaded; }

  private:
    bool m_loaded{};
    Transform m_transform{};
};

}  // namespace Vain