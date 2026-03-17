#include "Placement.h"


slope::StateInSlide slope::PlaceRelative::computePlacement(const ScreenPrimitiveInSlide &other) const {

    ScreenPrimitivePtr ptr = this->ptr;
    auto paddingx = this->paddingx;
    auto paddingy = this->paddingy;
    auto X = this->X;
    auto Y = this->Y;

    RelativePlacer rp = [X,Y,ptr,paddingy,paddingx,other] (vec2 other_position) {
        vec2 S = other.first->getRelativeSize()*other.second.getScale();
        vec2 P;
        switch(X) {
        case REL_LEFT:
            P(0) = other_position(0)
                   - S(0)*0.5
                   - paddingx
                   - ptr->getRelativeSize()(0)*0.5
                ;
            break;
        case ABS_LEFT:
            P(0) = paddingx + ptr->getRelativeSize()(0)*0.5;
            break;
        case CENTER_X:
            P(0) = CENTER(0);
            break;
        case SAME_X:
            P(0) = other_position(0);
            break;
        case REL_RIGHT:
            P(0) = other_position(0)
                   + S(0)*0.5
                   + paddingx
                   + ptr->getRelativeSize()(0)*0.5
                ;
            break;
        case ABS_RIGHT:
            P(0) = 1-paddingx-ptr->getRelativeSize()(0)*0.5;
            break;
        }
        switch(Y) {
        case REL_BOTTOM:
            P(1) = other_position(1)
                   + S(1)*0.5
                   + paddingy
                   + ptr->getRelativeSize()(1)*0.5
                ;
            break;
        case ABS_TOP:
            P(1) = paddingy-ptr->getRelativeSize()(1)*0.5;
            break;
        case CENTER_Y:
            P(1) = CENTER(1);
            break;
        case SAME_Y:
            P(1) = other_position(1);
            break;
        case REL_TOP:
            P(1) = other_position(1)
                   - S(1)*0.5
                   - paddingy
                   - ptr->getRelativeSize()(1)*0.5
                ;
            break;
        case ABS_BOTTOM:
            P(1) = 1-paddingy - ptr->getRelativeSize()(1)*0.5;
            break;
        }
        return P;
    };
    StateInSlide sis;
    sis.anchor = other.first->getAnchor();
    sis.placer = rp;
    return sis;
}

slope::PrimitiveInSlide slope::PlaceLeft(ScreenPrimitivePtr ptr, scalar y, scalar padding) {
    vec2 P;
    auto S =ptr->getRelativeSize();
    P(0) = S(0)*0.5+padding;
    P(1) = y;
    return {ptr,StateInSlide(P)};
}
