#ifndef STATEINSLIDE_H
#define STATEINSLIDE_H
#include "libslope.h"
#include "content/core/TimeObject.h"
#include "content/config/io.h"
#include "content/config/Options.h"
#include "content/screen_primitives/layout/Anchor.h"

#include "content/polyscope_primitives/Transform.h"
#include "content/polyscope_primitives/LiveTransform.h"
#include "content/screen_primitives/gpu/PlaneWarp.h"
#include <optional>

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
    // added after the placer, so it composes with any placement
    vec2 shift = vec2::Zero();

    Transform LocalToWorld;
    // a screen primitive reads persistentTransform as the plane it is pasted on
    PersistentTransform persistentTransform;
    // a scene primitive's transform, evaluated every frame inside the label's frame
    std::optional<LiveTransform> liveTransform;

    PlanePlacement plane;

    bool hasPlane() const {return plane.active(persistentTransform);}
    std::optional<Transform> planeTransform() const {return plane.resolve(persistentTransform);}

    bool updaterOverrided = false;
    Updater updaterOverride;

    StateInSlide() {
    }

    StateInSlide(AnchorPtr p) : anchor(p) {}

    Transform getLocalToWorld() const {
        Transform T = persistentTransform.isActive() ? persistentTransform.readFromLabel()
                                                     : LocalToWorld;
        return liveTransform ? liveTransform->within(T) : T;
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
        return placer(anchor->getPos()) + shift;
    }

    scalar getScale() const {
        // the wheel-editable anchor scale composes with the one asked for here
        if (anchor->isPersistent())
            return anchor->getScale() * scale;
        return scale;
    }

    // rotations add up where scales multiply
    scalar getAngle() const {
        if (anchor->isPersistent())
            return anchor->getAngle() + angle;
        return angle;
    }

    // at("label",0.5) already means half opacity, so the edited value scales it
    scalar getAlpha() const {
        if (anchor->isPersistent())
            return anchor->getAlpha() * alpha;
        return alpha;
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
