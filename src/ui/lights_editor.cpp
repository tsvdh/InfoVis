#include "lights_editor.h"
#include <fmt/core.h>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

ui::LightEditorWidget::LightEditorWidget(const volume::Volume& volume)
    : maxExtent(volume.dims() + glm::ivec3(DIM_MARGIN))
{
    // One default light
    sceneLights.push_back(render::PointLight {glm::vec3(0.0f), glm::vec3(1.0f)});
}

void ui::LightEditorWidget::draw() {
    for (size_t lightIdx = 0; lightIdx < sceneLights.size(); lightIdx++) {
        render::PointLight &currentLight = sceneLights[lightIdx];
        
        ImGui::Text(fmt::format("Light {}", lightIdx).c_str());
        ImGui::SliderFloat3("Position",
                            glm::value_ptr(currentLight.pos),
                            -DIM_MARGIN,
                            std::max({maxExtent.x, maxExtent.y, maxExtent.z}));
        ImGui::ColorEdit3("Colour", glm::value_ptr(currentLight.val));
        if (ImGui::Button("Delete")) { sceneLights.erase(sceneLights.begin() + lightIdx); }
    }
}

void ui::LightEditorWidget::updateRenderConfig(render::RenderConfig& renderConfig) const {
    renderConfig.sceneLights.clear();
    for (const render::PointLight &currentLight : sceneLights) {
        renderConfig.sceneLights.push_back(currentLight);
    }
}
