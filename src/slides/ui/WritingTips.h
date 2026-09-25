#pragma once

#include <filesystem>

struct ImFont;

namespace slope {

/*
 * The side panel of the file editor. It shows what can be written in the open file.
 * The deck keys and item fields come from the item registry, and the shader prelude comes from Shader,
 * so the tips follow the code.
 */
namespace WritingTips {

// True when the file has a panel.
bool available(const std::filesystem::path& file);

// Draws the panel of the file inside the current child window. Code is drawn with the font mono at size px.
void draw(const std::filesystem::path& file, ImFont* mono, float px);

} // namespace WritingTips

} // namespace slope
