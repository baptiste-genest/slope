#ifndef PLACEMENT_H
#define PLACEMENT_H

#include "ScreenPrimitive.h"


namespace slope {

const vec2 CENTER(0.5,0.5);
const vec2 TOP(0.5,0.1);
const vec2 BOTTOM(0.5,0.9);

struct Replace {
    Replace(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other = nullptr) : ptr(ptr),ptr_other(ptr_other) {}
    ScreenPrimitivePtr ptr;
    ScreenPrimitivePtr ptr_other = nullptr;
};


struct RelativePlacement {

    RelativePlacement(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other = nullptr) : ptr(ptr),ptr_other(ptr_other) {}

    virtual StateInSlide computePlacement(const ScreenPrimitiveInSlide& other) const = 0;

    ScreenPrimitivePtr ptr;
    ScreenPrimitivePtr ptr_other = nullptr;
};


enum placeX {
    REL_LEFT,
    ABS_LEFT,
    CENTER_X,
    SAME_X,
    REL_RIGHT,
    ABS_RIGHT
};
enum placeY {
    REL_TOP,
    ABS_TOP,
    CENTER_Y,
    SAME_Y,
    REL_BOTTOM,
    ABS_BOTTOM
    };

struct PlaceRelative : public RelativePlacement {
    PlaceRelative(ScreenPrimitivePtr ptr,placeX X,placeY Y,scalar paddingx = 0.01,scalar paddingy = 0.01) : RelativePlacement(ptr),X(X),Y(Y),paddingx(paddingx),paddingy(paddingy) {

    }
    PlaceRelative(ScreenPrimitivePtr ptr,ScreenPrimitivePtr ptr_other,placeX X,placeY Y,scalar paddingx = 0.01,scalar paddingy = 0.01) : RelativePlacement(ptr,ptr_other),X(X),Y(Y),paddingx(paddingx),paddingy(paddingy) {}

    StateInSlide computePlacement(const ScreenPrimitiveInSlide& other) const override;

    scalar paddingx,paddingy;
    placeX X;
    placeY Y;
};

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

inline PlaceRelative PlaceBelow(ScreenPrimitivePtr ptr,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,placeX::SAME_X,placeY::REL_BOTTOM,0.01,paddingy);
}
inline PlaceRelative PlaceBelow(ScreenPrimitivePtr ptr,ScreenPrimitivePtr other,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,other,placeX::SAME_X,placeY::REL_BOTTOM,0.01,paddingy);
}
inline PlaceRelative PlaceAbove(ScreenPrimitivePtr ptr,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,placeX::SAME_X,placeY::REL_TOP,0.01,paddingy);
}
inline PlaceRelative PlaceAbove(ScreenPrimitivePtr ptr,ScreenPrimitivePtr other,scalar paddingy = 0.01) {
    return PlaceRelative(ptr,other,placeX::SAME_X,placeY::REL_TOP,0.01,paddingy);
}


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
