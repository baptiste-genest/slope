#include "slides/ui/HUD.h"
#include "polyscope/polyscope.h"
#include "imgui.h"
#include <set>
#include <cmath>

void slope::HUD::initialize(int n_slides, const std::function<std::string(int)>& get_title)
{
    slide_numbers.resize(n_slides);
    std::set<std::string> done;

    for (int i = 0; i < n_slides; i++) {
        done.insert(get_title(i));
        slide_numbers[i] = int(done.size()) - 1;
    }
    int nb_distinct = int(done.size());

    slide_number_display.resize(nb_distinct);
    for (int i = 0; i < nb_distinct; i++) {
        auto label = std::to_string(i + 1) + "/" + std::to_string(nb_distinct);
        slide_number_display[i] = PlaceBottomRight(Text::Add(label), 0.01);
    }
}

void slope::HUD::drawSlideNumber(size_t current_slide) const
{
    const auto& DSN = slide_number_display[slide_numbers[current_slide]];
    DSN.first->play(TimeObject(), DSN.second);
}

void slope::HUD::drawGizmoMode(const std::string& what) const
{
    auto* dl = ImGui::GetWindowDrawList();
    auto  S  = ImGui::GetWindowSize();

    auto& bg = polyscope::view::bgColor;
    auto inv = [](float f) { return (int)((1.0f - f) * 255 + 0.5f); };
    ImU32 col = IM_COL32(inv(bg[0]), inv(bg[1]), inv(bg[2]), 200);

    std::string label = "gizmo mode : " + what;
    float size = ImGui::GetFontSize() * 1.5f;
    dl->AddText(ImGui::GetFont(), size, ImVec2(S.x * 0.02f, S.y * 0.02f), col, label.c_str());
}

void slope::HUD::drawPauseIndicator(float elapsed, float duration) const
{
    float remaining = 1.0f - elapsed / duration;

    auto* dl = ImGui::GetWindowDrawList();
    auto  S  = ImGui::GetWindowSize();

    constexpr float padding   = 0.02f;
    constexpr float norm_y    = 1.0f - 0.07f;
    constexpr float thickness = 2.5f;
    constexpr int   segments  = 48;

    float radius = S.y * 0.012f;
    ImVec2 center(S.x * (1.0f - padding) - radius, S.y * norm_y);

    auto& bg = polyscope::view::bgColor;
    auto inv = [](float f) { return (int)((1.0f - f) * 255 + 0.5f); };
    ImU32 col_dim  = IM_COL32(inv(bg[0]), inv(bg[1]), inv(bg[2]),  50);
    ImU32 col_full = IM_COL32(inv(bg[0]), inv(bg[1]), inv(bg[2]), 220);

    dl->AddCircle(center, radius, col_dim, segments, thickness);

    if (remaining > 0.0f) {
        constexpr float start = -M_PI * 0.5f;
        dl->PathArcTo(center, radius, start, start + remaining * 2.0f * M_PI, segments);
        dl->PathStroke(col_full, false, thickness);
    }
}

std::filesystem::path slope::HUD::drawReloadErrors(
    const std::map<std::filesystem::path, std::string>& errors) const
{
    std::filesystem::path clicked;
    if (errors.empty())
        return clicked;

    const ImVec2 S = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(S.x * 0.01f, S.y * 0.99f), ImGuiCond_Always, ImVec2(0.f, 1.f));
    ImGui::SetNextWindowBgAlpha(0.6f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                                 | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
                                 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##reload_errors", nullptr, flags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.55f, 0.45f, 1.f));
        ImGui::Text("! %d file%s failed to reload", int(errors.size()), errors.size() > 1 ? "s" : "");
        int shown = 0;
        for (const auto& [file, msg] : errors) {
            if (shown++ == 3) {
                ImGui::TextDisabled("+%d more", int(errors.size()) - 3);
                break;
            }
            std::string first = msg.substr(0, msg.find('\n'));
            if (first.size() > 90)
                first = first.substr(0, 87) + "...";
            ImGui::PushID(file.string().c_str());
            if (ImGui::Selectable((file.filename().string() + "   " + first).c_str()))
                clicked = file;
            ImGui::PopID();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    return clicked;
}
