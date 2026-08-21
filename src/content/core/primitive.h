#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "libslope.h"
#include "math/kernels.h"
#include "content/core/TimeObject.h"
#include "content/config/Options.h"
#include <fstream>
#include <iostream>
#include "content/config/io.h"
#include "content/core/transitions.h"

namespace slope {


using PrimitiveInSlide = std::pair<PrimitivePtr,StateInSlide>;

struct OverrideUpdater {
    PrimitiveID pid;
    Updater func;
};

struct Primitive {

    using Size = vec2;

    Updater updater = [] (TimeObject) {};

    PrimitiveID pid;
    static std::vector<PrimitivePtr> primitives;

    virtual bool isScreenSpace() const = 0;
    virtual bool isPolyscopePrimitive() const { return false;}

    static void addPrimitive(PrimitivePtr ptr);

    static PrimitivePtr get(PrimitiveID id);

    template<class T>
    static std::shared_ptr<T> get(PrimitiveID id){
        return std::static_pointer_cast<T>(primitives[id]);
    }

    index relativeSlideIndex(index in);

    void handleInnerTime();

    // backdates the clock so getInnerTime() returns t, which settles a
    // time-based animation past its intro on export
    void settleInnerTime(TimeTypeSec t) {
        inner_time = Time::now() - std::chrono::duration_cast<TimeStamp::duration>(DurationSec(t));
    }

    virtual void play(const TimeObject& t,const StateInSlide& sis);

    virtual void intro(const TimeObject& t,const StateInSlide& sis);

    virtual void outro(const TimeObject& t,const StateInSlide& sis);

    bool isEnabled() const;


    void disable();

    void enable();

    TimeTypeSec getInnerTime();


    bool exclusive = false;
    bool isExclusive() const;

    virtual void initPolyscope();

    void setDepth(int d);
    int getDepth() const;

    TransitionAnimator transition;
    static TransitionAnimator DefaultTransition;

    void upFirstSlideNumber(int f);

    // upFirstSlideNumber only ever lowers the index, so recomposing slides
    // at runtime must reset it first, or relative_frame_number keeps the
    // staging of the previous composition
    void resetFirstSlideNumber() {first_slide_to_appear = std::numeric_limits<int>::max();}

    OverrideUpdater setUpdater(const Updater& up);

protected:
    virtual void draw(const TimeObject& time,const StateInSlide& sis) = 0;
    virtual void playIntro(const TimeObject& t,const StateInSlide& sis) = 0;
    virtual void playOutro(const TimeObject& t,const StateInSlide& sis) = 0;
    virtual void forceDisable();;
    virtual void forceEnable();;

    int depth = 0;

    TimeStamp inner_time;
    bool enabled = false;
    int first_slide_to_appear = std::numeric_limits<int>::max();
};

template<class T>
static std::shared_ptr<T> DuplicatePrimitive(std::shared_ptr<T> ptr){
    auto other = std::make_shared<T>(*ptr);
    Primitive::addPrimitive(other);
    other->initPolyscope();
    return other;
}

template <class T,typename... Args>
std::shared_ptr<T> NewPrimitive(Args&& ... args){
    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    Primitive::addPrimitive(ptr);
    return ptr;
}


}

#endif // PRIMITIVE_H
