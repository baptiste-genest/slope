#pragma once

#include "slides/ui/WindowManager.h"

namespace slope {

// Pop-up that saves the current camera view to a file in the views folder.
class CameraExporter {
    char filename_buf[256] = {};

    // Writes the current camera to a file.
    void save(const std::string& file) const;

public:
    // Draws the pop-up.
    void drawPopup(WindowManager& wm);
};

} // namespace slope
