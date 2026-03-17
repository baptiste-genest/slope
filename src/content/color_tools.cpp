#include "color_tools.h"
#include "Options.h"


std::set<std::string> slope::PaletteHandler::labels;

slope::ColorType slope::PaletteHandler::GetColorFromLabel(std::string label)
{
    std::ifstream file (slope::Options::ProjectViewsPath + label + ".color");
    if (!file.is_open())
        throw std::runtime_error("couldn't read label file");
    ColorType rslt;
    file >> rslt.x >> rslt.y >> rslt.z >> rslt.w;
    return rslt;
}

void slope::PaletteHandler::WriteToLabel(std::string label, glm::vec4 color, bool override) {
    std::string command = fmt::format("mkdir {} 2>/dev/null",slope::Options::ProjectViewsPath);
    system(command.c_str());
    std::string filepath = slope::Options::ProjectViewsPath + label + ".color";
    if (!override && std::filesystem::exists(filepath)) {
        return;
    }
    std::ofstream file(filepath);

    if (!file.is_open()){
        spdlog::error("could not open file {}",filepath);
        throw std::runtime_error("could not open file");
    }
    file << color.x << " " << color.y << " " << color.z << " " << color.w << std::endl;
}

void slope::PaletteHandler::ShowColorPickingModule()
{
    ImGui::Begin("Color palettes");
    ImGui::SetNextFrameWantCaptureMouse(true);
    for (const auto& label : labels) {
        ColorType color = GetColorFromLabel(label);
        ImColor im_color(color.x, color.y, color.z, color.w);
        if (ImGui::ColorEdit4(label.c_str(), (float*)&im_color)) {
            WriteToLabel(label, glm::vec4(im_color.Value.x, im_color.Value.y, im_color.Value.z, im_color.Value.w),true);
        }
    }
    ImGui::End();
}
