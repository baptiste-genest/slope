#ifndef PLANEWARP_H
#define PLANEWARP_H

#include "content/polyscope_primitives/Transform.h"
#include "content/authoring/Snippet.h"
#include <optional>

namespace slope {

struct ImageData;

// The quad of the scene on which an image is pasted. It spans origin +- u +- v, with v going down the image.
struct Frame3D {
    vec origin = vec::Zero();
    vec u = vec::UnitX();
    vec v = -vec::UnitZ();
    // When true, the back of the quad is drawn too.
    bool double_sided = false;

    // Unit vector perpendicular to the quad.
    vec normal() const { return v.cross(u).normalized(); }
};

// A plane rebuilt every frame from three vectors. The length of u is the width of the plane.
struct LivePlane {
    LiveVec origin, u, normal;
};

// Current transform of a live plane. While the vectors are degenerate, it returns nothing and the last valid frame is kept.
std::optional<Transform> EvalLivePlane(const LivePlane& l);

// Where a screen primitive is pasted. The plane is a fixed frame, a live plane driven by snippets,
// or the one stored under a label in persistentTransform.
struct PlanePlacement {
    std::optional<Transform> frame;
    std::optional<LivePlane> live;
    bool double_sided = false;

    // One side of a transition between two placements.
    struct End {
        std::optional<Transform> frame;
        vec2 pos = vec2(0.5, 0.5);
        scalar scale = 1, angle = 0;
        bool double_sided = false;
    };
    // Set only while a transition blends two different placements.
    std::optional<End> from;
    // Weight of this placement in the blend, from 0 to 1.
    scalar blend = 1;

    // True when a plane is defined.
    bool active(const PersistentTransform& id) const { return frame || live || id.isActive(); }
    // Current transform of the plane.
    std::optional<Transform> resolve(const PersistentTransform& id) const {
        if (live)
            return EvalLivePlane(*live);
        return frame ? frame : id.stored();
    }
    // True while a transition blends two placements.
    bool blending() const { return from && blend < 1 - 1e-6; }
};

// Builds the quad of a transform. The height comes from the aspect ratio of the primitive, never from the transform.
Frame3D FrameFromTransform(const Transform& T, scalar aspect);
// Builds the transform of a quad.
Transform TransformFromFrame(const Frame3D& f);

// Builds a transform from the origin, the vector u along the width and the normal n.
Transform TransformFromWidth(const vec& origin, const vec& u, const vec& n);

// Includes the scale of the slide and a rotation inside the plane into a transform.
Transform FoldInPlane(const Transform& T, scalar k, scalar angle);

// View depth of the center of the scene, where a plane that was never placed is put.
scalar DefaultPlaneDepth();

// Converts between points of the scene and screen positions.
struct CameraProjector {
    // Projector of the current camera.
    static CameraProjector Current();

    // Gives the relative screen position of a point, with y down. Returns false for a point at or behind the eye plane.
    bool project(const vec& p, vec2& screen) const;
    // Point of the scene at a screen position and a view depth.
    vec unproject(const vec2& screen, scalar depth) const;
    // Distance of a point in front of the eye, along the view direction.
    scalar viewDepth(const vec& p) const { return (p - eye).dot(forward); }

    // Position of the eye and view direction.
    vec eye = vec::Zero();
    vec forward = vec::UnitZ();

private:
    std::array<scalar, 16> M{};
    std::array<scalar, 16> Minv{};
};

// Quad of the scene that projects onto the same pixels as a screen draw with this center, half sizes and angle.
Frame3D BillboardFrame(const vec2& pos, scalar half_w, scalar half_h, scalar angle, scalar depth);

// Remembers where a plane was drawn, so the editor can grab a plane that was never placed at the place where it appears.
void NotePlaneDrawn(const std::string& label, const Transform& T);
// Transform remembered for a label.
std::optional<Transform> LastPlaneDrawn(const std::string& label);

// Draws an image on a quad. The quad is subdivided so the affine UV of ImGui gives less than a pixel of error.
void DrawTexturedPlane(const Frame3D& f, const ImageData& data, const RGBA& tint, scalar alpha);

// Size in pixels of the projected quad, used to choose the size of the texture. Returns false when the quad is not visible.
bool PlaneScreenExtent(const Frame3D& f, scalar& px_w, scalar& px_h);

} // namespace slope

#endif // PLANEWARP_H
