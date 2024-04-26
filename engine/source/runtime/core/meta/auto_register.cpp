#include "auto_register.h"

#include <glm/gtc/quaternion.hpp>
#include <rttr/registration>

#include "core/math/aabb.h"
#include "core/math/transform.h"
#include "function/render/render_object.h"
#include "resource/asset_type.h"

namespace Vain {

AutoReflectionRegister::AutoReflectionRegister() {
    using namespace rttr;

    // glm
    registration::class_<glm::vec2>("vec2")
        .property("x", &glm::vec2::x)
        .property("y", &glm::vec2::y);
    registration::class_<glm::vec3>("vec3")
        .property("x", &glm::vec3::x)
        .property("y", &glm::vec3::y)
        .property("z", &glm::vec3::z);
    registration::class_<glm::vec4>("vec4")
        .property("x", &glm::vec4::x)
        .property("y", &glm::vec4::y)
        .property("z", &glm::vec4::z)
        .property("w", &glm::vec4::w);
    registration::class_<glm::quat>("quat")
        .property("x", &glm::quat::x)
        .property("y", &glm::quat::y)
        .property("z", &glm::quat::z)
        .property("w", &glm::quat::w);

    // asset type
    registration::class_<Vain::Color>("Color")
        .property("r", &Vain::Color::r)
        .property("g", &Vain::Color::g)
        .property("b", &Vain::Color::b);
    registration::class_<Vain::SkyBoxDesc>("SkyBoxDesc")
        .property("positive_x_map", &Vain::SkyBoxDesc::positive_x_map)
        .property("negative_x_map", &Vain::SkyBoxDesc::negative_x_map)
        .property("positive_y_map", &Vain::SkyBoxDesc::positive_y_map)
        .property("negative_y_map", &Vain::SkyBoxDesc::negative_y_map)
        .property("positive_z_map", &Vain::SkyBoxDesc::positive_z_map)
        .property("negative_z_map", &Vain::SkyBoxDesc::negative_z_map);
    registration::class_<Vain::DirectionalLightDesc>("DirectionalLightDesc")
        .property("direction", &Vain::DirectionalLightDesc::direction)
        .property("color", &Vain::DirectionalLightDesc::color);
    registration::class_<CameraPose>("CameraPose")
        .property("position", &Vain::CameraPose::position)
        .property("target", &Vain::CameraPose::target)
        .property("up", &Vain::CameraPose::up);
    registration::class_<Vain::CameraConfig>("CameraConfig")
        .property("pose", &Vain::CameraConfig::pose)
        .property("z_far", &Vain::CameraConfig::z_far)
        .property("z_near", &Vain::CameraConfig::z_near)
        .property("aspect", &Vain::CameraConfig::aspect);
    registration::class_<Vain::SceneGlobalDesc>("SceneGlobalDesc")
        .property("skybox_irradiance_map", &Vain::SceneGlobalDesc::skybox_irradiance_map)
        .property("skybox_specular_map", &Vain::SceneGlobalDesc::skybox_specular_map)
        .property("brdf_map", &Vain::SceneGlobalDesc::brdf_map)
        .property("sky_color", &Vain::SceneGlobalDesc::sky_color)
        .property("ambient_light", &Vain::SceneGlobalDesc::ambient_light)
        .property("camera_config", &Vain::SceneGlobalDesc::camera_config)
        .property("directional_light", &Vain::SceneGlobalDesc::directional_light);

    // math
    registration::class_<AxisAlignedBoundingBox>("AxisAlignedBoundingBox")
        .property("center", &AxisAlignedBoundingBox::center)
        .property("half_extent", &AxisAlignedBoundingBox::half_extent);
    registration::class_<Transform>("Transform")
        .property("translation", &Transform::translation)
        .property("scale", &Transform::scale)
        .property("rotation", &Transform::rotation);

    // game object
    registration::class_<GameObjectMeshDesc>("GameObjectMeshDesc")
        .property("mesh_file", &GameObjectMeshDesc::mesh_file);
    registration::class_<GameObjectMaterialDesc>("GameObjectMaterialDesc")
        .property(
            "base_color_texture_file", &GameObjectMaterialDesc::base_color_texture_file
        )
        .property("normal_texture_file", &GameObjectMaterialDesc::normal_texture_file)
        .property("metallic_texture_file", &GameObjectMaterialDesc::metallic_texture_file)
        .property(
            "roughness_texture_file", &GameObjectMaterialDesc::roughness_texture_file
        )
        .property(
            "occlusion_texture_file", &GameObjectMaterialDesc::occlusion_texture_file
        )
        .property(
            "emissive_texture_file", &GameObjectMaterialDesc::emissive_texture_file
        );
    registration::class_<GameObjectPartDesc>("GameObjectPartDesc")
        .property("mesh_desc", &GameObjectPartDesc::mesh_desc)
        .property("material_desc", &GameObjectPartDesc::material_desc)
        .property("transform", &GameObjectPartDesc::transform);
}

}  // namespace Vain