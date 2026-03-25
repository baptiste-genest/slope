#pragma once

namespace slope {

enum class WindowType {
    none,
    Palette,
    Camera,
    SlideMenu,
    Transform,
    DragAndDrop,
    PolyscopeGUI
};

class WindowManager {
public:

    bool Toggle(WindowType w) {
        if (active == WindowType::none){
            active = w;
            return true;
        }
        else if (active == w)
            active = WindowType::none;
        return false;
    }

    bool isAnyOpen() const {
        return active != WindowType::none;
    }

    bool isOpen(WindowType w) const {
        return active == w;
    }

    void CloseAll() {
        active = WindowType::none;
    }

    bool isOtherOpen(WindowType w) const {
        return active != WindowType::none && active != w;
    }

    WindowManager(){}

private:
    WindowType active = WindowType::none;
};

} // namespace slope
