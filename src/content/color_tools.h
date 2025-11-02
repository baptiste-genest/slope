#include "../PCH.h"

// Get the chromatic opposite of a color in HSV space, input and output are RGB
inline glm::vec3 GetChromaticOpposite(const glm::vec3& color_rgb) {
    glm::vec3 color_hsv = polyscope::RGBtoHSV(color_rgb);
    color_hsv[0] = std::fmod(color_hsv[0] + 0.5, 1.0);
    return polyscope::HSVtoRGB(color_hsv);
}
