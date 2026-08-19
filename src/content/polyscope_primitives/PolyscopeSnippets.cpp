#include "PolyscopeSnippets.h"

namespace slope {

// ── SnippetSurface ──────────────────────────────────────────────────────────

SnippetSurface::SnippetSurface(const Spec& spec) : sp(spec)
{
    if (sp.name.empty())
        sp.name = sp.fn;
    smooth = sp.smooth;
    f = Snippet::fn<vec(vec2)>(sp.fn);
    buildGrid();
}

SnippetSurface::SnippetSurfacePtr SnippetSurface::Add(const Spec& spec)
{
    return NewPrimitive<SnippetSurface>(spec);
}

SnippetSurface::SnippetSurfacePtr SnippetSurface::Add(const std::string& fn, int resolution)
{
    Spec sp;
    sp.fn = fn;
    sp.res_u = sp.res_v = resolution;
    return Add(sp);
}

void SnippetSurface::configure(const Spec& spec)
{
    const bool topology = spec.closed_u != sp.closed_u || spec.closed_v != sp.closed_v
                          || spec.res_u != sp.res_u || spec.res_v != sp.res_v;
    const bool domain = spec.u != sp.u || spec.v != sp.v;
    const std::string name = spec.name.empty() ? spec.fn : spec.name;
    const bool renamed = name != sp.name || spec.fn != sp.fn;

    sp = spec;
    sp.name = name;
    if (renamed)
        f = Snippet::fn<vec(vec2)>(sp.fn);
    if (sp.smooth != smooth) {
        smooth = sp.smooth;
        setSmooth(smooth);
    }
    if (topology)
        rebuild();
    else if (domain)
        update();
}

// the parameter point of grid node (i, j). A closed axis drops its last
// sample, the seam being the wrap of the face indices instead
vec2 SnippetSurface::node(int i, int j) const
{
    return vec2(sp.u(0) + (sp.u(1) - sp.u(0)) * scalar(i) / nu,
                sp.v(0) + (sp.v(1) - sp.v(0)) * scalar(j) / nv);
}

void SnippetSurface::buildGrid()
{
    nu = std::max(1, sp.res_u);
    nv = std::max(1, sp.res_v);
    const int cols = columns(), rws = rows();
    vertices.resize(size_t(cols) * rws);
    sample();

    faces.clear();
    faces.reserve(size_t(nu) * nv);
    auto id = [&](int i, int j) { return size_t((j % rws) * cols + (i % cols)); };
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++)
            faces.push_back({id(i, j), id(i + 1, j), id(i + 1, j + 1), id(i, j + 1)});
}

void SnippetSurface::sample()
{
    const int cols = columns(), rws = rows();
    // before the first frame nothing may be evaluated, and a flat parameter
    // square is a saner bounding box to register than a heap of zeros
    const bool live = Snippet::ready();
    for (int j = 0; j < rws; j++)
        for (int i = 0; i < cols; i++) {
            const vec2 uv = node(i, j);
            vertices[size_t(j) * cols + i] = live ? f(uv) : vec(uv(0), uv(1), 0);
        }
}

void SnippetSurface::rebuild()
{
    buildGrid();
    const bool was_enabled = isEnabled();
    initPolyscope();               // a new topology is a new polyscope mesh
    if (was_enabled)
        polyscope_ptr->setEnabled(true);
}

void SnippetSurface::update()
{
    if (!Snippet::ready())
        return;
    sample();
    pc->updateVertexPositions(vertices);
}

void SnippetSurface::initPolyscope()
{
    if (!registered) {
        surface_color = getColor();
        registered = true;
    }
    pc = polyscope::registerSurfaceMesh(getPolyscopeName(), vertices, faces);
    pc->setBackFacePolicy(polyscope::BackFacePolicy::Identical);
    initPolyscopeData(pc);
    setSmooth(smooth);
    pc->setSurfaceColor(surface_color);
}

void SnippetSurface::draw(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Mesh::draw(t, sis);
}

void SnippetSurface::playIntro(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Mesh::playIntro(t, sis);
}

void SnippetSurface::playOutro(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Mesh::playOutro(t, sis);
}

// ── SnippetCurve ────────────────────────────────────────────────────────────

SnippetCurve::SnippetCurve(const Spec& spec) : sp(spec)
{
    if (sp.name.empty())
        sp.name = sp.fn;
    loop = sp.closed;
    radius = sp.radius;
    f = Snippet::fn<vec(scalar)>(sp.fn);
    buildNodes();
}

SnippetCurve::SnippetCurvePtr SnippetCurve::Add(const Spec& spec)
{
    return NewPrimitive<SnippetCurve>(spec);
}

SnippetCurve::SnippetCurvePtr SnippetCurve::Add(const std::string& fn, int resolution)
{
    Spec sp;
    sp.fn = fn;
    sp.resolution = resolution;
    return Add(sp);
}

void SnippetCurve::configure(const Spec& spec)
{
    const bool topology = spec.closed != sp.closed || spec.resolution != sp.resolution
                          || spec.radius != sp.radius;
    const bool domain = spec.u != sp.u;
    const std::string name = spec.name.empty() ? spec.fn : spec.name;
    const bool renamed = name != sp.name || spec.fn != sp.fn;

    sp = spec;
    sp.name = name;
    if (renamed)
        f = Snippet::fn<vec(scalar)>(sp.fn);
    loop = sp.closed;
    radius = sp.radius;
    if (topology)
        rebuild();
    else if (domain)
        update();
}

// the parameter of node i. A closed curve drops its last sample, the loop
// closing it instead
scalar SnippetCurve::node(int i) const
{
    return sp.u(0) + (sp.u(1) - sp.u(0)) * scalar(i) / n;
}

void SnippetCurve::buildNodes()
{
    n = std::max(1, sp.resolution);
    nodes.resize(samples());
    sample();
}

void SnippetCurve::sample()
{
    // before the first frame nothing may be evaluated, and the parameter laid
    // on the x axis is a saner bounding box to register than a heap of zeros
    const bool live = Snippet::ready();
    for (int i = 0; i < samples(); i++) {
        const scalar u = node(i);
        nodes[i] = live ? f(u) : vec(u, 0, 0);
    }
}

void SnippetCurve::rebuild()
{
    buildNodes();
    const bool was_enabled = isEnabled();
    initPolyscope();               // a new node count is a new curve network
    if (was_enabled)
        polyscope_ptr->setEnabled(true);
}

void SnippetCurve::update()
{
    if (!Snippet::ready())
        return;
    sample();
    pc->updateNodePositions(nodes);
}

void SnippetCurve::initPolyscope()
{
    if (!registered) {
        curve_color = getColor();
        registered = true;
    }
    pc = loop ? polyscope::registerCurveNetworkLoop(getPolyscopeName(), nodes)
              : polyscope::registerCurveNetworkLine(getPolyscopeName(), nodes);
    if (radius > 0)
        pc->setRadius(radius, false);
    pc->setColor(curve_color);
    initPolyscopeData(pc);
}

void SnippetCurve::draw(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Curve3D::draw(t, sis);
}

void SnippetCurve::playIntro(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Curve3D::playIntro(t, sis);
}

void SnippetCurve::playOutro(const TimeObject& t, const StateInSlide& sis)
{
    update();
    Curve3D::playOutro(t, sis);
}

}
