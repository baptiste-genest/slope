#ifndef SNIPPET_H
#define SNIPPET_H

#include "libslope.h"
#include "content/core/TimeObject.h"
#include <array>
#include <set>
#include <source_location>

namespace slope {

/*
 * Lua snippets that reload when the file is saved. They define the shape of an
 * animation and can be edited while the show runs.
 *
 * The deck (deck.yaml), the constants (Params) and the pixel math (.frag) already reload live.
 * The logic between them is a C++ lambda and needs a rebuild. A snippet file removes that need.
 * Lua stays inside the file and everything else refers to snippets by name.
 *
 * The file
 * One .lua file, split into sections by a "--- name" line, which is a valid Lua comment.
 * What a section returns decides what it becomes.
 *
 *   --- envelope                       -- a value, one variable named like the section
 *   return math.sin(t.from_begin * speed)
 *
 *   --- lattice                        -- a table, one variable per key
 *   local z1 = complex(1, 0.5 + 0.5*math.cos(t.from_begin))
 *   return { z1 = z1, z2 = complex(0,1), tau = complex(0,1)/z1 }
 *
 *   --- wobble                         -- a function, a callable snippet
 *   return function(p, i)
 *     return p + vec3(0, 0, envelope * math.exp(-20*p:norm()^2))
 *   end
 *
 * A value, a table entry and the result of a function are read the same way.
 * They hold up to 4 numbers, given as numbers, as vec2, vec3 or complex, or as arrays of those.
 * These three are the same vec3.
 *
 *   return x, y, z          return vec3(x, y, z)          return {x, y, z}
 *
 * Any other shape is an error.
 * Reading a value with the wrong number of components gives a warning.
 * Reading a vec2 as a vec3 (z = 0) or an RGB as a color (alpha = 1) is allowed.
 *
 * A section name can be grouped with "/", so it can own a parameter that another object publishes.
 *
 *   --- fig/xrange                     -- the board reads its view from here
 *   return vec2(-3.15 + 3.75*t:sinceKeyframe("zoom"), 3.15)
 *
 * Sections see `t` (the TimeObject of the current slide), the built-ins below,
 * and every other name of the snippet namespace, which is shared with Params.
 * Reading another section evaluates it, so the order in the file does not matter.
 *
 * Reading from C++
 *   vec2   z1  = Snippet::get("z1");        // converts to scalar, int, vec2, vec or RGBA
 *   scalar env = Snippet::get("envelope");
 *
 * get() runs a section at most once per frame, on the first read.
 * A section that no slide reads never runs.
 * It runs again on every frame where it is read, even if it does not use `t`,
 * so heavy work belongs in a derive() with dependencies.
 *
 * To call a section that returns a function, get its handle once before the loop.
 * The name lookup costs more than the call.
 *
 *   auto wobble = Snippet::fn<vec(vec,int)>("wobble");   // resolved once
 *   for (size_t i = 0; i < V.size(); i++)
 *       V[i] = wobble(v0[i], int(i));
 *
 * A snippet that fails returns the fallback, which is the second argument of fn.
 * It is then skipped for the rest of the frame, so a broken snippet does not slow the show.
 *
 * C++ can also publish a variable in the same namespace, evaluated on demand.
 *
 *   Snippet::derive("g2", [](const TimeObject&) { return eisenstein(4); });
 *
 * changed() tells whether a value moved, to skip a costly computation.
 *
 *   if (Snippet::changed("z1") != last_gen) { ... }
 *
 * Feeding a shader
 *   fx->bind("z1", [] { return (vec2)Snippet::get("z1"); });
 *
 * Built-ins
 *   t          the TimeObject, with the names used by C++ and GLSL.
 *              Fields are from_begin, from_action, delta_time, absolute_frame_number,
 *              slide_progress and transition_parameter (both go from 0 to 1 during a slide change).
 *              Methods are afterKeyframe, beforeKeyframe, atKeyframe, slidesSinceKeyframe,
 *              secondsSinceKeyframe, duringKeyframe and sinceKeyframe, all taking a name,
 *              and slidePosition().
 *              from_begin is the clock that runs freely, usually the one an animation wants.
 *              A snippet has no moment of appearing, so it has no inner_time and no
 *              relative_frame_number. The shader uniforms with these names exist and are per primitive.
 *   param      param("x", def, min, max) declares a parameter with slider bounds and returns it.
 *              It is tunable in the Tuner panel and saved to params.json.
 *              An existing parameter is read by its bare name, like any value of the namespace.
 *   vec2, vec3 arithmetic and ==, :norm() :dot() :cross(). Sizes cannot be mixed.
 *   complex    * and /, :abs() :arg() :conj(), cis(theta)
 *   smoothstep, and the math and string libraries of Lua
 *
 * After a syntax error, the last chunk that worked is kept.
 * After a runtime error, the values of that section stay frozen.
 * Both are logged once and tried again on the next edit, so a broken snippet never stops the talk.
 * Other likely mistakes give a warning. These are a nan, an unknown keyframe,
 * a name published twice and a malformed "--- name" line.
 * Every error and warning shows in the file editor.
 */

class Snippet;
template <class Sig>
class SnippetFn;

// Global registry of snippet files and of the variables they publish.
class Snippet {
public:
    // Adds a snippet file, resolved from the project data path.
    static void load(const path& file);
    // Reads again every snippet file that changed. Call it once per frame.
    static void HotReloadIfModified();
    // Absolute paths of the snippet files that are watched.
    static std::vector<path> WatchedFiles();
    // Gives the TimeObject of the frame to the snippets and starts a new frame.
    static void setTime(const TimeObject& t);

    // False until the first frame has given a time. Nothing is evaluated before that.
    static bool ready();
    // True when a section has this name.
    static bool hasSection(const std::string& name);
    // True when at least one snippet file was loaded.
    static bool loadedAny();
    // True when a variable, a derivation or a section has this name.
    static bool provides(const std::string& name);
    // False while some section fails.
    static bool ok();
    // Message of the last error.
    static std::string lastError();
    // Names of every published variable.
    static std::vector<std::string> names();

    // Result of a section, with 1 to 4 components.
    // n is 0 when the name is unknown or its section failed.
    // A missing component converts to 0, and a missing alpha to 1.
    struct Value {
        std::array<scalar, 4> v{{0, 0, 0, 0}};
        int n = 0;

        // True when the value exists.
        bool valid() const { return n > 0; }
        // Conversions to a number, a boolean, a vector or a color. Missing components are 0, and a missing alpha is 1.
        operator scalar() const { return n ? v[0] : 0; }
        operator float() const { return float(n ? v[0] : 0); }
        operator int() const { return int(n ? v[0] : 0); }
        operator bool() const { return n > 0 && v[0] != 0; }
        operator vec2() const { return vec2(v[0], v[1]); }
        operator vec() const { return vec(v[0], v[1], v[2]); }
        operator RGBA() const {
            return n >= 4 ? RGBA(float(v[0]), float(v[1]), float(v[2]), float(v[3]))
                          : RGBA(float(v[0]), float(v[1]), float(v[2]), 1.f);
        }

        // The conversions above are ambiguous with constructors that accept any type, such as those of Eigen.
        // They only work in an assignment. To pass a value to a function, name the conversion.
        //   eisenstein(Snippet::get("z1").v2(), ...);
        scalar num() const { return n ? v[0] : 0; }
        vec2 v2() const { return operator vec2(); }
        vec v3() const { return operator vec(); }
        RGBA rgba() const { return operator RGBA(); }
    };

    // What a block of calls read.
    // Put a run of get() and fn() calls between beginRecord() and endRecord() to learn it.
    // Sampling a snippet into a texture uses this to keep the samples while nothing changed.
    struct Deps {
        std::set<std::string> names;
        // True when it read t, so it changes every frame.
        bool time = false;
    };
    // Starts recording what is read.
    static void beginRecord();
    // Stops recording and returns what was read.
    static Deps endRecord();
    // Number that changes when anything in d changed, including reloads and parameters.
    static long stateOf(const Deps& d);
    // Counts how many times a snippet file was read again.
    static long reloads();

    // Value of a variable, computed at most once per frame.
    static Value get(const std::string& name);
    // Same, with a warning when the name is unknown or does not hold `want` numbers.
    static Value get(const std::string& name, int want);
    // Counter that increases when the value differs from the previous frame.
    // dirty() below does the same without keeping the counter yourself.
    static long changed(const std::string& name);

    // True the first time, and then whenever one of these variables changed since this call site last asked.
    //
    //   if (Snippet::dirty({"z1", "z2"}))
    //       rebuild();
    //
    // Each call site has its own state, so two users of the same variable do not affect each other.
    // Two guards on the same line need different `tag` values.
    // It is meant to guard costly work, not to be called for every element.
    static bool dirty(std::initializer_list<const char*> names,
                      const char* tag = nullptr,
                      std::source_location where = std::source_location::current());
    static bool dirty(const char* name, const char* tag = nullptr,
                      std::source_location where = std::source_location::current());

    // Publishes a variable computed by C++, evaluated on demand and once per frame.
    using Derivation = std::function<Value(const TimeObject&)>;
    static void derive(const std::string& name, const Derivation& f);
    // Same, for the common return types.
    static void derive(const std::string& name, const std::function<scalar(const TimeObject&)>& f);
    static void derive(const std::string& name, const std::function<vec2(const TimeObject&)>& f);
    static void derive(const std::string& name, const std::function<vec(const TimeObject&)>& f);

    // Same, recomputed only when one of `deps` changed. The result is cached.
    //
    //   Snippet::derive("g2", {"z1", "z2"},
    //                   [](const TimeObject&) { return eisenstein(...); });
    static void derive(const std::string& name, std::initializer_list<const char*> deps,
                       const Derivation& f);
    static void derive(const std::string& name, std::initializer_list<const char*> deps,
                       const std::function<scalar(const TimeObject&)>& f);

    // A callable section.
    struct Call;
    using CallPtr = std::shared_ptr<Call>;
    // Returns a handle to a callable section. The handle stays valid after a reload.
    static CallPtr resolve(const std::string& name);
    // Calls a section with arguments given as a flat array, so fn<> below needs no Lua header.
    // sizes[i] is the number of components of argument i, either 1, 2 or 3.
    // With `exact`, extra results also give a warning.
    // Returns false when the call failed.
    static bool invoke(const CallPtr& c, const scalar* in, const int* sizes,
                       int nargs, scalar* out, int nout, bool exact = true);

    // Typed handle to a callable section.
    template <class Sig>
    using fn = SnippetFn<Sig>;

private:
    // Creates the Lua state once.
    static void ensureState();
};

namespace snippet_detail {

template <class T>
struct Marshal;

template <>
struct Marshal<scalar> {
    static constexpr int N = 1;
    static void put(scalar* d, scalar x) { d[0] = x; }
    static scalar get(const scalar* d) { return d[0]; }
};
template <>
struct Marshal<float> {
    static constexpr int N = 1;
    static void put(scalar* d, float x) { d[0] = x; }
    static float get(const scalar* d) { return float(d[0]); }
};
template <>
struct Marshal<int> {
    static constexpr int N = 1;
    static void put(scalar* d, int x) { d[0] = x; }
    static int get(const scalar* d) { return int(d[0]); }
};
template <>
struct Marshal<vec2> {
    static constexpr int N = 2;
    static void put(scalar* d, const vec2& x) {
        d[0] = x(0);
        d[1] = x(1);
    }
    static vec2 get(const scalar* d) { return vec2(d[0], d[1]); }
};
template <>
struct Marshal<vec> {
    static constexpr int N = 3;
    static void put(scalar* d, const vec& x) {
        d[0] = x(0);
        d[1] = x(1);
        d[2] = x(2);
    }
    static vec get(const scalar* d) { return vec(d[0], d[1], d[2]); }
};

} // namespace snippet_detail

/*
 * A callable section, resolved once and called many times.
 *   auto f = Snippet::fn<vec(vec,int)>("wobble", identity);
 * The fallback is returned when the snippet is missing or failing.
 */
template <class R, class... A>
class SnippetFn<R(A...)> {
    using RM = snippet_detail::Marshal<std::decay_t<R>>;
    static constexpr int NIN = (0 + ... + snippet_detail::Marshal<std::decay_t<A>>::N);

public:
    SnippetFn() = default;
    // Resolves the section `name`.
    explicit SnippetFn(const std::string& name, R fallback = R())
        : call(Snippet::resolve(name)), fb(fallback) {}

    // True when a section was resolved.
    bool valid() const { return bool(call); }

    // Calls the section, or returns the fallback when it fails.
    R operator()(A... a) const {
        scalar in[NIN > 0 ? NIN : 1];
        static constexpr int sizes[] = {snippet_detail::Marshal<std::decay_t<A>>::N..., 0};
        int k = 0;
        (pack(in, k, a), ...);
        scalar out[RM::N];
        if (!Snippet::invoke(call, in, sizes, int(sizeof...(A)), out, RM::N))
            return fb;
        return RM::get(out);
    }

private:
    template <class T>
    static void pack(scalar* d, int& k, const T& x) {
        using M = snippet_detail::Marshal<std::decay_t<T>>;
        M::put(d + k, x);
        k += M::N;
    }

    Snippet::CallPtr call;
    R fb{};
};

// A world vector, either fixed or given by a snippet variable read every frame.
struct LiveVec {
    vec fixed = vec::Zero();
    std::string snippet;

    LiveVec() = default;
    LiveVec(const vec& v) : fixed(v) {}
    LiveVec(std::string name) : snippet(std::move(name)) {}
    LiveVec(const char* name) : snippet(name) {}

    // True when the vector comes from a snippet.
    bool live() const { return !snippet.empty(); }
    // Current vector.
    vec value() const;
    // Identifies the source, so a user can cache on it.
    std::string key() const;
};

// A number, either fixed or given by a snippet variable read every frame.
struct LiveScalar {
    scalar fixed = 0;
    std::string snippet;

    LiveScalar() = default;
    LiveScalar(scalar v) : fixed(v) {}
    // Prevents a literal 0 from being taken for a name.
    LiveScalar(int v) : fixed(v) {}
    LiveScalar(std::string name) : snippet(std::move(name)) {}
    LiveScalar(const char* name) : snippet(name) {}

    // True when the number comes from a snippet.
    bool live() const { return !snippet.empty(); }
    // Current number.
    scalar value() const;
};

/*
 * A callable snippet sampled on a grid and given to a shader as a texture.
 * Lua cannot be called from GLSL, so the function is evaluated on the CPU at every grid point
 * and uploaded.
 *
 *   --- prior_mean                     -- in snippets.lua
 *   return function(x) return 0.55*math.sin(1.15*x) end
 *
 *   SnippetTexture::Spec sp;
 *   sp.fn = "prior_mean";
 *   sp.u  = vec2(-6, 6);               -- the range covered by the width
 *   fx->setTexture("prior", sp);       -- uniform sampler2D prior;
 *
 * A 1D function (res_v == 1) is called with one number and gives a texture one texel high.
 * A 2D function is called with a vec2.
 * The section returns 1 to 4 numbers, or a vec2 or vec3, and `components` sets how many are kept.
 *
 * Cost
 * Sampling makes res_u * res_v Lua calls, so what matters is how often it happens.
 * This is decided by what the section reads.
 *
 *   the section reads t          sampled every frame
 *   it does not                  sampled once, and again only when a snippet file is saved
 *                                or a value or parameter it read changed
 *
 * A fixed function costs nothing per frame, however fine the grid.
 * A function of time needs a resolution that is affordable at 60 Hz.
 * `when` overrides the automatic choice.
 */
struct SnippetTexture {
    struct Spec {
        // Name of the callable section to sample.
        std::string fn;
        // Grid size. With res_v equal to 1 the function takes one number.
        int res_u = 256, res_v = 1;
        // Range of the parameter covered by the width.
        vec2 u = vec2(0, 1);
        // Range covered by the height, for a 2D function.
        vec2 v = vec2(0, 1);
        // Number of returned values to keep, from 1 to 4.
        int components = 1;
        // Auto decides from what the section read.
        enum class When { Auto,
                          Once,
                          Always };
        When when = When::Auto;
    };

    explicit SnippetTexture(const Spec& spec) : sp(spec) {}

    // Changes the spec, which forces a new sampling.
    void configure(const Spec& spec);
    const Spec& spec() const { return sp; }

    // Samples again when needed. Returns true when the samples changed and must be uploaded.
    bool update();

    // Samples, row by row, with `components` values per texel.
    const std::vector<float>& data() const { return samples; }
    int width() const { return sp.res_u; }
    int height() const { return std::max(1, sp.res_v); }
    int components() const { return sp.components; }
    // True when the section is sampled every frame.
    bool animated() const { return deps.time; }

private:
    Spec sp;
    std::vector<float> samples;
    Snippet::Deps deps;
    long state = 0;
    bool sampled = false;

    // Evaluates the section on the grid.
    void sample();
};

} // namespace slope

#endif // SNIPPET_H
