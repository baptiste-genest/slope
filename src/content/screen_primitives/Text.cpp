#include "Text.h"


namespace slope {

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
    ImGui::SetWindowFontScale(1.5);

    auto size = ImGui::CalcTextSize(content.c_str());
    size.x *= 2;
    size.y *= 2;

    ImGuiStyle* style = &ImGui::GetStyle();
    auto old = style->Colors[ImGuiCol_Text];
    style->Colors[ImGuiCol_Text] = RGBA(ImVec4(0,0, 0, alpha));
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

void Text::playIntro(const TimeObject& t, const StateInSlide &sis)
{
    alpha = smoothstep(t.transitionParameter);
    display(sis);
}

void Text::playOutro(const TimeObject& t, const StateInSlide &sis)
{
    alpha = 1-smoothstep(t.transitionParameter);
    display(sis);

}

Primitive::Size Text::getSize() const
{
    //pushFont();
    auto size = ImGui::CalcTextSize(content.c_str());
    //ImGui::PopFont();
    return Size(size.x,size.y);
}

}
