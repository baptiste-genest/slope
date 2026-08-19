#ifndef SNIPPET_H
#define SNIPPET_H

#include "../libslope.h"
#include "TimeObject.h"
#include <array>
#include <set>
#include <source_location>

namespace slope {

/*
 * Hot-reloaded Lua snippets, the shape of an animation, editable while the
 * show runs.
 *
 * Composition (deck.yaml), constants (Params) and pixel math (.frag) already
 * reload live; the logic in between is a C++ lambda and costs a rebuild. A
 * snippet file closes that gap without putting any Lua syntax outside itself,
 * everything else refers to snippets *by name*.
 *
 * ── The file ───────────────────────────────────────────────────────────────
 * One .lua file, split into sections by a "--- name" line (a legal Lua comment,
 * so the file still highlights as Lua). What a section returns decides what it
 * becomes :
 *
 *   --- envelope                       -- a value, one variable, section-named
 *   return math.sin(t.from_begin * speed)
 *
 *   --- lattice                        -- a table, one variable per key, flat
 *   local z1 = complex(1, 0.5 + 0.5*math.cos(t.from_begin))
 *   return { z1 = z1, z2 = complex(0,1), tau = complex(0,1)/z1 }
 *
 *   --- wobble                         -- a function, a callable snippet
 *   return function(p, i)
 *     return p + vec3(0, 0, envelope * math.exp(-20*p:norm()^2))
 *   end
 *
 * Sections see `t` (the current slide's TimeObject), the built-ins below, and
 * every other name in the snippet namespace, which is shared with Params.
 * Reading another section evaluates it, so order in the file does not matter.
 *
 * ── Reading from C++ ───────────────────────────────────────────────────────
 *   vec2   z1  = Snippet::get("z1");        // converts to scalar/int/vec2/vec/RGBA
 *   scalar env = Snippet::get("envelope");
 *
 * get() is a per-frame memoized pull. The section runs at most once a frame,
 * on first read, and not at all on a slide that never asks. It does run again
 * every frame it is read on, whether or not it uses `t`, so heavy work belongs
 * in a derive() with dependencies rather than in a section.
 *
 * Calling a function-returning section wants its handle hoisted out of the loop
 * (the name lookup costs more than the call) :
 *
 *   auto wobble = Snippet::fn<vec(vec,int)>("wobble");   // resolved once
 *   for (size_t i = 0; i < V.size(); i++)
 *       V[i] = wobble(v0[i], int(i));
 *
 * A snippet that errors returns the fallback (the fn's second argument), and
 * latches for the rest of the frame so a broken snippet is fast, not slow.
 *
 * C++ can publish back into the same namespace, lazily :
 *
 *   Snippet::derive("g2", [](const TimeObject&) { return eisenstein(4); });
 *
 * and ask whether a value actually moved, to skip expensive recomputation :
 *
 *   if (Snippet::changed("z1") != last_gen) { ... }
 *
 * ── Feeding a shader ───────────────────────────────────────────────────────
 *   fx->bind("z1", [] { return (vec2)Snippet::get("z1"); });
 *
 * ── Built-ins ──────────────────────────────────────────────────────────────
 *   t          from_begin, from_action, delta_time, absolute_frame_number,
 *              transition_parameter (0 to 1 across a slide change, 1 when
 *              settled), and t:afterKeyframe/beforeKeyframe/atKeyframe/
 *              slidesSince("name"). from_begin is the free running clock an
 *              animation usually wants. A snippet has no moment of appearing,
 *              so there is no inner_time and no relative_frame_number (the
 *              shader uniforms of those names are per primitive and do exist)
 *   param      param("x", def, min, max) declares a parameter with its slider
 *              bounds and returns it, tunable in the Tuner panel and saved to
 *              params.json. An existing parameter is read by its bare name,
 *              like any other value in the namespace
 *   vec2/vec3  arithmetic, :norm() :dot() :cross()
 *   complex    *complex* * and /, :abs() :arg() :conj(), cis(theta)
 *   smoothstep, plus Lua's math / string
 *
 * A syntax error keeps the last chunk that worked, a runtime error freezes that
 * section's values, and both are logged once and retried on the next edit, so a
 * broken snippet never takes the talk down.
 */

class Snippet;
template<class Sig> class SnippetFn;

class Snippet {
public:
    // ── lifecycle ──────────────────────────────────────────────────────────
    // adds a snippet file (resolved against the project data path)
    static void load(const path& file);
    // re-reads any snippet file whose mtime moved; call once per frame
    static void HotReloadIfModified();
    // publishes the frame's TimeObject and opens a new evaluation frame
    static void setTime(const TimeObject& t);

    // False until the first frame has published a time. Nothing is evaluated
    // before that, so an unknown name still looks the same as one whose section
    // has simply not run yet.
    static bool ready();   // whether Lua sections may be evaluated yet
    static bool ok();                  // false while some section is failing
    static std::string lastError();
    static std::vector<std::string> names();   // every published variable

    // ── values ─────────────────────────────────────────────────────────────
    // 1..4 components, whatever the section returned; n == 0 means the name is
    // unknown or its section failed, and every conversion then yields zero
    struct Value {
        std::array<scalar,4> v{{0,0,0,0}};
        int n = 0;

        bool valid() const { return n > 0; }
        operator scalar() const { return n ? v[0] : 0; }
        operator float()  const { return float(n ? v[0] : 0); }
        operator int()    const { return int(n ? v[0] : 0); }
        operator bool()   const { return n > 0 && v[0] != 0; }
        operator vec2()   const { return n >= 2 ? vec2(v[0], v[1]) : vec2::Zero(); }
        operator vec()    const {
            if (n >= 3) return vec(v[0], v[1], v[2]);
            if (n == 2) return vec(v[0], v[1], 0);
            return vec::Zero();
        }
        operator RGBA() const {
            return n >= 4 ? RGBA(float(v[0]), float(v[1]), float(v[2]), float(v[3]))
                          : RGBA(float(v[0]), float(v[1]), float(v[2]), 1.f);
        }

        // The conversions above are ambiguous against a constructor that takes
        // anything (Eigen's), so they only serve an assignment. Say which one
        // you meant when handing a value straight to a function :
        //   eisenstein(Snippet::get("z1").v2(), ...);
        scalar num()  const { return n ? v[0] : 0; }
        vec2   v2()   const { return operator vec2(); }
        vec    v3()   const { return operator vec(); }
        RGBA   rgba() const { return operator RGBA(); }
    };

    // ── what a block of calls depends on ───────────────────────────────────
    // Wrap a run of get()/fn() calls in beginRecord()/endRecord() to learn what
    // it read. Sampling a snippet into a texture uses this to keep the samples
    // when nothing they were built from has moved.
    struct Deps {
        std::set<std::string> names;
        bool time = false;      // it read t, so it changes every frame
    };
    static void beginRecord();
    static Deps endRecord();
    // changes whenever anything in d has moved, reloads and parameters included
    static long stateOf(const Deps& d);
    // bumped every time a snippet file is re-read
    static long reloads();

    static Value get(const std::string& name);
    // bumps whenever the value actually differs from the previous frame's.
    // Track it yourself only when one consumer watches many things; dirty()
    // below is the same idea without the bookkeeping.
    static long changed(const std::string& name);

    // True the first time it is reached, and afterwards whenever one of these
    // variables has moved since *this call site* last asked :
    //
    //   if (Snippet::dirty({"z1", "z2"}))
    //       rebuild();                       // skipped while they sit still
    //
    // The call site is the identity, so two consumers of the same variable
    // never clear each other's flag and no caller has to hold a counter. Two
    // independent guards on one line want distinct `tag`s. Meant for gating
    // expensive work, not for calling per element.
    static bool dirty(std::initializer_list<const char*> names,
                      const char* tag = nullptr,
                      std::source_location where = std::source_location::current());
    static bool dirty(const char* name, const char* tag = nullptr,
                      std::source_location where = std::source_location::current());

    // a C++-computed variable, evaluated lazily and memoized per frame
    using Derivation = std::function<Value(const TimeObject&)>;
    static void derive(const std::string& name, const Derivation& f);
    // sugar for the common return types
    static void derive(const std::string& name, const std::function<scalar(const TimeObject&)>& f);
    static void derive(const std::string& name, const std::function<vec2(const TimeObject&)>& f);
    static void derive(const std::string& name, const std::function<vec(const TimeObject&)>& f);

    // the same, recomputed only when one of `deps` moves, and the cached result
    // is kept for you
    //
    //   Snippet::derive("g2", {"z1", "z2"},
    //                   [](const TimeObject&) { return eisenstein(...); });
    static void derive(const std::string& name, std::initializer_list<const char*> deps,
                       const Derivation& f);
    static void derive(const std::string& name, std::initializer_list<const char*> deps,
                       const std::function<scalar(const TimeObject&)>& f);

    // ── callable sections ──────────────────────────────────────────────────
    struct Call;
    using CallPtr = std::shared_ptr<Call>;
    // survives hot reloads, the handle is stable and its chunk re-resolved
    static CallPtr resolve(const std::string& name);
    // flat marshalling, so the fn<> template below needs no Lua header.
    // sizes[i] is the component count of argument i (1, 2 or 3).
    static bool invoke(const CallPtr& c, const scalar* in, const int* sizes,
                       int nargs, scalar* out, int nout);

    template<class Sig> using fn = SnippetFn<Sig>;

private:
    static void ensureState();
};

namespace snippet_detail {

template<class T> struct Marshal;

template<> struct Marshal<scalar> {
    static constexpr int N = 1;
    static void put(scalar* d, scalar x) { d[0] = x; }
    static scalar get(const scalar* d) { return d[0]; }
};
template<> struct Marshal<float> {
    static constexpr int N = 1;
    static void put(scalar* d, float x) { d[0] = x; }
    static float get(const scalar* d) { return float(d[0]); }
};
template<> struct Marshal<int> {
    static constexpr int N = 1;
    static void put(scalar* d, int x) { d[0] = x; }
    static int get(const scalar* d) { return int(d[0]); }
};
template<> struct Marshal<vec2> {
    static constexpr int N = 2;
    static void put(scalar* d, const vec2& x) { d[0] = x(0); d[1] = x(1); }
    static vec2 get(const scalar* d) { return vec2(d[0], d[1]); }
};
template<> struct Marshal<vec> {
    static constexpr int N = 3;
    static void put(scalar* d, const vec& x) { d[0] = x(0); d[1] = x(1); d[2] = x(2); }
    static vec get(const scalar* d) { return vec(d[0], d[1], d[2]); }
};

}

/*
 * A callable section, resolved once and called in a loop :
 *   auto f = Snippet::fn<vec(vec,int)>("wobble", identity);
 * The fallback is returned whenever the snippet is missing or failing.
 */
template<class R, class... A>
class SnippetFn<R(A...)> {
    using RM = snippet_detail::Marshal<std::decay_t<R>>;
    static constexpr int NIN = (0 + ... + snippet_detail::Marshal<std::decay_t<A>>::N);

public:
    SnippetFn() = default;
    explicit SnippetFn(const std::string& name, R fallback = R())
        : call(Snippet::resolve(name)), fb(fallback) {}

    bool valid() const { return bool(call); }

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
    template<class T>
    static void pack(scalar* d, int& k, const T& x) {
        using M = snippet_detail::Marshal<std::decay_t<T>>;
        M::put(d + k, x);
        k += M::N;
    }

    Snippet::CallPtr call;
    R fb{};
};


// a world vector given outright, or named by a snippet variable read each frame
struct LiveVec {
    vec fixed = vec::Zero();
    std::string snippet;

    bool live() const {return !snippet.empty();}
    vec value() const;
    // identifies the source, so a consumer can cache on it
    std::string key() const;
};

/*
 * A callable snippet sampled onto a grid, ready to hand to a shader as a
 * texture. Lua cannot be called from GLSL, so this is how a snippet function
 * reaches the GPU : it is evaluated on the CPU once per grid point and uploaded.
 *
 *   --- prior_mean                     -- in snippets.lua
 *   return function(x) return 0.55*math.sin(1.15*x) end
 *
 *   SnippetTexture::Spec sp;
 *   sp.fn = "prior_mean";
 *   sp.u  = vec2(-6, 6);               -- what the width covers
 *   fx->setTexture("prior", sp);       -- uniform sampler2D prior;
 *
 * A 1D function (res_v == 1) is called with one number and gives a texture one
 * texel high; a 2D one is called with a vec2. The section may return 1 to 4
 * numbers, or a vec2/vec3, and `components` says how many of them to keep.
 *
 * ── Cost ───────────────────────────────────────────────────────────────────
 * Sampling is res_u * res_v Lua calls, so the question that matters is how
 * often it happens. It is answered by watching what the section reads :
 *
 *   the section reads t          resampled every frame
 *   it does not                  sampled once, and again only when a snippet
 *                                file is saved, or a value or parameter it
 *                                read has moved
 *
 * So a fixed function costs nothing per frame however fine the grid, and a
 * time dependent one wants a resolution you would be happy to pay for at 60Hz.
 * `when` overrides the verdict when you know better than the recording does.
 */
struct SnippetTexture {
    struct Spec {
        std::string fn;                 // the callable section to sample
        int res_u = 256, res_v = 1;     // res_v == 1 means a function of one number
        vec2 u = vec2(0,1);             // the parameter range the width covers
        vec2 v = vec2(0,1);             // and the height, when 2D
        int components = 1;             // 1..4, how many returned numbers to keep
        // Auto reads the verdict off what the section touched
        enum class When { Auto, Once, Always };
        When when = When::Auto;
    };

    explicit SnippetTexture(const Spec& spec) : sp(spec) {}

    void configure(const Spec& spec);
    const Spec& spec() const {return sp;}

    // resamples when it has to. True when the samples changed and want uploading
    bool update();

    const std::vector<float>& data() const {return samples;}
    int width() const {return sp.res_u;}
    int height() const {return std::max(1,sp.res_v);}
    int components() const {return sp.components;}
    // whether the last verdict was to resample every frame
    bool animated() const {return deps.time;}

private:
    Spec sp;
    std::vector<float> samples;
    Snippet::Deps deps;
    long state = 0;
    bool sampled = false;

    void sample();
};

}

#endif // SNIPPET_H
