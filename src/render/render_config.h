#pragma once
#include <array>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <cstring> // memcmp  // macOS change TH
#include <vector>


namespace render {

struct TF2DTriangle {
    glm::vec2 intensityBase;
    float magnitudeHeight;
    float radius;
    glm::vec4 color;

    bool operator==(const TF2DTriangle& other) const;
};

inline bool TF2DTriangle::operator==(const TF2DTriangle& other) const {
        return this->intensityBase == other.intensityBase
           && this->magnitudeHeight == other.magnitudeHeight
           && this->radius == other.radius
           && this->color == other.color;
}

enum class RenderMode {
    RenderSlicer,
    RenderMIP,
    RenderIso,
    RenderComposite,
    RenderTF2D
};

struct RenderConfig {
    RenderMode renderMode { RenderMode::RenderSlicer };
    glm::ivec2 renderResolution;

    bool volumeShading { false };
    float isoValue { 95.0f };

    // 1D transfer function.
    std::array<glm::vec4, 256> tfColorMap;
    // Used to convert from a value to an index in the color map.
    // index = (value - start) / range * tfColorMap.size();
    float tfColorMapIndexStart;
    float tfColorMapIndexRange;

    // 2D transfer function.
    std::vector<TF2DTriangle> TF2DTriangles;

//    float TF2DIntensity;
//    float TF2DRadius;
//    glm::vec4 TF2DColor;


};

// NOTE(Mathijs): should be replaced by C++20 three-way operator (aka spaceship operator) if we require C++ 20 support from Linux users (GCC10 / Clang10).
inline bool operator==(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return lhs.renderMode == rhs.renderMode
        && lhs.renderResolution == rhs.renderResolution
        && lhs.volumeShading == rhs.volumeShading
        && lhs.isoValue == rhs.isoValue
        && lhs.tfColorMap == rhs.tfColorMap
        && lhs.tfColorMapIndexStart == rhs.tfColorMapIndexStart
        && lhs.tfColorMapIndexRange == rhs.tfColorMapIndexRange
        && lhs.TF2DTriangles == rhs.TF2DTriangles;
}
inline bool operator!=(const RenderConfig& lhs, const RenderConfig& rhs)
{
    return !(lhs == rhs);
}

}