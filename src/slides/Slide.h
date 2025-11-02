#ifndef SLIDE_H
#define SLIDE_H

#include "../content/primitive.h"
#include "../content/ScreenPrimitive.h"
#include "../content/CameraView.h"

namespace slope {

struct Transition{
    PrimitiveID pid;
    StateInSlide state_a;
    StateInSlide state_b;
};
using Transitions = std::vector<Transition>;

struct Slide : public std::map<PrimitivePtr,StateInSlide> {

    void add(PrimitivePtr p,const StateInSlide& sis = {});
    void add(PrimitivePtr p,const vec2& pos);

    std::vector<PrimitiveInSlide> getDepthSorted();

    void add(PrimitiveInSlide pis);

    void remove(PrimitivePtr ptr);

    TextualPrimitivePtr title_primitive = nullptr;
    CameraViewPtr camera = nullptr;

    std::map<ScreenPrimitivePtr,StateInSlide> getScreenPrimitives() const;

    std::map<PolyscopePrimitivePtr,StateInSlide> getPolyscopePrimitives() const;


    std::string getTitle() const;

    void setCam() const;

    bool sameCamera(const Slide& other) const;


};

}

#endif // SLIDE_H
