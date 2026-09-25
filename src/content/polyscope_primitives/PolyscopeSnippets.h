#ifndef POLYSCOPESNIPPETS_H
#define POLYSCOPESNIPPETS_H

#include "content/polyscope_primitives/Mesh.h"
#include "content/polyscope_primitives/Curve3D.h"
#include "content/authoring/Snippet.h"

namespace slope {

/*
 * Polyscope objects defined by a snippet. The geometry, its animation and its domain
 * are in the .lua file and reload with it, so an animated object needs no C++.
 *
 * Each wrapper samples one callable section over its parameter domain, once per frame.
 * The section sees `t` and the whole snippet namespace, so it can change with time.
 * Without `t` the object stays still.
 * The wrappers differ only by their parameter space.
 *
 *   SnippetSurface   vec2 to vec3, a grid over the rectangle u x v
 *   SnippetCurve     scalar to vec3, a subdivision of the segment u
 *
 * Each sample calls Lua, which is slow. A smooth animation can afford a few thousand samples.
 * See examples/snippet_perf for measurements.
 *
 * A deck uses them through "surface:" and "curve:" (see DeckLoader).
 */

// A mesh whose vertices are a snippet function of two parameters.
class SnippetSurface : public Mesh {
public:
    using SnippetSurfacePtr = std::shared_ptr<SnippetSurface>;

    /*
     *   --- wave                       -- in snippets.lua
     *   return function(uv)
     *     local x, y = uv.x, uv.y
     *     return vec3(x, y, 0.2*math.sin(6*x + t.from_begin)*math.cos(6*y))
     *   end
     *
     * The function takes a vec2 for the parameter point, not two numbers.
     * It returns a vec3, or three numbers.
     *
     *   auto s = SnippetSurface::Add("wave");                 // [0,1]^2, 64x64
     *
     *   SnippetSurface::Spec sp;                              // a torus
     *   sp.fn = "torus";
     *   sp.u = sp.v = vec2(0, TAU);
     *   sp.closed_u = sp.closed_v = true;
     *   show << SnippetSurface::Add(sp);
     */
    struct Spec {
        // Callable snippet section, from vec2 to vec3.
        std::string fn;
        // Name of the object, the same as fn by default.
        std::string name;
        // Parameter domain.
        vec2 u = vec2(0, 1);
        vec2 v = vec2(0, 1);
        // Number of subdivisions along each parameter.
        int res_u = 64, res_v = 64;
        // Joins the edge u = u1 to the edge u = u0, and the same for v.
        bool closed_u = false;
        bool closed_v = false;
        // Smooth shading.
        bool smooth = true;
    };

    // Builds a surface from a spec.
    static SnippetSurfacePtr Add(const Spec& spec);
    // Builds a surface on the domain [0,1]^2 with the same resolution on both axes.
    static SnippetSurfacePtr Add(const std::string& fn, int resolution = 64);

    SnippetSurface(const Spec& spec);

    // Applies a new spec, and rebuilds the grid if it changed. Lets a deck reload edit a surface in place.
    void configure(const Spec& spec);
    const Spec& spec() const { return sp; }

    // Evaluates the snippet on the grid. Called once per frame.
    void update();

    // Primitive interface
public:
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    Spec sp;
    // Grid size of the current topology.
    int nu = 0, nv = 0;
    Snippet::fn<vec(vec2)> f;

    // Parameter point of grid node (i, j).
    vec2 node(int i, int j) const;
    // Evaluates the snippet on the grid and updates the vertices.
    void sample();
    // Builds vertices and faces for the current resolution.
    void buildGrid();
    // Same, then creates a new polyscope structure for the new topology.
    void rebuild();
    // Number of vertices along u and along v.
    int columns() const { return sp.closed_u ? nu : nu + 1; }
    int rows() const { return sp.closed_v ? nv : nv + 1; }
};

using SnippetSurfacePtr = SnippetSurface::SnippetSurfacePtr;

// A curve whose nodes are a snippet function of one parameter.
class SnippetCurve : public Curve3D {
public:
    using SnippetCurvePtr = std::shared_ptr<SnippetCurve>;

    /*
     *   --- helix                      -- in snippets.lua
     *   return function(s)
     *     return vec3(math.cos(s + t.from_begin), math.sin(s + t.from_begin),
     *                 0.15*s)
     *   end
     *
     * The function takes one number and returns a vec3, or three numbers.
     *
     *   auto c = SnippetCurve::Add("helix");                  // [0,1], 200 nodes
     *
     *   SnippetCurve::Spec sp;                                // a closed loop
     *   sp.fn = "knot";
     *   sp.u = vec2(0, TAU);
     *   sp.closed = true;
     *   show << SnippetCurve::Add(sp);
     */
    struct Spec {
        // Callable snippet section, from scalar to vec3.
        std::string fn;
        // Name of the object, the same as fn by default.
        std::string name;
        // Parameter domain.
        vec2 u = vec2(0, 1);
        // Number of segments.
        int resolution = 200;
        // Joins the last node to the first.
        bool closed = false;
        // Tube radius. A negative value keeps the default of polyscope.
        scalar radius = -1;
    };

    // Builds a curve from a spec.
    static SnippetCurvePtr Add(const Spec& spec);
    // Builds a curve on the domain [0,1].
    static SnippetCurvePtr Add(const std::string& fn, int resolution = 200);

    SnippetCurve(const Spec& spec);

    // Applies a new spec, like for a surface, so a deck reload edits the curve in place.
    void configure(const Spec& spec);
    const Spec& spec() const { return sp; }

    // Evaluates the snippet at the nodes. Called once per frame.
    void update();

    // Primitive interface
public:
    void initPolyscope() override;
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    Spec sp;
    // Number of segments of the current nodes.
    int n = 0;
    Snippet::fn<vec(scalar)> f;

    // Number of nodes. A closed curve has no node for the last sample, since the loop closes it.
    int samples() const { return sp.closed ? n : n + 1; }
    // Parameter value of node i.
    scalar node(int i) const;
    // Evaluates the snippet at the nodes.
    void sample();
    // Allocates the nodes for the current resolution.
    void buildNodes();
    // Same, then creates a new polyscope structure.
    void rebuild();
};

using SnippetCurvePtr = SnippetCurve::SnippetCurvePtr;

} // namespace slope

#endif // POLYSCOPESNIPPETS_H
