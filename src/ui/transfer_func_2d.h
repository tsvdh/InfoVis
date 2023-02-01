#pragma once
#include "render/render_config.h"
#include "volume/gradient_volume.h"
#include "volume/volume.h"
#include <GL/glew.h> // Include before glfw3
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace ui {

struct TF2DTriangle {
    // base, left, right
    std::array<glm::vec2, 3> points;
    glm::vec4 color;
};

class TransferFunction2DWidget {
public:
    TransferFunction2DWidget(const volume::Volume& volume, const volume::GradientVolume& gradient);

    void draw();
    void updateRenderConfig(render::RenderConfig& renderConfig);

private:
    glm::vec2 normPoint(glm::vec2& point) const;

private:
    std::vector<TF2DTriangle> m_triangles;

    float m_maxIntensity, m_maxMagnitude;
//    float m_radius;
//    glm::vec4 m_color;

    std::optional<std::tuple<TF2DTriangle&, int>> m_interactingTriangle;

    TF2DTriangle m_selectedTriangle;

    GLuint m_histogramImg;

};
}
