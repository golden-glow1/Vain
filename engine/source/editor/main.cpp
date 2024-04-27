#include <engine.h>

#include "editor.h"

int main(int argc, char **argv) {
    auto engine = new Vain::VainEngine;
    auto editor = new Vain::VainEditor;

    std::filesystem::path executable_path(argv[0]);
    std::filesystem::path config_file_path =
        executable_path.parent_path() / "VainEditor.ini";

    engine->startEngine(config_file_path);
    editor->initialize(engine);

    editor->run();

    editor->clear();
    engine->shutdownEngine();

    delete engine;
    delete editor;

    return 0;
}