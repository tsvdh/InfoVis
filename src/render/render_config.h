#pragma once
#include "point_light.h"
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <cstring> // memcmp  // macOS change TH

namespace render {

constexpr size_t MAX_LIGHTS = 25;

enum class RenderMode {
    RenderSlicer,
    RenderMIP,
    RenderIso,
    RenderComposite,
    RenderTF2D
};

enum class ShadingMode {
    ShadingNone,
    ShadingPhong,
    ShadingGooch
};

struct RenderConfig {
    RenderMode renderMode { RenderMode::RenderSlicer };
    ShadingMode shadingMode { ShadingMode::ShadingNone };
    glm::ivec2 renderResolution;

    // Lighting
    bool volumeShading { false };
    bool includeCameraLight { true };
    std::array<const PointLight*, MAX_LIGHTS> sceneLights;
    size_t numLights { 0U };

    // Gooch shading
    float blueCoeff { 0.4f };
    float yellowCoeff { 0.4f };
    float coolDiffuseCoeff { 0.2f };
    float warmDiffuseCoeff { 0.6f };
    float edgeClassificationThreshold { 0.85f };

    // Edge detection
    bool edgeDetection { false };
    float edgeThreshold { 1.0f };

    // ISO rendering
    float isoValue { 95.0f };
    glm::vec3 isoColor { 0.8f, 0.8f, 0.2f };

    // 1D transfer function.
    std::array<glm::vec4, 256> tfColorMap;
    // Used to convert from a value to an index in the color map.
    // index = (value - start) / range * tfColorMap.size();
    float tfColorMapIndexStart;
    float tfColorMapIndexRange;

    // 2D transfer function.
    float TF2DIntensity;
    float TF2DRadius;
    glm::vec4 TF2DColor;
};

// NOTE(Mathijs): should be replaced by C++20 three-way operator (aka spaceship operator) if we require C++ 20 support from Linux users (GCC10 / Clang10).
inline bool operator==(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(RenderConfig)) == 0;
}
inline bool operator!=(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return !(lhs == rhs);
}

}