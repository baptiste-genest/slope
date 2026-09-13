#pragma once

#include <filesystem>

struct ImFont;

namespace slope {

/*
 * The side panel of the file editor: what can be written in the open file.
 * Deck keys and item fields come from the item registry and the shader
 * prelude from Shader, so the tips follow the code.
 */
namespace WritingTips {

// whether the file gets a panel at all
bool available(const std::filesystem::path& file);

// draws the panel for the file inside the current child window; code in mono at px
void draw(const std::filesystem::path& file, ImFont* mono, float px);

}

} // namespace slope
