// The keyframe queries of a TimeObject : when the boolean ones step across a
// slide change, and that they read the same whether a primitive is settled,
// arriving or leaving.
#include "slope.h"
#include "content/core/TimeObject.h"

#include <iostream>
#include <map>

using namespace slope;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK FAILED: " #cond " at " << __FILE__ << ":"    \
                      << __LINE__ << std::endl;                              \
            failures++;                                                      \
        }                                                                    \
    } while (0)

// the show sitting `progress` into the change from slide n-1 into slide n
static TimeObject at(int n, parameter progress)
{
    TimeObject t;
    t.absolute_frame_number = n;
    t.slide_progress = progress;
    t.transition_parameter = progress;
    return t;
}

int main()
{
    static const std::map<std::string, int> kf{{"a", 2}, {"b", 3}};
    TimeObject::keyframes = &kf;

    // ── the step lands at the midpoint, where the primitives are swapped ──
    CHECK(at(3, 0.0).shownFrame() == 2);
    CHECK(at(3, 0.25).shownFrame() == 2);
    CHECK(at(3, 0.75).shownFrame() == 3);
    CHECK(at(3, 1.0).shownFrame() == 3);

    // so the slide being left still answers for the first half of the change
    CHECK(at(3, 0.25).atKeyframe("a"));
    CHECK(!at(3, 0.25).atKeyframe("b"));
    CHECK(at(3, 0.25).beforeKeyframe("b"));
    CHECK(!at(3, 0.25).afterKeyframe("b"));
    CHECK(at(3, 0.25).slidesSinceKeyframe("a") == 0);

    // and the arriving one from the midpoint on
    CHECK(at(3, 0.75).atKeyframe("b"));
    CHECK(at(3, 0.75).afterKeyframe("b"));
    CHECK(!at(3, 0.75).beforeKeyframe("b"));
    CHECK(at(3, 0.75).slidesSinceKeyframe("a") == 1);

    // ── settled is unambiguous, whichever slide ──────────────────────────
    for (int n = 0; n < 5; n++)
        CHECK(at(n, 1.0).shownFrame() == n);

    // ── an unknown name is never reached, in either direction ────────────
    CHECK(!at(3, 1.0).afterKeyframe("nope"));
    CHECK(!at(3, 1.0).beforeKeyframe("nope"));
    CHECK(at(3, 1.0).slidesSinceKeyframe("nope") == TimeObject::keyframe_unreached);

    // ── the queries do not depend on which path draws the primitive ──────
    // Primitive::intro and ::outro overwrite transition_parameter with the
    // primitive's own half-ramp, which used to drag the keyframe queries with
    // it. slide_progress is the deck wide reading and stays put.
    for (parameter p : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const TimeObject settled = at(3, p);
        TimeObject arriving = settled;   // second half of the change, 0 -> 1
        arriving.transition_parameter = 2*p - 1;
        TimeObject leaving = settled;    // first half, 0 -> 1
        leaving.transition_parameter = 2*p;

        CHECK(arriving.shownFrame() == settled.shownFrame());
        CHECK(leaving.shownFrame() == settled.shownFrame());
        CHECK(arriving.slidePosition() == settled.slidePosition());
        CHECK(leaving.slidePosition() == settled.slidePosition());
        CHECK(arriving.duringKeyframe("b") == settled.duringKeyframe("b"));
        CHECK(leaving.sinceKeyframe("a") == settled.sinceKeyframe("a"));
    }

    // ── slidePosition slides, it does not jump ───────────────────────────
    CHECK(at(3, 0.0).slidePosition() == 2);
    CHECK(at(3, 1.0).slidePosition() == 3);
    // neighbouring windows sum to 1 across the change, so a blend of the two
    // states is continuous
    for (parameter p : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const TimeObject t = at(3, p);
        CHECK(std::abs(t.duringKeyframe("a") + t.duringKeyframe("b") - 1) < 1e-6);
    }

    std::cout << (failures ? "FAILED" : "ok") << std::endl;
    return failures ? 1 : 0;
}
