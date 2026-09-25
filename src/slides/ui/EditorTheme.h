#pragma once

#include "imgui.h"

#include <utility>

namespace slope::theme {

/*
 * The Dracula palette of the file editor. Its window and the Documentation panel beside it share it,
 * so both look like one tool.
 */

// Color from a hex value such as 0x282A36.
inline ImVec4 rgb(int hex, float a = 1.f)
{
    return ImVec4(((hex >> 16) & 0xFF) / 255.f, ((hex >> 8) & 0xFF) / 255.f, (hex & 0xFF) / 255.f, a);
}

inline const ImVec4 Background = rgb(0x21222C);
inline const ImVec4 Surface    = rgb(0x282A36);
inline const ImVec4 Raised     = rgb(0x44475A);
inline const ImVec4 Comment    = rgb(0x6272A4);
inline const ImVec4 Text       = rgb(0xF8F8F2);
inline const ImVec4 Muted      = rgb(0xB6B9C9);
inline const ImVec4 Cyan       = rgb(0x8BE9FD);
inline const ImVec4 Orange     = rgb(0xFFB86C);
inline const ImVec4 Purple     = rgb(0xBD93F9);
inline const ImVec4 Green      = rgb(0x50FA7B);

// Number of colors pushed by push().
inline constexpr int kColors = 29;

// Applies the theme. Call it before Begin of a window and call pop() after End. bg_alpha is between 0 and 1.
inline void push(float bg_alpha)
{
    const std::pair<ImGuiCol, ImVec4> colors[kColors] = {
        {ImGuiCol_WindowBg,             rgb(0x21222C, bg_alpha)},
        {ImGuiCol_ChildBg,              rgb(0x000000, 0.f)},
        {ImGuiCol_PopupBg,              rgb(0x21222C, 0.97f)},
        {ImGuiCol_ModalWindowDimBg,     rgb(0x000000, 0.35f)},
        {ImGuiCol_TitleBg,              rgb(0x191A21)},
        {ImGuiCol_TitleBgActive,        rgb(0x191A21)},
        {ImGuiCol_TitleBgCollapsed,     rgb(0x191A21)},
        {ImGuiCol_Border,               Raised},
        {ImGuiCol_Separator,            Raised},
        {ImGuiCol_Text,                 Text},
        {ImGuiCol_TextDisabled,         Muted},
        {ImGuiCol_Header,               rgb(0x44475A, 0.85f)},
        {ImGuiCol_HeaderHovered,        rgb(0x6272A4, 0.85f)},
        {ImGuiCol_HeaderActive,         Comment},
        {ImGuiCol_Button,               Raised},
        {ImGuiCol_ButtonHovered,        Comment},
        {ImGuiCol_ButtonActive,         Purple},
        {ImGuiCol_FrameBg,              Surface},
        {ImGuiCol_FrameBgHovered,       Raised},
        {ImGuiCol_FrameBgActive,        Raised},
        {ImGuiCol_CheckMark,            Green},
        {ImGuiCol_SliderGrab,           Purple},
        {ImGuiCol_SliderGrabActive,     rgb(0xFF79C6)},
        {ImGuiCol_TextSelectedBg,       Raised},
        {ImGuiCol_ScrollbarBg,          rgb(0x000000, 0.f)},
        {ImGuiCol_ScrollbarGrab,        Raised},
        {ImGuiCol_ScrollbarGrabHovered, Comment},
        {ImGuiCol_ScrollbarGrabActive,  Purple},
        {ImGuiCol_ResizeGrip,           rgb(0x44475A, 0.6f)},
    };
    for (const auto& [col, value] : colors)
        ImGui::PushStyleColor(col, value);
}

// Removes the theme.
inline void pop()
{
    ImGui::PopStyleColor(kColors);
}

} // namespace slope::theme
