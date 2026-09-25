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

// A primitive together with its state in one slide.
using PrimitiveInSlide = std::pair<PrimitivePtr, StateInSlide>;

// Replaces the updater of a primitive by id, used when a slide overrides it.
struct OverrideUpdater {
    PrimitiveID pid;
    Updater func;
};

// Per-slide state a primitive keeps itself. transition() does not carry
// StateInSlide fields it cannot interpolate, so a cue is applied when the
// slide is composed and read back from t.slidePosition() at draw time.
struct SlideCue {
    std::function<void(int slide)> apply;
};

// Base of everything drawn, on screen or in the polyscope scene.
struct Primitive {

    using Size = vec2;

    // Called every frame after drawing.
    Updater updater = [](TimeObject) {};

    // Index of this primitive in the global list.
    PrimitiveID pid;
    // Every primitive ever created, indexed by pid.
    static std::vector<PrimitivePtr> primitives;

    // True for 2D primitives drawn over the scene.
    virtual bool isScreenSpace() const = 0;
    // True for primitives that live in the polyscope scene.
    virtual bool isPolyscopePrimitive() const { return false; }

    // Registers ptr in the global list and gives it its pid.
    static void addPrimitive(PrimitivePtr ptr);

    // Returns the primitive with this pid.
    static PrimitivePtr get(PrimitiveID id);

    // Same as get, cast to T without checking the type.
    template <class T>
    static std::shared_ptr<T> get(PrimitiveID id) {
        return std::static_pointer_cast<T>(primitives[id]);
    }

    // Number of slides between the first appearance of this primitive and slide `in`.
    index relativeSlideIndex(index in);

    // Restarts the inner clock.
    void handleInnerTime();

    // Sets the inner clock so getInnerTime() returns t, used to skip the intro on export.
    void settleInnerTime(TimeTypeSec t) {
        inner_time = Time::now() - std::chrono::duration_cast<TimeStamp::duration>(DurationSec(t));
    }

    // Draws the primitive during a slide.
    virtual void play(const TimeObject& t, const StateInSlide& sis);

    // Draws the primitive while it appears.
    virtual void intro(const TimeObject& t, const StateInSlide& sis);

    // Draws the primitive while it disappears.
    virtual void outro(const TimeObject& t, const StateInSlide& sis);

    // True when the primitive is currently shown.
    bool isEnabled() const;

    // Hides the primitive.
    void disable();

    // Shows the primitive.
    void enable();

    // Seconds since the primitive was enabled.
    TimeTypeSec getInnerTime();

    // When true, the primitive is the slide title and replaces the previous title.
    bool exclusive = false;
    // True when the primitive is a slide title.
    bool isExclusive() const;

    // Creates the polyscope objects of the primitive. Does nothing for screen primitives.
    virtual void initPolyscope();

    // Sets the drawing order. A larger depth is drawn on top.
    void setDepth(int d);
    // Current drawing depth.
    int getDepth() const;

    // How the primitive appears and disappears.
    TransitionAnimator transition;
    // Transition used by primitives that did not set their own.
    static TransitionAnimator DefaultTransition;

    // Keeps the smallest slide index where the primitive appears.
    void upFirstSlideNumber(int f);

    // Forgets the first slide of appearance. Needed before recomposing slides at runtime.
    void resetFirstSlideNumber() { first_slide_to_appear = std::numeric_limits<int>::max(); }

    // Builds an updater that replaces this primitive's one for a single slide.
    OverrideUpdater setUpdater(const Updater& up);

protected:
    // Draws the primitive once, in the normal state.
    virtual void draw(const TimeObject& time, const StateInSlide& sis) = 0;
    // Draws one frame of the appearance.
    virtual void playIntro(const TimeObject& t, const StateInSlide& sis) = 0;
    // Draws one frame of the disappearance.
    virtual void playOutro(const TimeObject& t, const StateInSlide& sis) = 0;
    // Hides or shows the primitive with no bookkeeping.
    virtual void forceDisable();
    // Same for showing.
    virtual void forceEnable();

    int depth = 0;

    TimeStamp inner_time;
    bool enabled = false;
    int first_slide_to_appear = std::numeric_limits<int>::max();
};

// Copies a primitive and registers the copy.
template <class T>
static std::shared_ptr<T> DuplicatePrimitive(std::shared_ptr<T> ptr) {
    auto other = std::make_shared<T>(*ptr);
    Primitive::addPrimitive(other);
    other->initPolyscope();
    return other;
}

// Builds a primitive and registers it.
template <class T, typename... Args>
std::shared_ptr<T> NewPrimitive(Args&&... args) {
    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    Primitive::addPrimitive(ptr);
    return ptr;
}

} // namespace slope

#endif // PRIMITIVE_H
