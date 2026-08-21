#include "slides/ui/WindowManager.h"
#include <iostream>

namespace slope {

namespace {

// only the label changes, the binding stays ImGuiKey_LeftCtrl everywhere
std::string platformLabel(std::string shortcut)
{
#ifdef __APPLE__
    std::size_t pos;
    while ((pos = shortcut.find("Ctrl")) != std::string::npos)
        shortcut.replace(pos, 4, "Cmd");
#endif
    return shortcut;
}

} // namespace

void InputManager::printInputs()
{
    std::cout << "	[ KEYBOARD INPUT GUIDE ]:\n";
    for (const auto& input : inputs) {
        std::cout << "   - [" << platformLabel(input.shortcut) << "] : " << input.description << "\n";
    }
}


} // namespace slope
