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

    static void addPrimitive(PrimitivePtr ptr);

    static PrimitivePtr get(PrimitiveID id);

    template<class T>
    static std::shared_ptr<T> get(PrimitiveID id){
        return std::static_pointer_cast<T>(primitives[id]);
    }

    index relativeSlideIndex(index in);

    void handleInnerTime();

    void play(const TimeObject& t,const StateInSlide& sis);

    void intro(const TimeObject& t,const StateInSlide& sis);

    void outro(const TimeObject& t,const StateInSlide& sis);

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
    ptr->initPolyscope();
    ptr->transition = Primitive::DefaultTransition;
    return ptr;
}


}

#endif // PRIMITIVE_H
