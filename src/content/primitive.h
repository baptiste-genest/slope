#ifndef PRIMITIVE_H
#define PRIMITIVE_H

#include "../libslope.h"
#include "../math/kernels.h"
#include "TimeObject.h"
#include "Options.h"
#include <fstream>
#include <iostream>
#include "io.h"
#include "transitions.h"

namespace slope {

using PrimitiveInSlide = std::pair<PrimitivePtr,StateInSlide>;

struct Primitive {

    using Size = vec2;
    using Updater = std::function<void(TimeObject,Primitive*)>;

    Updater updater = [] (TimeObject,Primitive*) {};

    PrimitiveID pid;
    static std::vector<PrimitivePtr> primitives;

    virtual bool isScreenSpace() const = 0;

    static void addPrimitive(PrimitivePtr ptr) {
        ptr->pid = primitives.size();
        primitives.push_back(ptr);
    }

    static PrimitivePtr get(PrimitiveID id){
        return primitives[id];
    }

    template<class T>
    static std::shared_ptr<T> get(PrimitiveID id){
        return std::static_pointer_cast<T>(primitives[id]);
    }

    index relativeSlideIndex(index in) {
        return in - first_slide_to_appear;
    }

    void handleInnerTime() {
        inner_time = Time::now();
    }

    static int updater_framerate;

    void play(const TimeObject& t,const StateInSlide& sis) {
        enable();
        auto it = t(this);
        static int frame_count = 0;
        frame_count %= updater_framerate;
        if (frame_count == 0)
            updater(it,this);
        frame_count++;
        draw(it,sis);
    }

    void intro(const TimeObject& t,const StateInSlide& sis) {
        enable();
        auto it = t(this);
        playIntro(it,transition.intro(it,sis));
    }

    void outro(const TimeObject& t,const StateInSlide& sis) {
        auto it = t(this);
        playOutro(it,transition.outro(it,sis));
    }

    bool isEnabled() const {return enabled;}


    void disable() {
        if (!enabled)
            return;
        enabled = false;
        forceDisable();
    }

    void enable() {
        if (enabled)
            return;
        enabled = true;
        forceEnable();
        handleInnerTime();
    }

    TimeTypeSec getInnerTime(){
        return TimeFrom(inner_time);
    }


    bool exclusive = false;
    bool isExclusive() const {
        return exclusive;
    }

    virtual void initPolyscope() {}

    void setDepth(int d) {depth = d;}
    int getDepth() const {return depth;}

    TransitionAnimator transition;
    static TransitionAnimator DefaultTransition;

    void upFirstSlideNumber(int f) {
        first_slide_to_appear = std::min(first_slide_to_appear,f);
    }

protected:
    virtual void draw(const TimeObject& time,const StateInSlide& sis) = 0;
    virtual void playIntro(const TimeObject& t,const StateInSlide& sis) = 0;
    virtual void playOutro(const TimeObject& t,const StateInSlide& sis) = 0;
    virtual void forceDisable() {};
    virtual void forceEnable() {};

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
    ptr->initPolyscope();
    ptr->transition = Primitive::DefaultTransition;
    return ptr;
}


}

#endif // PRIMITIVE_H
