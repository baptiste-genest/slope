#ifndef SLIDE_H
#define SLIDE_H

#include "../content/primitive.h"
#include "../content/screen_primitives/ScreenPrimitive.h"
#include "../content/polyscope_primitives/CameraView.h"

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

    // draw order : by depth, ties broken by insertion order, so primitives
    // added first are drawn behind the ones added after
    std::vector<PrimitiveInSlide> getDepthSorted();

    // rank at which a primitive was added to this slide (-1 if absent)
    int orderOf(const PrimitivePtr& p) const;

    void add(PrimitiveInSlide pis);

    void remove(PrimitivePtr ptr);

    TextualPrimitivePtr title_primitive = nullptr;
    CameraViewPtr camera = nullptr;

    std::map<ScreenPrimitivePtr,StateInSlide> getScreenPrimitives() const;

    std::map<PolyscopePrimitivePtr,StateInSlide> getPolyscopePrimitives() const;


    std::string getTitle() const;

    void setCam() const;

    bool sameCamera(const Slide& other) const;

    TimeTypeSec pause_duration = 0;

private:
    std::map<PrimitivePtr,int> insertion_order;
    int insertion_counter = 0;
};

}

#endif // SLIDE_H
