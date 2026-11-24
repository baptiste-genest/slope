#include "content/screen_primitives/layout/Canvas.h"

namespace slope {

scalar Canvas::aspect = 16. / 9.;
ImU32 Canvas::bar_color = IM_COL32(0, 0, 0, 255);
std::array<float, 4> Canvas::background = {1, 1, 1, 1};

ImVec2 Canvas::pos(0, 0);
ImVec2 Canvas::extent(1920, 1080);
ImVec2 Canvas::window(1920, 1080);
scalar Canvas::squeeze = 1;
bool Canvas::fov_swapped = false;
float Canvas::real_fov = 0;
float Canvas::swapped_fov = 0;

void Canvas::Update() {
    // a frame whose ImGui render never ran left the wide fov in place
    restoreFov();

    window = ImGui::GetIO().DisplaySize;
    if (window.x < 1 || window.y < 1)
        return;
    // whole pixels, so text stays as crisp as without bars
    if (window.x / window.y > aspect) {
        extent = ImVec2(std::round(window.y * aspect), window.y);
        pos = ImVec2(std::floor((window.x - extent.x) * 0.5f), 0);
    } else {
        extent = ImVec2(window.x, std::round(window.x / aspect));
        pos = ImVec2(0, std::floor((window.y - extent.y) * 0.5f));
    }
    squeeze = extent.y / window.y;

    Options::ScreenResolutionWidth = size_t(extent.x);
    Options::ScreenResolutionHeight = size_t(extent.y);
}

ImVec2 Canvas::toScreen(const vec2& rel) {
    return ImVec2(pos.x + float(rel(0)) * extent.x, pos.y + float(rel(1)) * extent.y);
}

vec2 Canvas::fromScreen(const ImVec2& px) {
    return vec2((px.x - pos.x) / extent.x, (px.y - pos.y) / extent.y);
}

void Canvas::DrawBars() {
    auto* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 hi(pos.x + extent.x, pos.y + extent.y);
    if (pos.x > 0) {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(pos.x, window.y), bar_color);
        dl->AddRectFilled(ImVec2(hi.x, 0), window, bar_color);
    }
    if (pos.y > 0) {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(window.x, pos.y), bar_color);
        dl->AddRectFilled(ImVec2(0, hi.y), window, bar_color);
    }
}

void Canvas::BeginRenderFov() {
    restoreFov();
    if (squeeze >= 1)
        return;
    real_fov = polyscope::view::fov;
    swapped_fov = float(glm::degrees(2 * std::atan(std::tan(glm::radians(real_fov) * 0.5) / squeeze)));
    polyscope::view::fov = swapped_fov;
    fov_swapped = true;
    // the foreground list is executed last, after the scene and the gizmos
    ImGui::GetForegroundDrawList()->AddCallback([](const ImDrawList*, const ImDrawCmd*) { restoreFov(); }, nullptr);
}

void Canvas::restoreFov() {
    if (!fov_swapped)
        return;
    // a flight writes its own fov, which is a real one
    if (polyscope::view::fov == swapped_fov)
        polyscope::view::fov = real_fov;
    fov_swapped = false;
}

glm::mat4 Canvas::renderedProjection() {
    glm::mat4 P = polyscope::view::getCameraPerspectiveMatrix();
    if (fov_swapped || squeeze >= 1)
        return P;
    // what the wide fov gives, rows x and y scaled
    for (int c = 0; c < 4; c++) {
        P[c][0] *= float(squeeze);
        P[c][1] *= float(squeeze);
    }
    return P;
}

glm::mat4 Canvas::canvasProjection() {
    glm::mat4 P = renderedProjection();
    const float sx = window.x / extent.x, sy = window.y / extent.y;
    for (int c = 0; c < 4; c++) {
        P[c][0] *= sx;
        P[c][1] *= sy;
    }
    return P;
}

} // namespace slope
