#ifndef MESHSCALARFIELD_H
#define MESHSCALARFIELD_H

#include "content/polyscope_primitives/Mesh.h"

namespace polyscope {
class SurfaceVertexScalarQuantity;
}

namespace slope {

class MeshScalarField;
using MeshScalarFieldPtr = std::shared_ptr<MeshScalarField>;

// A scalar value per vertex shown on a mesh with a colormap.
class MeshScalarField : public Primitive {
public:
    using Quantity = polyscope::SurfaceVertexScalarQuantity;

    // Field with one value per vertex of the mesh.
    MeshScalarField(const Mesh::MeshPtr& mesh, const std::string& name,
                    const scalars& values, const std::string& colormap);

    // Builds the field and registers it. The name identifies it in polyscope.
    static MeshScalarFieldPtr Add(const Mesh::MeshPtr& mesh, const std::string& name,
                                  const scalars& values,
                                  const std::string& colormap = "viridis");

    static MeshScalarFieldPtr Add(const Mesh::MeshPtr& mesh, const std::string& name,
                                  const Vec& values,
                                  const std::string& colormap = "viridis");

    // Fraction of the intro spent fading the mesh color to colormap(baseline).
    // The field then grows from it. Without this stage the surface color jumps.
    // 0 removes the stage.
    scalar color_split = 0.35;

    // Value that the field starts from during the intro. The default is the smallest value.
    void setBaseline(scalar v);

    // Quantity of polyscope, for direct access.
    Quantity* getQuantity() const { return q; }

    const scalars& getValues() const { return values; }

    // Primitive interface
public:
    void initPolyscope() override;
    bool isScreenSpace() const override { return false; }

protected:
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;
    void forceEnable() override;
    void forceDisable() override;

private:
    Mesh::MeshPtr mesh;
    std::string name, colormap;
    scalars values, scratch;

    Quantity* q = nullptr;
    std::pair<double, double> range;
    scalar baseline = 0;
    bool baseline_set = false;

    glm::vec3 base_color = glm::vec3(0.f);
    glm::vec3 field_color = glm::vec3(0.f);

    scalar last_p = -1;

    // Finds the colors of the mesh and of the field.
    void resolveColors();
    // Draws the field at progress u, between the flat start and the full field.
    void apply(scalar u);
    // Sends to polyscope the values blended between the baseline and the field at progress p.
    void uploadField(scalar p);
};

} // namespace slope

#endif // MESHSCALARFIELD_H
