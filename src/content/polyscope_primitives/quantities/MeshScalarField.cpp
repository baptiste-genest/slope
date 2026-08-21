#include "content/polyscope_primitives/quantities/MeshScalarField.h"

#include "polyscope/surface_mesh.h"
#include "polyscope/render/engine.h"

#include <spdlog/spdlog.h>

slope::MeshScalarField::MeshScalarField(const Mesh::MeshPtr &mesh, const std::string &name,
                                       const scalars &values, const std::string &colormap)
    : mesh(mesh), name(name), colormap(colormap), values(values), scratch(values)
{
}

slope::MeshScalarFieldPtr slope::MeshScalarField::Add(const Mesh::MeshPtr &mesh,
                                                      const std::string &name,
                                                      const scalars &values,
                                                      const std::string &colormap)
{
    if (mesh == nullptr)
        throw std::runtime_error("MeshScalarField::Add : null mesh");
    if (values.empty())
        throw std::runtime_error("MeshScalarField::Add : empty field");
    if (values.size() != mesh->getVertices().size())
        throw std::runtime_error("MeshScalarField::Add : field has " +
                                 std::to_string(values.size()) + " values for " +
                                 std::to_string(mesh->getVertices().size()) + " vertices");

    return NewPrimitive<MeshScalarField>(mesh, name, values, colormap);
}

slope::MeshScalarFieldPtr slope::MeshScalarField::Add(const Mesh::MeshPtr &mesh,
                                                      const std::string &name,
                                                      const Vec &values,
                                                      const std::string &colormap)
{
    return Add(mesh, name, scalars(values.data(), values.data() + values.size()), colormap);
}

void slope::MeshScalarField::initPolyscope()
{
    q = mesh->pc->addVertexScalarQuantity(name, values);
    q->setColorMap(colormap);

    const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
    range = {*lo, *hi};
    if (!baseline_set)
        baseline = *lo;

    q->setMapRange(range);
    q->setEnabled(false);

    resolveColors();
}

void slope::MeshScalarField::setBaseline(scalar v)
{
    baseline = v;
    baseline_set = true;
    if (q == nullptr)
        return;
    if (v < range.first || v > range.second)
        spdlog::warn("[MeshScalarField] baseline {} is outside the field range [{}, {}] : "
                     "the flat start is clamped by the colour map, so the hand-off from "
                     "the mesh colour becomes visible",
                     v, range.first, range.second);
    resolveColors();
    last_p = -1;
}

void slope::MeshScalarField::resolveColors()
{
    const double span = range.second - range.first;
    const double t = span > 1e-12 ? (baseline - range.first) / span : 0.;
    field_color = polyscope::render::engine->getColorMap(colormap).getValue(t);
}

void slope::MeshScalarField::draw(const TimeObject &, const StateInSlide &)
{
    apply(1);
}

void slope::MeshScalarField::playIntro(const TimeObject &t, const StateInSlide &)
{
    apply(t.transition_parameter);
}

void slope::MeshScalarField::playOutro(const TimeObject &t, const StateInSlide &)
{
    apply(1 - t.transition_parameter);
}

void slope::MeshScalarField::forceEnable()
{
    base_color = mesh->pc->getSurfaceColor();
    last_p = -1;
}

void slope::MeshScalarField::forceDisable()
{
    if (q != nullptr)
        q->setEnabled(false);
    mesh->pc->setSurfaceColor(base_color);
    last_p = -1;
}

void slope::MeshScalarField::apply(scalar u)
{
    if (std::abs(u - last_p) < 1e-6)
        return;
    last_p = u;

    const scalar split = std::clamp(color_split, 0., 1.);

    if (u < split) {
        q->setEnabled(false);
        mesh->pc->setSurfaceColor(glm::mix(base_color, field_color, float(u / split)));
        return;
    }

    mesh->pc->setSurfaceColor(field_color);
    q->setEnabled(true);
    uploadField(split < 1. ? (u - split) / (1. - split) : 1.);
}

void slope::MeshScalarField::uploadField(scalar p)
{
    for (size_t i = 0; i < values.size(); ++i)
        scratch[i] = std::lerp(baseline, values[i], p);
    q->updateData(scratch);
}
