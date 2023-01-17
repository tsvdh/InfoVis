#include "volume.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype> // isspace
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/span>
#include <iostream>
#include <string>

struct Header {
    glm::ivec3 dim;
    size_t elementSize;
};
static Header readHeader(std::ifstream& ifs);
static float computeMinimum(gsl::span<const uint16_t> data);
static float computeMaximum(gsl::span<const uint16_t> data);
static std::vector<int> computeHistogram(gsl::span<const uint16_t> data);

namespace volume {

Volume::Volume(const std::filesystem::path& file)
    : m_fileName(file.string())
{
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    loadFile(file);
    auto end = clock::now();
    std::cout << "Time to load: " << std::chrono::duration<double, std::milli>(end - start).count() << "ms" << std::endl;

    if (m_data.size() > 0) {
        m_minimum = computeMinimum(m_data);
        m_maximum = computeMaximum(m_data);
        m_histogram = computeHistogram(m_data);
    }
}

Volume::Volume(std::vector<uint16_t> data, const glm::ivec3& dim)
    : m_fileName()
    , m_elementSize(2)
    , m_dim(dim)
    , m_data(std::move(data))
    , m_minimum(computeMinimum(m_data))
    , m_maximum(computeMaximum(m_data))
    , m_histogram(computeHistogram(m_data))
{
}

float Volume::minimum() const
{
    return m_minimum;
}

float Volume::maximum() const
{
    return m_maximum;
}

std::vector<int> Volume::histogram() const
{
    return m_histogram;
}

glm::ivec3 Volume::dims() const
{
    return m_dim;
}

std::string_view Volume::fileName() const
{
    return m_fileName;
}

float Volume::getVoxel(int x, int y, int z) const
{
    const size_t i = size_t(x + m_dim.x * (y + m_dim.y * z));
    return static_cast<float>(m_data[i]);
}

// This function returns a value based on the current interpolation mode
float Volume::getSampleInterpolate(const glm::vec3& coord) const
{
    switch (interpolationMode) {
    case InterpolationMode::NearestNeighbour: {
        return getSampleNearestNeighbourInterpolation(coord);
    }
    case InterpolationMode::Linear: {
        return getSampleTriLinearInterpolation(coord);
    }
    case InterpolationMode::Cubic: {
        return getSampleTriCubicInterpolation(coord);
    }
    default: {
        throw std::exception();
    }
    }
}

// This function returns the nearest neighbour value at the continuous 3D position given by coord.
// Notice that in this framework we assume that the distance between neighbouring voxels is 1 in all directions
float Volume::getSampleNearestNeighbourInterpolation(const glm::vec3& coord) const
{
    // check if the coordinate is within volume boundaries, since we only look at direct neighbours we only need to check within 0.5
    if (glm::any(glm::lessThan(coord + 0.5f,            glm::vec3(0))) ||
        glm::any(glm::greaterThanEqual(coord + 0.5f,    glm::vec3(m_dim))))
        return 0.0f;

    // nearest neighbour simply rounds to the closest voxel positions
    auto roundToPositiveInt = [](float f) {
        // rounding is equal to adding 0.5 and cutting off the fractional part
        return static_cast<int>(f + 0.5f);
    };

    return getVoxel(roundToPositiveInt(coord.x), roundToPositiveInt(coord.y), roundToPositiveInt(coord.z));
}

// This function linearly interpolates the value at X using incoming values g0 and g1 given a factor (equal to the positon of x in 1D)
//
// g0--X--------g1
//   factor
float Volume::linearInterpolate(float g0, float g1, float factor) {
    return (g1 * factor) + (g0 * (1.0f - factor));
}

// This function bi-linearly interpolates the value at the given continuous 2D XY coordinate for a fixed integer z coordinate.
float Volume::biLinearInterpolate(const glm::vec2& xyCoord, int z) const {
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
    float bottomLeft    = getVoxel(xFloor, yFloor, zClamp);
    float bottomRight   = getVoxel(xCeilClamp, yFloor, zClamp);
    float topLeft       = getVoxel(xFloor, yCeilClamp, zClamp);
    float topRight      = getVoxel(xCeilClamp, yCeilClamp, zClamp);

    // Interpolate horizontally, then interpolate result vertically
    float horizontalInterpFactor    = xyCoord.x - static_cast<float>(xFloor);
    float verticalInterpFactor      = xyCoord.y - static_cast<float>(yFloor);
    float bottomInterp  = linearInterpolate(bottomLeft, bottomRight, horizontalInterpFactor);
    float topInterp     = linearInterpolate(topLeft, topRight, horizontalInterpFactor);
    return linearInterpolate(bottomInterp, topInterp, verticalInterpFactor);
}

// ======= TODO : IMPLEMENT the functions below for tri-linear interpolation ========
// ======= Consider using the linearInterpolate and biLinearInterpolate functions ===
// This function returns the trilinear interpolated value at the continuous 3D position given by coord.
    float Volume::getSampleTriLinearInterpolation(const glm::vec3& coord) const {
        // Check if the given coord lies within the volume's bounds
        if (glm::any(glm::lessThan(coord,           glm::vec3(0.0f))) ||
            glm::any(glm::greaterThanEqual(coord,   glm::vec3(m_dim))))
            return 0.0f;

        float depthInterpFactor = coord.z - glm::floor(coord.z);
        float nearPlaneInterp   = biLinearInterpolate({coord.x, coord.y}, static_cast<int>(glm::floor(coord.z)));
        float farPlaneInterp    = biLinearInterpolate({coord.x, coord.y}, static_cast<int>(glm::ceil(coord.z)));
        return linearInterpolate(nearPlaneInterp, farPlaneInterp, depthInterpFactor);
    }

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function represents the h(x) function, which returns the weight of the cubic interpolation kernel for a given position x
float Volume::weight(float x)
{
    // TODO: find best value
    const float a = -0.75;

    float absX = glm::abs(x);

    if (0 <= absX && absX < 1) {
        return (a + 2) * glm::pow(absX, 3) - (a + 3) * glm::pow(absX, 2) + 1;
    }
    else if (1 <= absX && absX < 2) {
        return a * glm::pow(absX, 3) - 5 * a * glm::pow(x, 2) + 8 * a * absX - 4 * a;
    }
    else {
        return 0;
    }
}

// Takes a floating point, and returns the four adjacent voxel coordinates
// A coordinate is -1 if out of bounds
std::array<int, 4> Volume::getVoxelCoors(float pos, int min, int max)
{
    std::array<int, 4> coors{};

    int floor = glm::floor(pos);
    int ceil = glm::ceil(pos);

    coors[0] = floor - 1 < min ? -1 : floor - 1;
    coors[1] = floor;
    coors[2] = ceil > max ? -1 : ceil;
    coors[3] = coors[2] == -1 ? -1 : (ceil + 1 > max ? -1 : ceil + 1);

    return coors;
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This functions returns the results of a cubic interpolation using 4 values and a factor
// g0-----g1-----g2-----g3
//         |--X--|
//      factor range
float Volume::cubicInterpolate(float g0, float g1, float g2, float g3, float factor)
{
    return
          g0 * weight(-1 - factor)
        + g1 * weight(0 - factor)
        + g2 * weight(1 - factor)
        + g3 * weight(2 - factor);
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function returns the value of a bicubic interpolation
float Volume::biCubicInterpolate(const glm::vec2& xyCoord, int z) const
{
    // Get the bounded coordinates
    auto xCoors = getVoxelCoors(xyCoord.x, 0, m_dim.x - 1);
    auto yCoors = getVoxelCoors(xyCoord.y, 0, m_dim.y - 1);

    // Calculate the factors, use previous roundings for optimization
    float horizontalFactor = xyCoord.x - static_cast<float>(xCoors[1]);
    float verticalFactor = xyCoord.y - static_cast<float>(yCoors[1]);

    float voxels[4][4];
    // Get the voxel values if coors are in bounds
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (xCoors[i] == -1 || yCoors[j] == -1) {
                voxels[i][j] = 0;
            } else {
                voxels[i][j] = getVoxel(xCoors[i], yCoors[j], z);
            }
        }
    }

    float tempInterps[4];
    // Interpolate vertically
    for (int i = 0; i < 4; i++) {
        tempInterps[i] = cubicInterpolate(voxels[i][0], voxels[i][1], voxels[i][2], voxels[i][3], verticalFactor);
    }

    // Interpolate horizontally
    return cubicInterpolate(tempInterps[0], tempInterps[1], tempInterps[2], tempInterps[3], horizontalFactor);
}

// ======= OPTIONAL : This functions can be used to implement cubic interpolation ========
// This function computes the tricubic interpolation at coord
float Volume::getSampleTriCubicInterpolation(const glm::vec3& coord) const
{
    // Check if the given coord lies within the volume's bounds
    if (glm::any(glm::lessThan(coord,           glm::vec3(0.0f))) ||
        glm::any(glm::greaterThanEqual(coord,   glm::vec3(m_dim))))
        return 0.0f;

    // Get the bounded coordinates
    auto zCoors = getVoxelCoors(coord.z, 0, m_dim.z - 1);

    // Calculate depth factor, use previous rounding for optimization
    float depthFactor = coord.z - static_cast<float>(zCoors[1]);

    float tempInterps[4];
    // Interpolate the four planes, if they are in bounds
    for (int i = 0; i < 4; i++) {
        if (zCoors[i] == -1) {
            tempInterps[i] = 0;
        } else {
            tempInterps[i] = biCubicInterpolate({coord.x, coord.y}, zCoors[i]);
        }
    }

    // Interpolate depth wise
    return cubicInterpolate(tempInterps[0], tempInterps[0], tempInterps[0], tempInterps[0], depthFactor);
}

// Load an fld volume data file
// First read and parse the header, then the volume data can be directly converted from bytes to uint16_ts
void Volume::loadFile(const std::filesystem::path& file)
{
    assert(std::filesystem::exists(file));
    std::ifstream ifs(file, std::ios::binary);
    assert(ifs.is_open());

    const auto header = readHeader(ifs);
    m_dim = header.dim;
    m_elementSize = header.elementSize;

    const size_t voxelCount = static_cast<size_t>(header.dim.x * header.dim.y * header.dim.z);
    const size_t byteCount = voxelCount * header.elementSize;
    std::vector<char> buffer(byteCount);
    // Data section is separated from header by two /f characters.
    ifs.seekg(2, std::ios::cur);
    ifs.read(buffer.data(), std::streamsize(byteCount));

    m_data.resize(voxelCount);
    if (header.elementSize == 1) { // Bytes.
        for (size_t i = 0; i < byteCount; i++) {
            m_data[i] = static_cast<uint16_t>(buffer[i] & 0xFF);
        }
    } else if (header.elementSize == 2) { // uint16_ts.
        for (size_t i = 0; i < byteCount; i += 2) {
            m_data[i / 2] = static_cast<uint16_t>((buffer[i] & 0xFF) + (buffer[i + 1] & 0xFF) * 256);
        }
    }
}
}

static Header readHeader(std::ifstream& ifs)
{
    Header out {};

    // Read input until the data section starts.
    std::string line;
    while (ifs.peek() != '\f' && !ifs.eof()) {
        std::getline(ifs, line);
        // Remove comments.
        line = line.substr(0, line.find('#'));
        // Remove any spaces from the string.
        // https://stackoverflow.com/questions/83439/remove-spaces-from-stdstring-in-c
        line.erase(std::remove_if(std::begin(line), std::end(line), ::isspace), std::end(line));
        if (line.empty())
            continue;

        const auto separator = line.find('=');
        const auto key = line.substr(0, separator);
        const auto value = line.substr(separator + 1);

        if (key == "ndim") {
            if (std::stoi(value) != 3) {
                std::cout << "Only 3D files supported\n";
            }
        } else if (key == "dim1") {
            out.dim.x = std::stoi(value);
        } else if (key == "dim2") {
            out.dim.y = std::stoi(value);
        } else if (key == "dim3") {
            out.dim.z = std::stoi(value);
        } else if (key == "nspace") {
        } else if (key == "veclen") {
            if (std::stoi(value) != 1)
                std::cerr << "Only scalar m_data are supported" << std::endl;
        } else if (key == "data") {
            if (value == "byte") {
                out.elementSize = 1;
            } else if (value == "short") {
                out.elementSize = 2;
            } else {
                std::cerr << "Data type " << value << " not recognized" << std::endl;
            }
        } else if (key == "field") {
            if (value != "uniform")
                std::cerr << "Only uniform m_data are supported" << std::endl;
        } else if (key == "#") {
            // Comment.
        } else {
            std::cerr << "Invalid AVS keyword " << key << " in file" << std::endl;
        }
    }
    return out;
}

static float computeMinimum(gsl::span<const uint16_t> data)
{
    return float(*std::min_element(std::begin(data), std::end(data)));
}

static float computeMaximum(gsl::span<const uint16_t> data)
{
    return float(*std::max_element(std::begin(data), std::end(data)));
}

static std::vector<int> computeHistogram(gsl::span<const uint16_t> data)
{
    std::vector<int> histogram(size_t(*std::max_element(std::begin(data), std::end(data)) + 1), 0);
    for (const auto v : data)
        histogram[v]++;
    return histogram;
}
