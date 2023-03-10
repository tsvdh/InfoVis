#include "gradient_volume.h"
#include <algorithm>
#include <exception>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>
#include <gsl/span>

namespace volume {

// Compute the maximum magnitude from all gradient voxels
static float computeMaxMagnitude(gsl::span<const GradientVoxel> data)
{
    return std::max_element(
        std::begin(data),
        std::end(data),
        [](const GradientVoxel& lhs, const GradientVoxel& rhs) {
            return lhs.magnitude < rhs.magnitude;
        })
        ->magnitude;
}

// Compute the minimum magnitude from all gradient voxels
static float computeMinMagnitude(gsl::span<const GradientVoxel> data)
{
    return std::min_element(
        std::begin(data),
        std::end(data),
        [](const GradientVoxel& lhs, const GradientVoxel& rhs) {
            return lhs.magnitude < rhs.magnitude;
        })
        ->magnitude;
}

// Compute a gradient volume from a volume
static std::vector<GradientVoxel> computeGradientVolume(const Volume& volume)
{
    const auto dim = volume.dims();

    std::vector<GradientVoxel> out(static_cast<size_t>(dim.x * dim.y * dim.z));
    for (int z = 1; z < dim.z - 1; z++) {
        for (int y = 1; y < dim.y - 1; y++) {
            for (int x = 1; x < dim.x - 1; x++) {
                const float gx = (volume.getVoxel(x + 1, y, z) - volume.getVoxel(x - 1, y, z)) / 2.0f;
                const float gy = (volume.getVoxel(x, y + 1, z) - volume.getVoxel(x, y - 1, z)) / 2.0f;
                const float gz = (volume.getVoxel(x, y, z + 1) - volume.getVoxel(x, y, z - 1)) / 2.0f;

                const glm::vec3 v { gx, gy, gz };
                const size_t index = static_cast<size_t>(x + dim.x * (y + dim.y * z));
                out[index] = GradientVoxel { v, glm::length(v) };
            }
        }
    }
    return out;
}

GradientVolume::GradientVolume(const Volume& volume)
    : m_dim(volume.dims())
    , m_data(computeGradientVolume(volume))
    , m_minMagnitude(computeMinMagnitude(m_data))
    , m_maxMagnitude(computeMaxMagnitude(m_data))
{
}

float GradientVolume::maxMagnitude() const
{
    return m_maxMagnitude;
}

float GradientVolume::minMagnitude() const
{
    return m_minMagnitude;
}

glm::ivec3 GradientVolume::dims() const
{
    return m_dim;
}

// This function returns a gradientVoxel at coord based on the current interpolation mode.
GradientVoxel GradientVolume::getGradientInterpolate(const glm::vec3& coord) const
{
    switch (interpolationMode) {
    case InterpolationMode::NearestNeighbour: {
        return getGradientNearestNeighbor(coord);
    }
    case InterpolationMode::Linear: {
        return getGradientLinearInterpolate(coord);
    }
    case InterpolationMode::Cubic: {
        // No cubic in this case, linear is good enough for the gradient.
        return getGradientLinearInterpolate(coord);
    }
    default: {
        throw std::exception();
    }
    };
}

// This function returns the nearest neighbour given a position in the volume given by coord.
// Notice that in this framework we assume that the distance between neighbouring voxels is 1 in all directions
GradientVoxel GradientVolume::getGradientNearestNeighbor(const glm::vec3& coord) const
{
    if (glm::any(glm::lessThan(coord, glm::vec3(0))) || glm::any(glm::greaterThanEqual(coord, glm::vec3(m_dim))))
        return { glm::vec3(0.0f), 0.0f };

    auto roundToPositiveInt = [](float f) {
        return static_cast<int>(f + 0.5f);
    };

    return getGradient(roundToPositiveInt(coord.x), roundToPositiveInt(coord.y), roundToPositiveInt(coord.z));
}

// ======= TODO : IMPLEMENT ========
// Returns the trilinearly interpolated gradinet at the given coordinate.
// Use the linearInterpolate function that you implemented below.
GradientVoxel GradientVolume::getGradientLinearInterpolate(const glm::vec3& coord) const {
    // Check if the given coord lies within the volume's bounds
    if (glm::any(glm::lessThan(coord,           glm::vec3(0.0f))) ||
        glm::any(glm::greaterThanEqual(coord,   glm::vec3(m_dim)))) { return { glm::vec3(0), 0.0f }; }

    float depthInterpFactor         = coord.z - glm::floor(coord.z);
    GradientVoxel nearPlaneInterp   = biLinearInterpolate({coord.x, coord.y}, static_cast<int>(glm::floor(coord.z)));
    GradientVoxel farPlaneInterp    = biLinearInterpolate({coord.x, coord.y}, static_cast<int>(glm::ceil(coord.z)));
    return linearInterpolate(nearPlaneInterp, farPlaneInterp, depthInterpFactor);
}

GradientVoxel GradientVolume::biLinearInterpolate(const glm::vec2 &xyCoord, int z) const {
    // Precompute floor calls
    int xFloor = static_cast<int>(glm::floor(xyCoord.x));
    int yFloor = static_cast<int>(glm::floor(xyCoord.y));

    // Clamp outputs of ceil calls since they might result in an out of bounds number
    int xCeilClamp  = static_cast<int>(glm::ceil(xyCoord.x));
    xCeilClamp      = xCeilClamp >= m_dim.x ? m_dim.x - 1 : xCeilClamp;
    int yCeilClamp  = static_cast<int>(glm::ceil(xyCoord.y));
    yCeilClamp      = yCeilClamp >= m_dim.y ? m_dim.y - 1 : yCeilClamp;
    int zClamp      = z >= m_dim.z ? m_dim.z - 1 : z;

    // Get 4 nearest neighbours
    const GradientVoxel &bottomLeft    = getGradient(xFloor, yFloor, zClamp);
    const GradientVoxel &bottomRight   = getGradient(xCeilClamp, yFloor, zClamp);
    const GradientVoxel &topLeft       = getGradient(xFloor, yCeilClamp, zClamp);
    const GradientVoxel &topRight      = getGradient(xCeilClamp, yCeilClamp, zClamp);

    // Interpolate horizontally, then interpolate result vertically
    float horizontalInterpFactor    = xyCoord.x - static_cast<float>(xFloor);
    float verticalInterpFactor      = xyCoord.y - static_cast<float>(yFloor);
    GradientVoxel bottomInterp  = linearInterpolate(bottomLeft, bottomRight, horizontalInterpFactor);
    GradientVoxel topInterp     = linearInterpolate(topLeft, topRight, horizontalInterpFactor);
    return linearInterpolate(bottomInterp, topInterp, verticalInterpFactor);
}

// ======= TODO : IMPLEMENT ========
// This function should linearly interpolates the value from g0 to g1 given the factor (t).
// At t=0, linearInterpolate should return g0 and at t=1 it returns g1.
GradientVoxel GradientVolume::linearInterpolate(const GradientVoxel& g0, const GradientVoxel& g1, float factor) {
    return { glm::mix(g0.dir, g1.dir, factor),
             (g1.magnitude * factor) + (g0.magnitude * (1.0f - factor)) };
}

// This function returns a gradientVoxel without using interpolation
GradientVoxel GradientVolume::getGradient(int x, int y, int z) const
{
    const size_t i = static_cast<size_t>(x + m_dim.x * (y + m_dim.y * z));
    return m_data[i];
}
}