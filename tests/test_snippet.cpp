// the Lua snippet system, headless. Sections, dictionaries, callables, complex
// arithmetic, C++ derivations, error resilience and hot reload
#include "slope.h"
#include "content/Snippet.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
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

#define CHECK_NEAR(a, b)                                                     \
    do {                                                                     \
        double va = (a), vb = (b);                                           \
        if (std::abs(va - vb) > 1e-9) {                                      \
            std::cerr << "CHECK FAILED: " #a " == " #b " (" << va << " vs "  \
                      << vb << ") at " << __FILE__ << ":" << __LINE__        \
                      << std::endl;                                          \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static fs::path dir;

static void write(const std::string& body)
{
    std::ofstream f(dir / "snippets.lua");
    f << body;
}

// hot reload watches mtime, whose granularity is a few ms on most filesystems :
// a human editing a file is never this fast, a test is
static void rewrite(const std::string& body)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    write(body);
    Snippet::HotReloadIfModified();
}

static TimeObject at(double inner, int slide = 0)
{
    TimeObject t;
    t.from_begin = float(inner);
    t.inner_time = float(inner);   // C++ derivations still see the whole object
    t.from_action = float(inner);
    t.absolute_frame_number = slide;
    return t;
}

int main()
{
    dir = fs::temp_directory_path() / "slope_snippet_test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    Options::ProjectDataPath = dir.string() + "/";
    Options::ProjectViewsPath = dir.string() + "/";

    write(R"(
--- envelope
return math.sin(t.from_begin)

--- lattice
local z1 = complex(1, t.from_begin)
return { z1 = z1, z2 = complex(0, 1), tau = complex(0, 1) / z1 }

--- shifted
return envelope + 10

--- wobble
return function(p, i)
  return p + vec3(0, 0, envelope * i)
end

--- pt
return vec2(3, 4)
)");

    Snippet::load("snippets.lua");

    // ── a value section ─────────────────────────────────────────────────────
    Snippet::setTime(at(0.5));
    CHECK_NEAR((scalar)Snippet::get("envelope"), std::sin(0.5));

    // ── a dictionary section publishes one variable per key ─────────────────
    vec2 z1 = Snippet::get("z1");
    CHECK_NEAR(z1(0), 1.0);
    CHECK_NEAR(z1(1), 0.5);
    vec2 z2 = Snippet::get("z2");
    CHECK_NEAR(z2(0), 0.0);
    CHECK_NEAR(z2(1), 1.0);

    // tau = i / (1 + 0.5i) = (0.5 + i) / 1.25, complex division not per-component
    vec2 tau = Snippet::get("tau");
    CHECK_NEAR(tau(0), 0.5 / 1.25);
    CHECK_NEAR(tau(1), 1.0 / 1.25);

    // ── sections see each other by bare name, in any order ──────────────────
    CHECK_NEAR((scalar)Snippet::get("shifted"), std::sin(0.5) + 10);

    // ── a vec2 section, and the conversions ─────────────────────────────────
    vec2 p = Snippet::get("pt");
    CHECK_NEAR(p(0), 3.0);
    CHECK_NEAR(p(1), 4.0);
    CHECK((int)Snippet::get("pt") == 3);
    CHECK(Snippet::get("pt").valid());
    CHECK(!Snippet::get("no_such_name").valid());
    CHECK_NEAR((scalar)Snippet::get("no_such_name"), 0.0);

    // ── a callable section ──────────────────────────────────────────────────
    auto wobble = Snippet::fn<vec(vec, int)>("wobble");
    CHECK(wobble.valid());
    vec r = wobble(vec(1, 2, 3), 2);
    CHECK_NEAR(r(0), 1.0);
    CHECK_NEAR(r(1), 2.0);
    CHECK_NEAR(r(2), 3.0 + 2 * std::sin(0.5));

    // it follows the clock like everything else
    Snippet::setTime(at(1.5));
    r = wobble(vec(0, 0, 0), 1);
    CHECK_NEAR(r(2), std::sin(1.5));
    CHECK_NEAR((scalar)Snippet::get("envelope"), std::sin(1.5));

    // ── memoization, one evaluation per frame ───────────────────────────────
    rewrite(R"(
--- counter
count = (count or 0) + 1
return count
)");
    Snippet::setTime(at(0));
    scalar first = Snippet::get("counter");
    scalar again = Snippet::get("counter");
    CHECK_NEAR(first, again);
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("counter"), first + 1);

    // ── C++ derivations, and changed() ──────────────────────────────────────
    int calls = 0;
    Snippet::derive("g2", std::function<scalar(const TimeObject&)>(
        [&calls](const TimeObject& t) { calls++; return 2 * t.inner_time; }));
    Snippet::setTime(at(3));
    CHECK_NEAR((scalar)Snippet::get("g2"), 6.0);
    CHECK_NEAR((scalar)Snippet::get("g2"), 6.0);
    CHECK(calls == 1);                       // lazy and memoized per frame

    long g = Snippet::changed("g2");
    Snippet::setTime(at(3));
    CHECK(Snippet::changed("g2") == g);      // same value, same generation
    Snippet::setTime(at(4));
    CHECK(Snippet::changed("g2") == g + 1);  // it moved

    // ── dirty(), the call site is the identity, no counter to hold ──────────
    // each lambda is one call site, asked repeatedly -- which is what a guard
    // inside an updater is. Two spellings on two lines are two consumers.
    auto probe = [] { return Snippet::dirty("g2"); };
    auto other = [] { return Snippet::dirty("g2"); };

    Snippet::setTime(at(6));
    CHECK(probe());                 // first visit always fires
    Snippet::setTime(at(6));
    CHECK(!probe());                // same value, quiet
    Snippet::setTime(at(7));
    CHECK(probe());                 // it moved

    // a second consumer of the same variable has its own state, and neither
    // one clears the other's
    Snippet::setTime(at(7));
    CHECK(!probe());                // probe has already seen this value
    CHECK(other());                 // ...this one has not
    Snippet::setTime(at(7));
    CHECK(!probe());
    CHECK(!other());

    // a tag separates two guards that share a line
    Snippet::setTime(at(8));
    CHECK(Snippet::dirty("g2", "left") && Snippet::dirty("g2", "right"));

    // several names, dirty when *any* of them moves. counter climbs every
    // frame, so this pair is never quiet...
    auto pair_busy = [] { return Snippet::dirty({"g2", "counter"}); };
    Snippet::setTime(at(9));
    CHECK(pair_busy());
    Snippet::setTime(at(9));
    CHECK(pair_busy());

    // ...while an unknown name simply never moves, and does not wedge the guard
    auto pair_quiet = [] { return Snippet::dirty({"g2", "no_such_var"}); };
    Snippet::setTime(at(10));
    CHECK(pair_quiet());
    Snippet::setTime(at(10));
    CHECK(!pair_quiet());

    // ── derive with dependencies, the cache is kept for you ─────────────────
    int sums = 0;
    Snippet::derive("heavy", {"g2"}, std::function<scalar(const TimeObject&)>(
        [&sums](const TimeObject& t) { sums++; return 10 * t.inner_time; }));
    Snippet::setTime(at(11));
    CHECK_NEAR((scalar)Snippet::get("heavy"), 110.0);
    CHECK(sums == 1);
    Snippet::setTime(at(11));
    CHECK_NEAR((scalar)Snippet::get("heavy"), 110.0);
    CHECK(sums == 1);                  // g2 did not move, not re-summed
    Snippet::setTime(at(12));
    CHECK_NEAR((scalar)Snippet::get("heavy"), 120.0);
    CHECK(sums == 2);                  // g2 moved, recomputed once

    // a derivation is visible to Lua by name, like any other variable
    rewrite(R"(
--- doubled
return g2 * 2
)");
    Snippet::setTime(at(5));
    CHECK_NEAR((scalar)Snippet::get("doubled"), 20.0);

    // ── a broken snippet must not take the show down ────────────────────────
    rewrite(R"(
--- good
return 42

--- bad
return nonexistent_thing.field

--- worse
this is not lua at all
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("good"), 42.0);   // its neighbours still work
    CHECK(!Snippet::get("bad").valid());
    CHECK(!Snippet::ok());
    CHECK(!Snippet::lastError().empty());

    // a call into a broken snippet returns the fallback, every time
    auto broken = Snippet::fn<scalar(scalar)>("bad", -1.0);
    CHECK_NEAR(broken(1), -1.0);
    CHECK_NEAR(broken(2), -1.0);

    // ── hot reload picks the file back up ───────────────────────────────────
    rewrite(R"(
--- good
return 43
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("good"), 43.0);

    // ── the call handle survives a reload ───────────────────────────────────
    rewrite(R"(
--- scale
return function(x) return x * 3 end
)");
    Snippet::setTime(at(0));
    auto scale = Snippet::fn<scalar(scalar)>("scale", 0.0);
    CHECK_NEAR(scale(2), 6.0);

    rewrite(R"(
--- scale
return function(x) return x * 5 end
)");
    Snippet::setTime(at(0));
    CHECK_NEAR(scale(2), 10.0);   // same handle, new body

    // ── a dictionary holding a numeric key ──────────────────────────────────
    // reading such a key as a string would convert it in place and derail the
    // walk, which the Lua API answers with a panic, i.e. the show going down
    rewrite(R"(
--- mixed
local d = { a = 1 }
d[3] = 7
return d
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("a"), 1.0);

    // ── order in the file does not matter, and costs no false error ─────────
    rewrite(R"(
--- aaa
return zzz + 1

--- zzz
return 10
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("aaa"), 11.0);
    CHECK(Snippet::ok());          // zzz had simply not run yet, not an error

    // ── a syntax error keeps the chunk that worked ──────────────────────────
    rewrite(R"(
--- kept
return 11
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("kept"), 11.0);

    rewrite(R"(
--- kept
return 11 +
)");
    Snippet::setTime(at(0));
    CHECK_NEAR((scalar)Snippet::get("kept"), 11.0);   // saved mid-typing
    CHECK(!Snippet::ok());                            // and said so

    // ...while a section actually removed from the file does go
    rewrite(R"(
--- other
return 1
)");
    Snippet::setTime(at(0));
    CHECK(!Snippet::get("kept").valid());

    // ── a runtime error freezes the section's values ────────────────────────
    rewrite(R"(
--- frozen
if t.from_begin > 1.5 then return missing.field end
return 5
)");
    Snippet::setTime(at(1));
    CHECK_NEAR((scalar)Snippet::get("frozen"), 5.0);
    Snippet::setTime(at(2));
    CHECK_NEAR((scalar)Snippet::get("frozen"), 5.0);   // held, not zeroed

    // ── the keyframe helpers read the same dotted or called as a method ─────
    static const std::map<std::string, int> kf{{"a", 0}, {"b", 1}};
    TimeObject::keyframes = &kf;
    rewrite(R"(
--- dotted
return t.during("a")

--- method
return t:during("a")
)");
    Snippet::setTime(at(0));
    CHECK((scalar)Snippet::get("method") > 0);
    CHECK_NEAR((scalar)Snippet::get("dotted"), (scalar)Snippet::get("method"));
    TimeObject::keyframes = nullptr;

    // ── the per-call cost, for the record ───────────────────────────────────
    // trivial vs realistic, to separate the C -> Lua boundary from the body
    rewrite(R"(
--- nop
return function(x) return x end

--- displace
return function(p, i)
  return p + vec3(0, 0, math.sin(p.x * 10 + t.from_begin))
end

--- displace_flat
return function(p, i)
  return p.x, p.y, p.z + math.sin(p.x * 10 + t.from_begin)
end
)");
    Snippet::setTime(at(1));
    {
        auto nop = Snippet::fn<scalar(scalar)>("nop");
        const int M = 200000;
        auto a0 = std::chrono::steady_clock::now();
        scalar s = 0;
        for (int i = 0; i < M; i++) s += nop(1.0);
        auto a1 = std::chrono::steady_clock::now();
        std::cout << "boundary only: "
                  << std::chrono::duration<double, std::nano>(a1 - a0).count() / M
                  << " ns/call" << std::endl;
        CHECK_NEAR(s, scalar(M));
    }

    Snippet::setTime(at(1));
    auto displace = Snippet::fn<vec(vec, int)>("displace");
    const int N = 100000;
    auto t0 = std::chrono::steady_clock::now();
    vec acc = vec::Zero();
    for (int i = 0; i < N; i++)
        acc += displace(vec(i * 1e-5, 0, 0), i);
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::cout << "vec3 in/out:   " << ns << " ns/call  (" << N << " calls, "
              << ns * N / 1e6 << " ms total)" << std::endl;
    CHECK(acc.norm() >= 0);   // keep the loop

    // the same body returning three numbers, no userdata built for the result
    auto flat = Snippet::fn<vec(vec, int)>("displace_flat");
    auto t2 = std::chrono::steady_clock::now();
    vec acc2 = vec::Zero();
    for (int i = 0; i < N; i++)
        acc2 += flat(vec(i * 1e-5, 0, 0), i);
    auto t3 = std::chrono::steady_clock::now();
    double ns2 = std::chrono::duration<double, std::nano>(t3 - t2).count() / N;
    std::cout << "vec3 in, 3 out: " << ns2 << " ns/call  (" << N << " calls, "
              << ns2 * N / 1e6 << " ms total)" << std::endl;
    CHECK_NEAR(acc(2), acc2(2));   // and the same answer

    fs::remove_all(dir);
    if (failures == 0)
        std::cout << "all snippet checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
