#include "Text.h"


namespace slope {

// the on-screen scale Text is drawn at; display() and getSize() must agree
static constexpr float kFontScale = 1.5f;

Text::TextPtr Text::Add(const std::string &content, FontID font)
{
    TextPtr rslt = NewPrimitive<Text>();
    rslt->content = content;
    rslt->fontID = font;
    return rslt;
}

void Text::display(const StateInSlide &sis) const
{
    pushFont();

    //set imgui font size
    ImGui::SetWindowFontScale(kFontScale);

    // CalcTextSize reports the unscaled size, so the centring offset has to use
    // the same factor the text is actually drawn at. It used to use 2 against a
    // scale of 1.5, which shifted every string left by a quarter of its width —
    // invisible on a short label, obvious on a sentence.
    auto size = ImGui::CalcTextSize(content.c_str());
    size.x *= kFontScale;
    size.y *= kFontScale;

    ImGuiStyle* style = &ImGui::GetStyle();
    auto old = style->Colors[ImGuiCol_Text];
    style->Colors[ImGuiCol_Text] = RGBA(ImVec4(0,0, 0, sis.alpha));
    auto S = ImGui::GetWindowSize();

    auto P = sis.getAbsolutePosition();
    ImGui::SetCursorPos(ImVec2(P.x - size.x*0.5,P.y - size.y*0.5));
    ImGui::Text(content.c_str());

    style->Colors[ImGuiCol_Text] = old;

    //ImGui::PopFont();
}

void Text::pushFont() const
{
    if (fontID != -1){
        ImGui::PushFont(FontManager::getFont(fontID));
    }
    else{
        //auto F = FontManager::getFont(Style::default_font);
        //ImGui::PushFont(F);
    }
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
    // must agree with display() : this is what the drag editor's hit box and
    // any relative placement are computed from
    auto size = ImGui::CalcTextSize(content.c_str());
    return Size(size.x * kFontScale, size.y * kFontScale);
}

}
