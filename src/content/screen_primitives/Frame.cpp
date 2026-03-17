#include "Frame.h"

namespace slope {


vec2 AbsoluteFrame::getSize() const {
    vec2 rslt = size;
    auto W = ImGui::GetWindowSize();
    return vec2(rslt(0)*W.x, rslt(1)*W.y);
}


void DrawRectangle(const vec2 &center, const vec2 &size, const ImColor &color, float roundness)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto W = ImGui::GetWindowSize();
    vec2 pmin = center - size*0.5;
    vec2 pmax = center + size*0.5;


    draw_list->AddRectFilled(
        ImVec2(pmin(0)*W.x, pmin(1)*W.y),
        ImVec2(pmax(0)*W.x, pmax(1)*W.y),
        color,roundness
        );

}

std::pair<vec2,vec2> EnglobingFrame::ComputeBoundingBoxAndPosition(const std::vector<InsideType> &primitives) const {
    if (primitives.empty()) return {vec2(0, 0),vec2(0,0)};

    vec2 minCorner = vec2(1000,1000);
    vec2 maxCorner = -vec2(1000,1000);

    for (const auto& primitive : primitives) {
        vec2 pos = primitive.second.getPosition();
        vec2 size = primitive.first->getRelativeSize()*primitive.second.getScale();
        // std::cout << pos.transpose() << " " << size.transpose() << std::endl;
        minCorner = minCorner.cwiseMin(pos - size*0.5);
        maxCorner = maxCorner.cwiseMax(pos + size*0.5);
    }
    maxCorner.array() += padding;
    minCorner.array() -= padding;
    // std::cout << "Min corner: " << minCorner.transpose() << ", Max corner: " << maxCorner.transpose() << std::endl;

    return {maxCorner - minCorner,(maxCorner+minCorner)*0.5};
}

} // namespace slope
