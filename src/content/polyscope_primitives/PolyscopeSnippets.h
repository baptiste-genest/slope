#ifndef POLYSCOPESNIPPETS_H
#define POLYSCOPESNIPPETS_H

#include "content/polyscope_primitives/Mesh.h"
#include "content/polyscope_primitives/Curve3D.h"
#include "content/authoring/Snippet.h"

namespace slope {

/*
 * Polyscope objects that *are* a snippet : the geometry, its animation and
 * its domain all live in the .lua file and reload with it, so an animated
 * scene object needs no C++ at all.
 *
 * Each wrapper samples one callable section over its parameter domain, once
 * per frame. The section sees `t` and the whole snippet namespace like any
 * other, which is what makes it time varying : drop `t` and the object simply
 * sits still. What differs between them is only the parameter space :
 *
 *   SnippetSurface   vec2 -> vec3, a grid of the rectangle u x v
 *   SnippetCurve     scalar -> vec3, a subdivision of the segment u
 *
 * A per sample Lua call is not free : count on a few thousand of them for a
 * smooth animation, and see examples/snippet_perf for the real curve.
 *
 * From a deck, "surface:" and "curve:" (see DeckLoader).
 */

class SnippetSurface : public Mesh
{
public:
    using SnippetSurfacePtr = std::shared_ptr<SnippetSurface>;

    /*
     *   --- wave                       -- in snippets.lua
     *   return function(uv)
     *     local x, y = uv.x, uv.y
     *     return vec3(x, y, 0.2*math.sin(6*x + t.from_begin)*math.cos(6*y))
     *   end
     *
     * The function takes a vec2 (the parameter point, *not* two numbers) and
     * returns a vec3, or three numbers.
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
        std::string fn;                 // callable snippet section, vec2 -> vec3
        std::string name;               // reference name, the fn name by default
        vec2 u = vec2(0, 1);            // parameter domain
        vec2 v = vec2(0, 1);
        int res_u = 64, res_v = 64;     // subdivisions of each parameter axis
        bool closed_u = false;          // welds the u = u1 seam onto u = u0
        bool closed_v = false;
        bool smooth = true;
    };

    static SnippetSurfacePtr Add(const Spec& spec);
    static SnippetSurfacePtr Add(const std::string& fn, int resolution = 64);

    SnippetSurface(const Spec& spec);

    // re-reads the domain, the seams and the resolution, and rebuilds when
    // the grid changed. Lets a deck reload edit a surface in place
    void configure(const Spec& spec);
    const Spec& spec() const {return sp;}

    // samples the snippet over the current grid; called once per frame
    void update();

    // Primitive interface
public:
    void initPolyscope() override;
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    Spec sp;
    int nu = 0, nv = 0;                 // grid the current topology was built on
    Snippet::fn<vec(vec2)> f;
    bool registered = false;
    glm::vec3 surface_color;

    // the parameter point of grid node (i, j)
    vec2 node(int i, int j) const;
    // evaluates the snippet over the current grid, in place
    void sample();
    // vertices and faces for the current parameter counts
    void buildGrid();
    // the same, then a fresh polyscope structure for the new topology
    void rebuild();
    int columns() const {return sp.closed_u ? nu : nu + 1;}
    int rows() const {return sp.closed_v ? nv : nv + 1;}
};

using SnippetSurfacePtr = SnippetSurface::SnippetSurfacePtr;


class SnippetCurve : public Curve3D
{
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
        std::string fn;                 // callable snippet section, scalar -> vec3
        std::string name;               // reference name, the fn name by default
        vec2 u = vec2(0, 1);            // parameter domain
        int resolution = 200;           // subdivisions of the segment
        bool closed = false;            // joins the last node back to the first
        scalar radius = -1;             // < 0 leaves polyscope its default
    };

    static SnippetCurvePtr Add(const Spec& spec);
    static SnippetCurvePtr Add(const std::string& fn, int resolution = 200);

    SnippetCurve(const Spec& spec);

    // as for a surface, so a deck reload edits the curve in place
    void configure(const Spec& spec);
    const Spec& spec() const {return sp;}

    // samples the snippet over the current subdivision; once per frame
    void update();

    // Primitive interface
public:
    void initPolyscope() override;
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;

private:
    Spec sp;
    int n = 0;                          // subdivision the nodes were built on
    Snippet::fn<vec(scalar)> f;
    bool registered = false;
    glm::vec3 curve_color;

    // a closed curve drops its last sample, the loop closing it instead
    int samples() const {return sp.closed ? n : n + 1;}
    scalar node(int i) const;
    void sample();
    void buildNodes();
    void rebuild();
};

using SnippetCurvePtr = SnippetCurve::SnippetCurvePtr;

}

#endif // POLYSCOPESNIPPETS_H
