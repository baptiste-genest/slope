#ifndef STATEINSLIDE_H
#define STATEINSLIDE_H
#include "../libslope.h"
#include "io.h"
#include "Options.h"
#include "Anchor.h"

#include "Transform.h"

namespace slope {

using RelativePlacer = std::function<vec2(vec2)>;

struct StateInSlide {
    scalar alpha = 1;
    RelativePlacer placer = [] (const vec2& p) {
        return p;
    };
    AnchorPtr anchor = GlobalAnchor;
    scalar angle=0;
    scalar scale = 1;
    bool offseted = false;

    Transform LocalToWorld;
    PersistentTransform persistentTransform;

    StateInSlide() {
    }

    StateInSlide(AnchorPtr p) : anchor(p) {}

    Transform getLocalToWorld() const {
        if (persistentTransform.isActive()) {
            Transform T = persistentTransform.readFromLabel();
            return T;
        }
        return LocalToWorld;
    }

    StateInSlide(const vec2& x)  {
        anchor = AbsoluteAnchor::Add(x);
    }

    StateInSlide(const Transform& T) {
        LocalToWorld = T;
    }

    void setOffset(const vec2& x) {
        offseted = true;
        placer = [x] (const vec2& p) {
            return p + x;
        };
    }

    void addOffset(const vec2& x) {
        auto old = placer;
        offseted = true;
        placer = [old,x] (const vec2& p) {
            return old(p) + x;
        };
    }


    vec2 getPosition() const {
        return placer(anchor->getPos());
    }

    scalar getScale() const {
        if (anchor->isPersistent())
            return anchor->getScale();
        return scale;
    }

    ImVec2 getAbsolutePosition() const {
        vec2 P = getPosition();
        auto W = ImGui::GetWindowSize();
        return ImVec2(P(0)*W.x,P(1)*W.y);
    }
};

using StatePtr = std::shared_ptr<StateInSlide>;


}

#endif // STATEINSLIDE_H
