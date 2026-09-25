#ifndef KERNELS_H
#define KERNELS_H

#include "libslope.h"

namespace slope {
// Sign of val, either -1, 0 or 1.
template <typename T>
int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

// Sine wave of period 1 going from 0 to 1.
inline scalar periodic01(scalar t) {
    return std::sin(t * TAU) * 0.5 + 0.5;
}

// Smooth ramp from 0 to 1, clamped outside [0,1].
inline scalar smoothstep(scalar t) {
    if (t < 0)
        return 0;
    if (t > 1)
        return 1;
    return 3 * t * t - 2 * t * t * t;
}

} // namespace slope

#endif // KERNELS_H
