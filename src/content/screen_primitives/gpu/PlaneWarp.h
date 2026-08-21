#ifndef PLANEWARP_H
#define PLANEWARP_H

#include "content/polyscope_primitives/Transform.h"
#include "content/scripting/Snippet.h"
#include <optional>

namespace slope {

struct ImageData;

// the world quad pasted onto, spanning origin +- u +- v with v down the image
struct Frame3D {
    vec origin = vec::Zero();
    vec u = vec::UnitX();
    vec v = -vec::UnitZ();
    bool double_sided = false;

    vec normal() const {return v.cross(u).normalized();}
};

// a plane rebuilt every frame from three vectors, u's length being its width
struct LivePlane {
    LiveVec origin,u,normal;
};

// nullopt while the vectors are degenerate, holding the last frame that was not
std::optional<Transform> EvalLivePlane(const LivePlane& l);

// the plane is given here, driven by snippets, or named by persistentTransform
struct PlanePlacement {
    std::optional<Transform> frame;
    std::optional<LivePlane> live;
    bool double_sided = false;

    struct End {
        std::optional<Transform> frame;
        vec2 pos = vec2(0.5,0.5);
        scalar scale = 1, angle = 0;
        bool double_sided = false;
    };
    // set only while a transition blends two different placements
    std::optional<End> from;
    scalar blend = 1;

    bool active(const PersistentTransform& id) const {return frame || live || id.isActive();}
    std::optional<Transform> resolve(const PersistentTransform& id) const {
        if (live)
            return EvalLivePlane(*live);
        return frame ? frame : id.stored();
    }
    bool blending() const {return from && blend < 1-1e-6;}
};

// height comes from the primitive, never from the transform
Frame3D FrameFromTransform(const Transform& T,scalar aspect);
Transform TransformFromFrame(const Frame3D& f);

Transform TransformFromWidth(const vec& origin,const vec& u,const vec& n);

// folds a slide's own scale and in plane spin into the placement
Transform FoldInPlane(const Transform& T,scalar k,scalar angle);

// view depth of the scene center, where a plane nobody placed yet is parked
scalar DefaultPlaneDepth();

struct CameraProjector {
    static CameraProjector Current();

    // relative [0,1]^2, y down. False at or behind the eye plane
    bool project(const vec& p,vec2& screen) const;
    vec unproject(const vec2& screen,scalar depth) const;
    scalar viewDepth(const vec& p) const {return (p - eye).dot(forward);}

    vec eye = vec::Zero();
    vec forward = vec::UnitZ();
private:
    std::array<scalar,16> M{};
    std::array<scalar,16> Minv{};
};

// the world quad that reprojects onto the pixels a screen space draw covers
Frame3D BillboardFrame(const vec2& pos,scalar half_w,scalar half_h,scalar angle,scalar depth);

// lets the editor grab a plane that was never placed right where it is drawn
void NotePlaneDrawn(const std::string& label,const Transform& T);
std::optional<Transform> LastPlaneDrawn(const std::string& label);

// subdivided so that ImGui's affine UVs stay under a pixel of error
void DrawTexturedPlane(const Frame3D& f,const ImageData& data,const RGBA& tint,scalar alpha);

// pixel extent of the projected quad, for sizing the texture
bool PlaneScreenExtent(const Frame3D& f,scalar& px_w,scalar& px_h);

}

#endif // PLANEWARP_H
