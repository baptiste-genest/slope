#ifndef UTILS_H
#define UTILS_H
#include "libslope.h"

namespace slope {

// Returns the affine map sending [a,b] to [c,d].
inline std::function<scalar(scalar)> buildRangeMapper(scalar a,scalar b,scalar c,scalar d) {
    return [a,b,c,d] (scalar x) {
        return c + (d-c)*(x-a)/(b-a);
    };
}

// Linear interpolation, giving a at t=0 and b at t=1.
template <typename T>
inline T lerp(const T& a,const T& b,scalar t) {
    return T(a + (b-a)*t);
}

// Centers the bounding box on the origin and scales its longest side to 1.
inline void normalizeToUnitCube(vecs& X) {
    if (X.empty())
        return;
    vec lo = X[0], hi = X[0];
    for (const auto& x : X) {
        lo = lo.cwiseMin(x);
        hi = hi.cwiseMax(x);
    }
    const vec center = 0.5*(lo + hi);
    const scalar extent = (hi - lo).maxCoeff();
    for (auto& x : X)
        x = extent > 0 ? vec((x - center)/extent) : vec(x - center);
}

}

#endif // UTILS_H
