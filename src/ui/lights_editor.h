#ifndef _LIGHTS_EDITOR_H_
#define _LIGHTS_EDITOR_H_
#include "render/point_light.h"
#include "render/render_config.h"
#include "volume/volume.h"
#include <GL/glew.h> // Include before glfw3
#include <glm/vec3.hpp>
#include <vector>

namespace ui {

constexpr float DIM_MARGIN = 50;

class LightEditorWidget {
public:
    LightEditorWidget(const volume::Volume& volume);

    void draw();
    void updateRenderConfig(render::RenderConfig& renderConfig) const;

private:
    std::vector<render::PointLight> sceneLights;
    glm::vec3 maxExtent;
};
}

#endif
