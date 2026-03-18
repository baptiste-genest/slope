#include "Box.h"

namespace slope {


vec2 FixedBox::getSize() const {
    vec2 rslt = size;
    auto W = ImGui::GetWindowSize();
    return vec2(rslt(0)*W.x, rslt(1)*W.y);
}


void Box::drawBox(const vec2 &pos, const vec2 &size, Color c, float roundness, float alpha) const{
    auto color = c.getImColor();
    color.Value.w *= alpha;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto W = ImGui::GetWindowSize();
    vec2 pmin = pos - size*0.5;
    vec2 pmax = pos + size*0.5;


    draw_list->AddRectFilled(
        ImVec2(pmin(0)*W.x, pmin(1)*W.y),
        ImVec2(pmax(0)*W.x, pmax(1)*W.y),
        color,roundness
        );
}

EnglobingBox::EnglobingParams EnglobingBox::ComputeEnglobing(const std::vector<InsideType> &primitives) const {
    if (primitives.empty()) {
        EnglobingParams params;
        params.bbox = vec2(0, 0);
        params.pos = vec2(0, 0);
        params.min_depth = getDepth()+1;
        return params;
    }

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

    int min_depth = std::numeric_limits<int>::max();
    for (const auto& primitive : primitives) {
        min_depth = std::min(min_depth, primitive.first->getDepth());
    }

    EnglobingParams params;
    params.bbox = maxCorner - minCorner;
    params.pos = (maxCorner + minCorner) * 0.5;
    params.min_depth = min_depth;

    return params;
}

} // namespace slope
