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

// Moves a position given in screen units.
using RelativePlacer = std::function<vec2(vec2)>;

// How a primitive is placed and shown in one slide.
struct StateInSlide {
    // Opacity between 0 and 1.
    scalar alpha = 1;
    // Moves the position of the anchor.
    RelativePlacer placer = [](const vec2& p) {
        return p;
    };
    // Where the primitive is attached.
    AnchorPtr anchor = GlobalAnchor;
    // Rotation in radians.
    scalar angle = 0;
    scalar scale = 1;
    // True when the placer holds a user offset, so transitions do not add theirs.
    bool offseted = false;
    // Added after the placer.
    vec2 shift = vec2::Zero();

    // Transform of a scene primitive.
    Transform LocalToWorld;
    // A screen primitive reads it as the plane it is pasted on.
    PersistentTransform persistentTransform;
    // Transform of a scene primitive, evaluated every frame inside the frame of the label.
    std::optional<LiveTransform> liveTransform;

    // Plane on which a screen primitive is pasted.
    PlanePlacement plane;

    // True when the primitive is pasted on a plane.
    bool hasPlane() const { return plane.active(persistentTransform); }
    // Transform of the plane, empty when there is none.
    std::optional<Transform> planeTransform() const { return plane.resolve(persistentTransform); }

    // When true, updaterOverride runs instead of the updater of the primitive.
    bool updaterOverrided = false;
    Updater updaterOverride;

    StateInSlide() {
    }

    // State attached to an anchor.
    StateInSlide(AnchorPtr p) : anchor(p) {}

    // Transform of a scene primitive, from the label if one is used.
    Transform getLocalToWorld() const {
        Transform T = persistentTransform.isActive() ? persistentTransform.readFromLabel()
                                                     : LocalToWorld;
        return liveTransform ? liveTransform->within(T) : T;
    }

    // State placed at a fixed screen position.
    StateInSlide(const vec2& x) {
        anchor = AbsoluteAnchor::Add(x);
    }

    // State of a scene primitive with a transform.
    StateInSlide(const Transform& T) {
        LocalToWorld = T;
    }

    // Replaces the placement by a shift of x.
    void setOffset(const vec2& x) {
        offseted = true;
        placer = [x](const vec2& p) {
            return p + x;
        };
    }

    // Adds x to the current placement.
    void addOffset(const vec2& x) {
        auto old = placer;
        offseted = true;
        placer = [old, x](const vec2& p) {
            return old(p) + x;
        };
    }

    // Screen position of the primitive, in screen units.
    vec2 getPosition() const {
        return placer(anchor->getPos()) + shift;
    }

    // Scale of the state, multiplied by the scale set on a persistent anchor.
    scalar getScale() const {
        if (anchor->isPersistent())
            return anchor->getScale() * scale;
        return scale;
    }

    // Angle of the state, added to the angle set on a persistent anchor.
    scalar getAngle() const {
        if (anchor->isPersistent())
            return anchor->getAngle() + angle;
        return angle;
    }

    // Opacity of the state, multiplied by the opacity set on a persistent anchor.
    scalar getAlpha() const {
        if (anchor->isPersistent())
            return anchor->getAlpha() * alpha;
        return alpha;
    }

    // Position in pixels inside the current ImGui window.
    ImVec2 getAbsolutePosition() const {
        vec2 P = getPosition();
        auto W = ImGui::GetWindowSize();
        return ImVec2(P(0) * W.x, P(1) * W.y);
    }
};

using StatePtr = std::shared_ptr<StateInSlide>;

} // namespace slope

#endif // STATEINSLIDE_H
