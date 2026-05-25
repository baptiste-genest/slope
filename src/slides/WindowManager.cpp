#include "WindowManager.h"
#include <iostream>

namespace slope {

void InputManager::printInputs()
{
    std::cout << "	[ KEYBOARD INPUT GUIDE ]:\n";
    for (const auto& input : inputs) {
        std::cout << "   - [" << input.shortcut << "] : " << input.description << "\n";
    }
}


} // namespace slope
