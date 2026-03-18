#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "../libslope.h"

namespace slope {

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
    return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a)
{
    return ImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a);
}

using mapping = std::function<vec(const vec&)>;

inline std::pair<vec,vec> CompleteBasis(const vec& x) {
    vec n = x.normalized();
    vec tmp(1.,0.,0.);
    if (std::abs(n[0]) > 0.999)
        tmp = vec(0.,1.,0.);
    vec a1 = n.cross(tmp);
    vec a2 = n.cross(a1);
    return {a1,a2};
}

inline vec OrthoprojAalongB(const vec& a,const vec& b) {
    return b.dot(a)*b/b.squaredNorm();
}

inline vec OrthoprojAagainstB(const vec& a,const vec& b) {
    return a - OrthoprojAalongB(a,b);
}

inline mat CrossProductMatrix(const vec& z) {
    mat M;
    M << 0, -z(2), z(1),
         z(2), 0, -z(0),
         -z(1), z(0), 0;
    return M;
}

inline mat SmallestRotation(vec x,vec y) {
    x = x.normalized(); y = y.normalized();
    vec z = x.cross(y);
    scalar c = x.dot(y);
    if (c < -0.999) return -mat::Identity();
    mat J = CrossProductMatrix(z);
    mat R = mat::Identity() + J + J*J/(1+c);
    return R;
}


inline vec slerp(vec x,vec y,scalar t) {
    scalar lx = x.norm();
    scalar ly = y.norm();
    x.normalize();
    y.normalize();
    scalar angle = std::acos(x.dot(y));
    return (x*std::sin((1-t)*angle)/std::sin(angle) + y*std::sin(t*angle)/std::sin(angle))*((1-t)*lx + t*ly);
}


}


#endif // GEOMETRY_H
