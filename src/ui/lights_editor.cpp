#include "lights_editor.h"
#include <fmt/core.h>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

ui::LightEditorWidget::LightEditorWidget(const volume::Volume& volume)
    : maxExtent(volume.dims())
    , selectedLight(0)
    , includeCameraLight(true)
{
    // One default light
    sceneLights.push_back(render::PointLight {glm::vec3(0.0f), glm::vec3(1.0f)});
}

void ui::LightEditorWidget::draw() {
    // Enable / disable controls
    ImGui::Checkbox("Include Camera Light", &includeCameraLight);
    ImGui::NewLine();

    // Add / remove controls
    if (ImGui::Button("Add")) { 
        if (sceneLights.size() < render::MAX_LIGHTS) {sceneLights.push_back(render::PointLight {glm::vec3(0.0f), glm::vec3(1.0f)});}
    }
    if (ImGui::Button("Remove selected")) { 
        if (selectedLight < sceneLights.size()) {
            std::vector<render::PointLight>::iterator iterator = sceneLights.begin();
            std::advance(iterator, selectedLight);
            sceneLights.erase(iterator);
            selectedLight = 0U;
        }
    }
    ImGui::NewLine();

    // Selection controls
    std::vector<std::string> options;
    for (size_t lightIdx = 0; lightIdx < sceneLights.size(); lightIdx++) { options.push_back("Light " + std::to_string(lightIdx + 1)); }
    std::vector<const char*> optionsPointers;
    std::transform(std::begin(options), std::end(options), std::back_inserter(optionsPointers),
        [](const auto& str) { return str.c_str(); });
    ImGui::Combo("Selected light", &selectedLight, optionsPointers.data(), static_cast<int>(optionsPointers.size()));

    // Selected light controls
    if (sceneLights.size() > 0) {
        ImGui::SliderFloat3("Position",
                          glm::value_ptr(sceneLights[selectedLight].pos),
                          0.0f,
                          std::max({maxExtent.x, maxExtent.y, maxExtent.z}));
        ImGui::ColorEdit3("Color", glm::value_ptr(sceneLights[selectedLight].val));
    }
}

void ui::LightEditorWidget::updateRenderConfig(render::RenderConfig& renderConfig) const {
    // Toggle(s)
    renderConfig.includeCameraLight = includeCameraLight;
    
    // Light source management
    if (renderConfig.numLights != sceneLights.size()) {
        for (size_t lightIdx = 0; lightIdx < sceneLights.size(); lightIdx++) {
            const render::PointLight* lightPtr  = sceneLights.data() + lightIdx;
            renderConfig.sceneLights[lightIdx]  = lightPtr;
        }
        renderConfig.numLights = sceneLights.size();
    }   
}
