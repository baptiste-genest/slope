#pragma once

#include "content/core/StateInSlide.h"
#include "content/screen_primitives/text/Text.h"
#include "content/screen_primitives/layout/Placement.h"
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace slope {

class HUD {
    std::vector<int> slide_numbers;
    std::vector<PrimitiveInSlide> slide_number_display;

public:
    void initialize(int n_slides, const std::function<std::string(int)>& get_title);
    void drawSlideNumber(size_t current_slide) const;
    void drawPauseIndicator(float elapsed, float duration) const;
    void drawGizmoMode(const std::string& what) const;
    // the files whose last reload failed, bottom left; returns the one clicked, empty if none
    std::filesystem::path drawReloadErrors(const std::map<std::filesystem::path, std::string>& errors) const;
};

} // namespace slope
