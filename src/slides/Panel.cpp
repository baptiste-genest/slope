#include "Panel.h"

slope::Panel::Panel(bool reveal, vec2 p) : reveal(reveal) {
    anchor = AbsoluteAnchor::Add(p);
}

slope::AnchorPtr slope::Panel::getAnchor() const {return anchor;}

slope::Panel &slope::Panel::operator<<(ScreenPrimitivePtr ptr) {
    if (root != nullptr){
        std::cerr << "[ PANEL ERROR ] only root can be without position" << std::endl;
        exit(1);
    }
    last_inserted = {ptr,StateInSlide(vec2(0,0))};
    buffer.add(last_inserted);
    root = ptr;
    return *this;
}

slope::Panel &slope::Panel::operator<<(const RelativePlacement &P) {
    if (root == nullptr)
        std::cout << "[ PANEL ERROR ] no root" << std::endl;
    StateInSlide sis;
    if (!P.ptr_other)
        sis = P.computePlacement(last_inserted);
    else
        sis = P.computePlacement({P.ptr_other,buffer[P.ptr_other]});
    buffer[P.ptr] = sis;
    return *this;
}

void slope::Panel::addToSlideManager(SlideManager &sm){
    auto meanpos = computeOffsetToMean(buffer);
    PrimitiveInSlide ris; ris.first = root;
    ris.second.anchor = anchor;
    ris.second.setOffset(-meanpos);
    sm << ris;
    for (auto&& [ptr,sis] : buffer) {
        if (ptr == root) continue;
        if (reveal)
            sm << inNextFrame;
        sm << PrimitiveInSlide(ptr,sis);
    }

}
