#ifndef PLACEMENT_H
#define PLACEMENT_H

#include "content/screen_primitives/ScreenPrimitive.h"


namespace slope {

// Named screen points that the "config:" block of a deck can move.
// The compiled values are kept here, to go back to them.
namespace placement_default {
inline const vec2 CENTER(0.5,0.5);
inline const vec2 TOP(0.5,0.1);
inline const vec2 BOTTOM(0.5,0.9);
}
extern vec2 CENTER;
extern vec2 TOP;
extern vec2 BOTTOM;

// Puts ptr in the place of ptr_other on the current slide.
struct Replace {
    Replace(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other = nullptr) : ptr(ptr),ptr_other(ptr_other) {}
    ScreenPrimitivePtr ptr;
    ScreenPrimitivePtr ptr_other = nullptr;
};


// Places a primitive relative to another one. The other one is ptr_other, or the one given to computePlacement.
struct RelativePlacement {

    RelativePlacement(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other = nullptr) : ptr(ptr),ptr_other(ptr_other) {}

    // Returns the state that puts ptr next to `other`.
    virtual StateInSlide computePlacement(const ScreenPrimitiveInSlide& other) const = 0;

    ScreenPrimitivePtr ptr;
    ScreenPrimitivePtr ptr_other = nullptr;
};


// Horizontal rule of PlaceRelative. REL is relative to the other primitive, and ABS is relative to the window edge.
// SAME_X uses the same x as the other primitive and CENTER_X the center of the screen.
enum placeX {
    REL_LEFT,
    ABS_LEFT,
    CENTER_X,
    SAME_X,
    REL_RIGHT,
    ABS_RIGHT
};
// Vertical rule of PlaceRelative, with the same meaning as placeX.
enum placeY {
    REL_TOP,
    ABS_TOP,
    CENTER_Y,
    SAME_Y,
    REL_BOTTOM,
    ABS_BOTTOM
    };

// Places a primitive with one rule per axis and a gap in relative units.
struct PlaceRelative : public RelativePlacement {
    PlaceRelative(ScreenPrimitivePtr ptr,placeX X,placeY Y,scalar paddingx = 0.01,scalar paddingy = 0.01) : RelativePlacement(ptr),X(X),Y(Y),paddingx(paddingx),paddingy(paddingy) {

    }
    PlaceRelative(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other,placeX X,placeY Y,scalar paddingx = 0.01,scalar paddingy = 0.01) : RelativePlacement(ptr,ptr_other),X(X),Y(Y),paddingx(paddingx),paddingy(paddingy) {}

    StateInSlide computePlacement(const ScreenPrimitiveInSlide& other) const override;

    scalar paddingx,paddingy;
    placeX X;
    placeY Y;
};

// Places ptr on the same line as the other primitive, on its right if side is 1 and on its left otherwise.
inline PlaceRelative PlaceNextTo(ScreenPrimitivePtr ptr,int side,scalar paddingx = 0.01,ScreenPrimitivePtr other = nullptr) {
    if (other) {
        if (side == 1)
            return PlaceRelative(ptr,other,placeX::REL_RIGHT,SAME_Y,paddingx);
        return PlaceRelative(ptr,other,placeX::REL_LEFT,SAME_Y,paddingx);
    }
    if (side == 1)
        return PlaceRelative(ptr,placeX::REL_RIGHT,SAME_Y,paddingx);
    return PlaceRelative(ptr,placeX::REL_LEFT,SAME_Y,paddingx);
}

// Places ptr under the other primitive, with the same x.
inline PlaceRelative PlaceBelow(ScreenPrimitivePtr ptr,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,placeX::SAME_X,placeY::REL_BOTTOM,0.01,paddingy);
}
inline PlaceRelative PlaceBelow(ScreenPrimitivePtr ptr,ScreenPrimitivePtr other,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,other,placeX::SAME_X,placeY::REL_BOTTOM,0.01,paddingy);
}
// Places ptr above the other primitive, with the same x.
inline PlaceRelative PlaceAbove(ScreenPrimitivePtr ptr,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,placeX::SAME_X,placeY::REL_TOP,0.01,paddingy);
}
inline PlaceRelative PlaceAbove(ScreenPrimitivePtr ptr,ScreenPrimitivePtr other,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,other,placeX::SAME_X,placeY::REL_TOP,0.01,paddingy);
}


// Place ptr against an edge of the window, at height y (or x for top and bottom), with the given gap.
// The corner versions use the gap on both axes.
PrimitiveInSlide PlaceLeft(ScreenPrimitivePtr ptr,scalar y = 0.5,scalar padding = 0.1);


inline PrimitiveInSlide PlaceRight(ScreenPrimitivePtr ptr,scalar y = 0.5,scalar padding = 0.1) {
    vec2 P;
    P(0) = 1-ptr->getRelativeSize()(0)*0.5-padding;
    P(1) = y;
    return {ptr,StateInSlide(P)};
}

inline PrimitiveInSlide PlaceBottom(ScreenPrimitivePtr ptr,scalar x = 0.5,scalar padding = 0.1) {
    vec2 P;
    P(0) = x;
    P(1) = 1. - ptr->getRelativeSize()(1)*0.5-padding;
    return {ptr,StateInSlide(P)};
}

inline PrimitiveInSlide PlaceTop(ScreenPrimitivePtr ptr,scalar x = 0.5,scalar padding = 0.1) {
    vec2 P;
    P(0) = x;
    P(1) = ptr->getRelativeSize()(1)*0.5+padding;
    return {ptr,StateInSlide(P)};
}

inline PrimitiveInSlide PlaceBottomRight(ScreenPrimitivePtr ptr,scalar padding = 0.1) {
    vec2 P;
    P(0) = 1- ptr->getRelativeSize()(0)*0.5-padding;
    P(1) = 1- ptr->getRelativeSize()(1)*0.5-padding;
    return {ptr,StateInSlide(P)};
}

inline PrimitiveInSlide PlaceBottomLeft(ScreenPrimitivePtr ptr,scalar padding = 0.1) {
    vec2 P;
    P(0) = ptr->getRelativeSize()(0)*0.5+padding;
    P(1) = 1- ptr->getRelativeSize()(1)*0.5-padding;
    return {ptr,StateInSlide(P)};
}





}

#endif // PLACEMENT_H
