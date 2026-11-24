#ifndef CANVAS_H
#define CANVAS_H

#include "content/config/Options.h"
#include <array>

namespace slope {

// The slide area: the largest rectangle of the deck's aspect ratio that fits in the window, centered.
// The rest of the window is covered by bars, so the layout never depends on the screen.
// Relative positions are fractions of the canvas, screen positions are ImGui pixels of the window.
class Canvas {
public:
    // Width over height of the slides.
    static scalar aspect;
    static ImU32 bar_color;
    // The slide background colour. polyscope::view::bgColor holds it pre-tonemapped.
    static std::array<float, 4> background;

    // Fits the canvas in the window, once per frame before anything is drawn.
    static void Update();

    static ImVec2 origin() { return pos; }
    static ImVec2 size() { return extent; }

    // Screen pixels per design pixel, a slide being designed 1080 high. One factor for both axes,
    // so nothing is ever stretched. Before the first frame, the canvas the command line size would give.
    static scalar pixelScale() {
        const scalar w = Options::ScreenResolutionWidth, h = Options::ScreenResolutionHeight;
        return std::min(h, w / aspect) / 1080.;
    }

    static ImVec2 toScreen(const vec2& rel);
    static vec2 fromScreen(const ImVec2& px);

    // Draws the bars over the 3D scene and under every ImGui window.
    static void DrawBars();

    // polyscope renders the whole window with a vertical fov. With bars above and below, the fov is
    // widened for the render and the gizmos only, and set back by an ImGui callback once they are done,
    // so input, flights and saved views only ever see the real one. Call at the end of the frame's callback.
    static void BeginRenderFov();

    // The projection polyscope rasterizes with, clip space of the whole window.
    static glm::mat4 renderedProjection();
    // The projection seen from the canvas, clip space of the canvas, and its vertical fov in degrees.
    static glm::mat4 canvasProjection();
    static float canvasFov() { return fov_swapped ? real_fov : polyscope::view::fov; }
    static ImVec2 windowSize() { return window; }

private:
    static ImVec2 pos, extent, window;
    // Vertical squeeze of the render, canvas height over window height, 1 with bars on the sides.
    static scalar squeeze;
    static bool fov_swapped;
    static float real_fov, swapped_fov;

    static void restoreFov();
};

} // namespace slope

#endif // CANVAS_H
