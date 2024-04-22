#pragma once

namespace Vain {

class RenderCamera;

class EditorSceneManager {
  public:
    EditorSceneManager() = default;
    ~EditorSceneManager();

    void initialize();
    void clear();
};

}