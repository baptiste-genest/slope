#include "content/polyscope_primitives/LiveTransform.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <set>

namespace slope {

namespace {

// a name that gives nothing usable is said once, and the field keeps its default
bool readNamed(const std::string& name, const char* field,
               std::initializer_list<int> sizes, Snippet::Value& out)
{
    out = Snippet::get(name);
    if (std::find(sizes.begin(), sizes.end(), out.n) != sizes.end())
        return true;
    static std::set<std::string> said;
    // sections have not run before the first frame
    if (Snippet::ready() && said.insert(std::string(field) + ":" + name).second) {
        if (!out.valid())
            spdlog::error("transform {} : \"{}\" gives no value", field, name);
        else
            spdlog::error("transform {} : \"{}\" gives {} numbers", field, name, out.n);
    }
    return false;
}

vec readVec(const LiveVec& l, const char* field, const vec& def)
{
    if (!l.live())
        return l.fixed;
    Snippet::Value v;
    return readNamed(l.snippet, field, {3}, v) ? v.v3() : def;
}

}

Transform LiveTransform::value() const
{
    vec s = scale.fixed;
    if (scale.live()) {
        Snippet::Value v;
        if (!readNamed(scale.snippet, "scale", {1, 3}, v))
            s = vec(1,1,1);
        else
            s = v.n == 1 ? vec(v.v[0], v.v[0], v.v[0]) : v.v3();
    }

    scalar degrees = angle.fixed;
    if (angle.live()) {
        Snippet::Value v;
        degrees = readNamed(angle.snippet, "angle", {1}, v) ? v.num() : 0;
    }

    vec a = readVec(axis, "axis", vec::UnitZ());
    scalar th = degrees * M_PI / 180.;
    // a zero axis has no direction to turn about
    if (a.norm() < 1e-9) {
        a = vec::UnitZ();
        th = 0;
    }
    return Transform::ScalePositionRotate(s, readVec(pos, "pos", vec::Zero()), a, th);
}

Transform LiveTransform::within(const Transform& parent) const
{
    const Transform T = value();
    // an identity parent, kept exact rather than decomposed
    if (parent.angle == 0 && parent.translation == glm::vec3(0) && parent.scale == glm::vec3(1))
        return T;
    Transform R;
    R.fromGLMMat4(parent.getMatrix() * T.getMatrix());
    return R;
}

}
