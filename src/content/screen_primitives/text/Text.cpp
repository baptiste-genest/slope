#include "content/screen_primitives/text/Text.h"
#include "polyscope/view.h"


namespace slope {

// the on-screen scale Text is drawn at; display() and getSize() must agree
static constexpr float kFontScale = 1.5f;

Text::TextPtr Text::Add(const std::string &content)
{
    TextPtr rslt = NewPrimitive<Text>();
    rslt->content = content;
    return rslt;
}

void Text::display(const StateInSlide &sis) const
{
    //set imgui font size
    ImGui::SetWindowFontScale(kFontScale);

    // CalcTextSize reports the unscaled size, so the centring offset has to use
    // the factor the text is actually drawn at
    auto size = ImGui::CalcTextSize(content.c_str());
    size.x *= kFontScale;
    size.y *= kFontScale;

    ImGuiStyle* style = &ImGui::GetStyle();
    auto old = style->Colors[ImGuiCol_Text];
    // ink follows the background, as the rest of the HUD already does
    const auto& bg = polyscope::view::bgColor;
    style->Colors[ImGuiCol_Text] =
        RGBA(ImVec4(1.f-bg[0], 1.f-bg[1], 1.f-bg[2], sis.getAlpha()));
    auto S = ImGui::GetWindowSize();

    auto P = sis.getAbsolutePosition();
    ImGui::SetCursorPos(ImVec2(P.x - size.x*0.5,P.y - size.y*0.5));
    ImGui::Text(content.c_str());

    style->Colors[ImGuiCol_Text] = old;

    //ImGui::PopFont();
}

void Text::playIntro(const TimeObject&, const StateInSlide &sis)
{
    display(sis);
}

void Text::playOutro(const TimeObject&, const StateInSlide &sis)
{
    display(sis);
}

Primitive::Size Text::getSize() const
{
    // must agree with display(), the hit box and relative placement use it
    auto size = ImGui::CalcTextSize(content.c_str());
    return Size(size.x * kFontScale, size.y * kFontScale);
}

}
