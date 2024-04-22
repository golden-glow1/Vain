#version 460

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput in_scene_color;

layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput in_ui_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 scene_color = subpassLoad(in_scene_color).rgba;
    
    vec4 ui_color = subpassLoad(in_ui_color).rgba;
    
    if(ui_color.r < 1e-6 && ui_color.g < 1e-6 && ui_color.a < 1e-6) {
        out_color = scene_color;
    } else {
        out_color = ui_color;
    }
}
