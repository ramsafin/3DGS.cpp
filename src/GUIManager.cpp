#include "GUIManager.hpp"

#include <memory>

#include "imgui.h"
#include "implot/implot.h"

struct ScrollingBuffer final {
    static constexpr int DEFAULT_MAX_SIZE = 10'000;

    int maxSize = 0;
    int offset = 0;
    ImVector<ImVec2> data;

    explicit ScrollingBuffer(int max_size = DEFAULT_MAX_SIZE) {
        maxSize = max_size;
        offset = 0;
        data.reserve(maxSize);
    }

    void addPoint(float x, float y) {
        // circular buffer
        if (data.size() < maxSize)
            data.push_back(ImVec2(x, y));
        else {
            data[offset] = ImVec2(x, y);
            offset = (offset + 1) % maxSize;
        }
    }

    void clear() {
        if (data.size() > 0) {
            data.shrink(0);
            offset = 0;
        }
    }
};

static std::shared_ptr<std::unordered_map<std::string, ScrollingBuffer>> metricsMap;
static std::shared_ptr<std::unordered_map<std::string, float>> textMetricsMap;
static constexpr auto DEFAULT_HISTORY_SECONDS = 10.0f;

GUIManager::GUIManager() {
    metricsMap = std::make_shared<std::unordered_map<std::string, ScrollingBuffer>>();
    textMetricsMap = std::make_shared<std::unordered_map<std::string, float>>();
}

GUIManager::~GUIManager() {
    if (ImPlot::GetCurrentContext() != nullptr) {
        ImPlot::DestroyContext();
    }
}

void GUIManager::init() {
    ImPlot::CreateContext();
}

void GUIManager::buildGui() {
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::Begin("Performance");

    static float history = DEFAULT_HISTORY_SECONDS;
    static ImPlotAxisFlags flags = ImPlotAxisFlags_AutoFit;

    if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("s", "time (ms)", flags, flags);
        const auto t = ImGui::GetTime();
        ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);
        for (auto& [name, values] : *metricsMap) {
            if (!values.data.empty()) {
                ImPlot::PlotLine(
                    name.c_str(),
                    &values.data[0].x,
                    &values.data[0].y,
                    values.data.size(),
                    0,
                    values.offset,
                    2 * sizeof(float)
                );
            }
        }
        ImPlot::EndPlot();
    }
    ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");
    ImGui::End();

    bool popen = true;
    ImGui::SetNextWindowPos(ImVec2(10, 270), ImGuiCond_FirstUseEver);
    ImGui::Begin("Metrics", &popen, ImGuiWindowFlags_AlwaysAutoResize);
    for (auto& [name, value] : *textMetricsMap) {
        ImGui::Text("%s: %.2f", name.c_str(), value);
    }
    for (auto& [name, values] : *metricsMap) {
        ImGui::Text("%s: %.2f", name.c_str(), values.data.empty() ? 0 : values.data.back().y);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(10, 310), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls", &popen, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("MMB drag: orbit");
    ImGui::Text("RMB drag: pan");
    ImGui::Text("Scroll: dolly");
    ImGui::Text("WASD: fly");
    ImGui::Text("Space: up");
    ImGui::Text("Shift: down");
    ImGui::Text("F: frame scene");
    ImGui::Text("R: reset camera");
    ImGui::Separator();
    ImGui::Text("Position: %.3f, %.3f, %.3f", cameraPosition.x, cameraPosition.y, cameraPosition.z);
    ImGui::Text(
        "Rotation quat [w,x,y,z]: %.4f, %.4f, %.4f, %.4f",
        cameraRotation.w,
        cameraRotation.x,
        cameraRotation.y,
        cameraRotation.z
    );
    ImGui::End();

    if (!wantCaptureKeyboard()) {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
            frameSceneRequested = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            resetCameraRequested = true;
        }
    }
}

void GUIManager::pushTextMetric(const std::string& name, float value) {
    if (!textMetricsMap->contains(name)) {
        textMetricsMap->insert({name, value});
    } else {
        textMetricsMap->at(name) = value;
    }
}

void GUIManager::pushMetric(const std::string& name, float value) {
    int maxSize = 600;
    if (!metricsMap->contains(name)) {
        metricsMap->insert({name, ScrollingBuffer{}});
    }
    metricsMap->at(name).addPoint(static_cast<float>(ImGui::GetTime()), value);
}

void GUIManager::pushMetric(const std::unordered_map<std::string, float>& name) {
    for (auto& [metric_name, metric_value] : name) {
        pushMetric(metric_name, metric_value);
    }
}

bool GUIManager::wantCaptureMouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUIManager::wantCaptureKeyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}
