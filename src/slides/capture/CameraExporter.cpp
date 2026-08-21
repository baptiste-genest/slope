#include "slides/capture/CameraExporter.h"
#include "content/polyscope_primitives/CameraView.h"
#include "spdlog/spdlog.h"
#include "imgui.h"
#include <fstream>
#include <spdlog/spdlog.h>

void slope::CameraExporter::save(const std::string& file) const
{
    std::ofstream camfile(file);
    camfile << removeResolutionFromCamfile(polyscope::view::getCameraJson());
    spdlog::info("current camera view exported at {}", file);
}

void slope::CameraExporter::drawPopup(WindowManager& wm)
{
    ImGui::OpenPopup("Save current camera");
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::BeginPopupModal("Save current camera", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextItemWidth(display.x * 0.25f);
        ImGui::InputText("filename", filename_buf, sizeof(filename_buf));

        if (ImGui::Button("cancel")) {
            wm.CloseAll();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("save")) {
            save(formatCameraFilename(std::string(filename_buf)));
            wm.CloseAll();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
