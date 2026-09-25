#ifndef EASING_H
#define EASING_H

#include "libslope.h"
#include <string_view>

namespace slope {

// Maps a progress in [0,1] to an eased progress.
using RateFunc = scalar (*)(scalar);

namespace rate {

// Easing functions.
scalar linear(scalar t);
scalar smooth(scalar t);
scalar smooth_squared(scalar t);
scalar smoother(scalar t);
scalar rush_into(scalar t);
scalar rush_from(scalar t);
scalar ease_out_back(scalar t);
scalar ease_out_elastic(scalar t);
// Goes from 0 to 1 and back to 0.
scalar there_and_back(scalar t);

} // namespace rate

// Returns the easing with this name.
RateFunc RateFromName(std::string_view name);

// Returns the name of an easing.
const char* NameOfRate(RateFunc f);

} // namespace slope

#endif // EASING_H
