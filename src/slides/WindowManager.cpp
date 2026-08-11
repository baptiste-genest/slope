#include "WindowManager.h"
#include <iostream>

namespace slope {

namespace {

// the shortcuts are authored as "Ctrl+..." since that's the binding
// (ImGuiKey_LeftCtrl, see DragEditor.cpp/Slideshow.cpp) ; macOS keyboards
// and users call that same key "Cmd" by convention, so only the label shown
// here changes, not the actual key checked
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
