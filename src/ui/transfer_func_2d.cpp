#include "transfer_func_2d.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <imgui.h>
#include <iostream>
#include <vector>

static ImVec2 glmToIm(const glm::vec2& v);
static glm::vec2 ImToGlm(const ImVec2& v);
static std::vector<glm::vec4> createHistogramImage(
    const volume::Volume& volume, const volume::GradientVolume& gradient, const glm::ivec2& res);

namespace ui {

// Radius of the three points in the histogram image.
static constexpr float pointRadius = 8.0f;
static constexpr glm::ivec2 widgetSize { 475, 300 };

TransferFunction2DWidget::TransferFunction2DWidget(const volume::Volume& volume, const volume::GradientVolume& gradient)
    : m_triangles()
    , m_maxIntensity(volume.maximum())
    , m_maxMagnitude(gradient.maxMagnitude())
    , m_interactingTriangle(std::nullopt)
    , m_selectedTriangle(0)
    , m_histogramImg(0)
{
    m_triangles.push_back({{glm::vec2(90.0f, 30.0f), {60.0f, 70.0f}, {120.0f, 70.0f}}, {0.0f, 0.8f, 0.6f, 0.3f}});

    const glm::ivec2 res = glm::ivec2(volume.maximum(), gradient.maxMagnitude() + 1);
    const auto imgData = createHistogramImage(volume, gradient, res);

    glGenTextures(1, &m_histogramImg);
    glBindTexture(GL_TEXTURE_2D, m_histogramImg);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, m_histogramImg);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.x, res.y, 0, GL_RGBA, GL_FLOAT, imgData.data());
}

// Draw the widget and handle interactions
void TransferFunction2DWidget::draw()
{
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::Text("2D Transfer Function");
    ImGui::Text("Click and drag points to alter the shape of a triangle, or left click\nto add a new triangle. Right click on a triangle to remove it.");

    // Histogram image is positioned to the right of the content region.
    const glm::vec2 canvasSize { widgetSize.x, widgetSize.y - 20 };
    glm::vec2 canvasPos = ImToGlm(ImGui::GetCursorScreenPos()); // this is the imgui draw cursor, not mouse cursor
    const float xOffset = (ImToGlm(ImGui::GetContentRegionAvail()).x - canvasSize.x);
    canvasPos.x += xOffset; // center widget

    // Draw side text (imgui cannot center-align text so we have to do it ourselves).
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursorPos.x + 25, cursorPos.y + canvasSize.y / 2 - 20 - ImGui::GetFontSize()));
    //ImGui::Text("");
    ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(cursorPos.x + 15, cursorPos.y + canvasSize.y / 2 - 20));
    ImGui::Text("Gradient");
    ImGui::SetCursorPos(ImVec2(cursorPos.x + 5, cursorPos.y + canvasSize.y / 2 - 20 + ImGui::GetFontSize()));
    ImGui::Text("Magnitude");
    ImGui::SetCursorPos(cursorPos);

    // Draw box and histogram image.
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(glmToIm(canvasPos), glmToIm(canvasPos + canvasSize));
    drawList->AddRect(glmToIm(canvasPos), glmToIm(canvasPos + canvasSize), ImColor(180, 180, 180, 255));

    cursorPos = ImVec2(ImGui::GetCursorPosX() + xOffset, ImGui::GetCursorPosY());

    // Draw histogram image that we uploaded to the GPU using OpenGL.
    ImGui::SetCursorPos(cursorPos);

    // NOTE(Mathijs): reinterpret casting the pointer is undefined behavior according to the standard.
    // Use memcpy for now and move to std::bit_cast (C++20) when more compilers support it.
    //
    // From the standard:
    // When it is needed to interpret the bytes of an object as a value of a different type, std::memcpy or std::bit_cast (since C++20)can be used:
    // https://en.cppreference.com/w/cpp/language/reinterpret_cast
    ImTextureID imguiTexture;
    std::memcpy(&imguiTexture, &m_histogramImg, sizeof(m_histogramImg));
    ImGui::Image(imguiTexture, glmToIm(canvasSize - glm::vec2(1)));

    // Detect and handle mouse interaction.
    if (!io.MouseDown[0] && !io.MouseDown[1]) {
        m_interactingTriangle = std::nullopt;
    }

    // Place an invisible button on top of the histogram. IsItemHovering returns whether the cursor is
    // hovering over the last added item which in this case is the invisble button. This way we can
    // easily detect whether the cursor is inside the histogram image.
    ImGui::SetCursorPos(cursorPos);
    ImGui::InvisibleButton("tfn_canvas", glmToIm(canvasSize));

    // Mouse position within the histogram image.
    const ImVec2 bbMin = ImGui::GetItemRectMin();
    const ImVec2 bbMax = ImGui::GetItemRectMax();
    const glm::vec2 clippedMousePos {
        std::min(std::max(io.MousePos.x, bbMin.x), bbMax.x),
        std::min(std::max(io.MousePos.y, bbMin.y), bbMax.y)
    };

    const glm::vec2 viewScale { canvasSize.x, -canvasSize.y };
    const glm::vec2 viewOffset { canvasPos.x, canvasPos.y + canvasSize.y };
    if (ImGui::IsItemHovered() && (io.MouseDown[0] || io.MouseDown[1])) {
        const glm::vec2 mousePos = glm::clamp((clippedMousePos - viewOffset) / viewScale, 0.0f, 1.0f);

        // No point is currently being clicked; check if the user clicked any of the points.
        if (!m_interactingTriangle.has_value()) {
            for (int i = 0; i < m_triangles.size(); ++i) {
                for (int j = 0; j < m_triangles[i].points.size(); ++j) {

                    const glm::vec2 pointPosition = normPoint(m_triangles[i].points[j]) * viewScale + viewOffset;
                    const glm::vec2 diff = pointPosition - clippedMousePos;

                    float distanceSquared = glm::dot(diff, diff);
                    if (distanceSquared < pointRadius * pointRadius) {
                        m_interactingTriangle = {i, j};
                    }
                }
            }
        }

        // Left mouse button is down.
        if (io.MouseDown[0]) {
            // On a triangle, move it to current mouse pos

            float newIntensity = mousePos.x * m_maxIntensity;
            float newMagnitude = mousePos.y * m_maxMagnitude;

            if (m_interactingTriangle.has_value()) {
                // A point was already selected.

                int curTriangle = std::get<0>(m_interactingTriangle.value());

                m_selectedTriangle = curTriangle;

                TF2DTriangle& triangle = m_triangles[curTriangle];
                auto &base = triangle.points[0];
                auto &left = triangle.points[1];
                auto &right = triangle.points[2];

                switch (std::get<1>(m_interactingTriangle.value())) {
                case 0: {
                    // base
                    left.x = newIntensity + (left.x - base.x);
                    left.y = newMagnitude + (left.y - base.y);

                    right.x = newIntensity + (right.x - base.x);
                    right.y = newMagnitude + (right.y - base.y);

                    base.x = newIntensity;
                    base.y = newMagnitude;
                } break;
                case 1: {
                    // left
                    left.x = glm::min(base.x, newIntensity);
                    left.y = glm::max(newMagnitude, base.y);

                    right.x = base.x + (base.x - left.x);
                    right.y = left.y;
                } break;
                case 2: {
                    // right
                    right.x = glm::max(base.x, newIntensity);
                    right.y = glm::max(newMagnitude, base.y);

                    left.x = base.x - (right.x - base.x);
                    left.y = right.y;
                } break;
                };
            }
            // Not on a triangle, add new one
            else {
                m_interactingTriangle = {m_triangles.size(), 0};
                m_selectedTriangle = (int)m_triangles.size();

                m_triangles.push_back({{glm::vec2(newIntensity, newMagnitude),
                                        {newIntensity - 30, newMagnitude + 40},
                                        {newIntensity + 30, newMagnitude + 40}},
                                       {1, 1, 1, 0.5}});
            }
        }

        // Right mouse button is down.
        if (io.MouseDown[1] && m_interactingTriangle.has_value()) {
            if (m_triangles.size() > 1) {
                m_triangles.erase(m_triangles.begin() + std::get<0>(m_interactingTriangle.value()));
                m_interactingTriangle = std::nullopt;
                m_selectedTriangle = std::nullopt;
            }
        }
    }

    // Draw m_intensity and m_radius points.
    for (int i = 0; i < m_triangles.size(); ++i) {
        auto triangle = m_triangles[i];

        auto base = triangle.points[0];
        auto left = triangle.points[1];
        auto right = triangle.points[2];

        const ImVec2 basePoint = glmToIm(normPoint(base) * viewScale + viewOffset);
        const ImVec2 leftPoint = glmToIm(normPoint(left) * viewScale + viewOffset);
        const ImVec2 rightPoint = glmToIm(normPoint(right) * viewScale + viewOffset);

        drawList->AddLine(leftPoint, basePoint, 0xFFFFFFFF);
        drawList->AddLine(basePoint, rightPoint, 0xFFFFFFFF);
        drawList->AddLine(rightPoint, leftPoint, 0xFFFFFFFF);

        auto outline = (i == m_selectedTriangle.value_or(-1)) ? 0xFFAAFFFF : 0xFFFFFFFF;
        drawList->AddCircleFilled(leftPoint, pointRadius, outline);
        drawList->AddCircleFilled(basePoint, pointRadius, outline);
        drawList->AddCircleFilled(rightPoint, pointRadius, outline);

        auto color = ImColor(triangle.color.r, triangle.color.g, triangle.color.b, 1.0f);
        drawList->AddCircleFilled(leftPoint, pointRadius * 0.6, color);
        drawList->AddCircleFilled(basePoint, pointRadius * 0.6, color);
        drawList->AddCircleFilled(rightPoint, pointRadius * 0.6, color);
    }

    drawList->PopClipRect();

    // Bottom text.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset + canvasSize.x / 2 - 40);
    ImGui::Text("Voxel Value");

    // Draw text boxes and color picker.
    if (m_selectedTriangle.has_value()) {
        auto base = m_triangles[m_selectedTriangle.value()].points[0];
        auto left = m_triangles[m_selectedTriangle.value()].points[1];
        float radius = base.x - left.x;

        ImGui::NewLine();
        int inputFill = 0;
        ImGui::PushItemWidth(
                0.1f); // These 3 coming lines show nothing but vertically align the text of the intensity and radius boxes
        ImGui::InputScalar("", ImGuiDataType_U32, &inputFill);
        ImGui::SameLine();
        ImGui::Text("Intensity: ");
        ImGui::SameLine();
        ImGui::PushItemWidth(50.f);
        ImGui::InputScalar("", ImGuiDataType_Float, &base.x, NULL, NULL, "%.2f", ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::Text("Radius: ");
        ImGui::SameLine();
        ImGui::PushItemWidth(50.f);
        ImGui::InputScalar("", ImGuiDataType_Float, &radius, NULL, NULL, "%.2f", ImGuiInputTextFlags_ReadOnly);

        ImGui::NewLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset / 2);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() * 0.4f);
        ImGui::ColorPicker4("Color", glm::value_ptr(m_triangles[m_selectedTriangle.value()].color));
    }
}

void TransferFunction2DWidget::updateRenderConfig(render::RenderConfig& renderConfig)
{
    std::vector<render::TF2DTriangle> newTriangles = {};

    for (TF2DTriangle triangle : m_triangles) {
        auto base = triangle.points[0];

        newTriangles.push_back({
            base,
            triangle.points[1].y - base.y,
            triangle.points[2].x - base.x,
            triangle.color
        });
    }

    if (newTriangles != renderConfig.TF2DTriangles) {
        renderConfig.TF2DTriangles = newTriangles;
    }
}
}

glm::vec2 ui::TransferFunction2DWidget::normPoint(glm::vec2& point) const
{
    return {point.x / m_maxIntensity, point.y / m_maxMagnitude};
}

static ImVec2 glmToIm(const glm::vec2& v)
{
    return ImVec2(v.x, v.y);
}

static glm::vec2 ImToGlm(const ImVec2& v)
{
    return glm::vec2(v.x, v.y);
}

// Compute a histogram texture from the volume and gradient data
static std::vector<glm::vec4> createHistogramImage(
    const volume::Volume& volume, const volume::GradientVolume& gradient, const glm::ivec2& res)
{
    const size_t numPixels = static_cast<size_t>(res.x * res.y);
    std::vector<int> bins(numPixels, 0);
    for (int z = 0; z < volume.dims().z; z++) {
        for (int y = 0; y < volume.dims().y; y++) {
            for (int x = 0; x < volume.dims().x; x++) {
                const size_t imgX = static_cast<size_t>(volume.getVoxel(x, y, z));
                const size_t imgY = static_cast<size_t>(res.y - 1) - static_cast<size_t>(gradient.getGradient(x, y, z).magnitude);
                const size_t index = imgX + imgY * static_cast<size_t>(res.x);
                bins[index]++;
            }
        }
    }

    const int maxCount = *std::max_element(std::begin(bins), std::end(bins));
    const float factor = 1.0f / std::log(float(maxCount));

    std::vector<glm::vec4> imageData(numPixels, glm::vec4(0.0f));
    std::transform(std::begin(bins), std::end(bins), std::begin(imageData),
        [&](int count) {
            return glm::vec4(1.0f, 1.0f, 1.0f, std::log(float(count)) * factor);
        });
    return imageData;
}
