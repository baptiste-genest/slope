#ifndef KERNELS_H
#define KERNELS_H

#include "libslope.h"

namespace slope {
template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

inline scalar periodic01(scalar t){
    return std::sin(t*TAU)*0.5 + 0.5;
}

inline scalar smoothstep(scalar t){
    if (t < 0)
        return 0;
    if (t > 1)
        return 1;
    return 3*t*t - 2*t*t*t;
}

}

#endif // KERNELS_H
