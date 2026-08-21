#ifndef UTILS_H
#define UTILS_H
#include "libslope.h"

namespace slope {

inline std::function<scalar(scalar)> buildRangeMapper(scalar a,scalar b,scalar c,scalar d) {
    return [a,b,c,d] (scalar x) {
        return c + (d-c)*(x-a)/(b-a);
    };
}

template <typename T>
inline T lerp(const T& a,const T& b,scalar t) {
    return T(a + (b-a)*t);
}

}

#endif // UTILS_H
