#include "easing.h"
#include "kernels.h"

#include <array>
#include <cmath>

namespace slope {

namespace rate {

scalar linear(scalar t) { return std::clamp(t, 0., 1.); }

scalar smooth(scalar t) { return smoothstep(t); }

scalar smooth_squared(scalar t) {
    const scalar s = smoothstep(t);
    return s * s;
}

scalar smoother(scalar t) {
    t = std::clamp(t, 0., 1.);
    return t * t * t * (t * (t * 6 - 15) + 10);
}

scalar rush_into(scalar t) {
    t = std::clamp(t, 0., 1.);
    return smoothstep(0.5 + 0.5 * t) * 2 - 1;
}

scalar rush_from(scalar t) {
    t = std::clamp(t, 0., 1.);
    return smoothstep(0.5 * t) * 2;
}

scalar ease_out_back(scalar t) {
    t = std::clamp(t, 0., 1.);
    constexpr scalar c1 = 1.70158;
    constexpr scalar c3 = c1 + 1;
    const scalar u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}

scalar ease_out_elastic(scalar t) {
    if (t <= 0)
        return 0;
    if (t >= 1)
        return 1;
    constexpr scalar period = TAU / 3;
    return std::pow(2., -10. * t) * std::sin((t * 10 - 0.75) * period) + 1;
}

scalar there_and_back(scalar t) {
    t = std::clamp(t, 0., 1.);
    return smoothstep(1 - std::abs(2 * t - 1));
}

} // namespace rate

namespace {

struct NamedRate {
    std::string_view name;
    RateFunc func;
};

constexpr std::array<NamedRate, 8> kRates{{
    {"linear", rate::linear},
    {"smooth", rate::smooth},
    {"smooth_squared", rate::smooth_squared},
    {"smoother", rate::smoother},
    {"rush_into", rate::rush_into},
    {"rush_from", rate::rush_from},
    {"ease_out_back", rate::ease_out_back},
    {"ease_out_elastic", rate::ease_out_elastic},
}};

} // namespace

RateFunc RateFromName(std::string_view name) {
    for (const auto &r : kRates)
        if (r.name == name)
            return r.func;
    return nullptr;
}

const char *NameOfRate(RateFunc f) {
    for (const auto &r : kRates)
        if (r.func == f)
            return r.name.data();
    return "custom";
}

} // namespace slope
