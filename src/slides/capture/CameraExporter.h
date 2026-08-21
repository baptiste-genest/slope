#pragma once

#include "slides/ui/WindowManager.h"

namespace slope {

class CameraExporter {
    char filename_buf[256] = {};

    void save(const std::string& file) const;

public:
    void drawPopup(WindowManager& wm);
};

} // namespace slope
