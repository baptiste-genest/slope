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

// Information drawn over the slides, such as the slide number and the reload errors.
class HUD {
    std::vector<int> slide_numbers;
    std::vector<PrimitiveInSlide> slide_number_display;

public:
    // Prepares the slide numbers for a deck of n_slides slides.
    void initialize(int n_slides, const std::function<std::string(int)>& get_title);
    // Draws the number of the current slide in the bottom right corner.
    void drawSlideNumber(size_t current_slide) const;
    // Draws the progress of a pause, given the elapsed time and the total duration in seconds.
    void drawPauseIndicator(float elapsed, float duration) const;
    // Draws the name of the active gizmo mode.
    void drawGizmoMode(const std::string& what) const;
    // Draws in the bottom left corner the files whose last reload failed.
    // Returns the file that was clicked, or an empty path.
    std::filesystem::path drawReloadErrors(const std::map<std::filesystem::path, std::string>& errors) const;
};

} // namespace slope
