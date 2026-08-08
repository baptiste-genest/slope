#pragma once

#include "../content/StateInSlide.h"
#include "../content/screen_primitives/Text.h"
#include "../content/screen_primitives/Placement.h"
#include <functional>
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
};

} // namespace slope
