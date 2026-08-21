#ifndef EASING_H
#define EASING_H

#include "libslope.h"
#include <string_view>

namespace slope {

using RateFunc = scalar (*)(scalar);

namespace rate {

scalar linear(scalar t);
scalar smooth(scalar t);
scalar smooth_squared(scalar t);
scalar smoother(scalar t);
scalar rush_into(scalar t);
scalar rush_from(scalar t);
scalar ease_out_back(scalar t);
scalar ease_out_elastic(scalar t);
scalar there_and_back(scalar t);

} // namespace rate

RateFunc RateFromName(std::string_view name);

const char *NameOfRate(RateFunc f);

} // namespace slope

#endif // EASING_H
