#ifndef MESHSCALARFIELD_H
#define MESHSCALARFIELD_H

#include "../Mesh.h"

namespace polyscope {
class SurfaceVertexScalarQuantity;
}

namespace slope {

class MeshScalarField;
using MeshScalarFieldPtr = std::shared_ptr<MeshScalarField>;

class MeshScalarField : public Primitive
{
public:
    using Quantity = polyscope::SurfaceVertexScalarQuantity;

    MeshScalarField(const Mesh::MeshPtr& mesh, const std::string& name,
                    const scalars& values, const std::string& colormap);

    static MeshScalarFieldPtr Add(const Mesh::MeshPtr& mesh, const std::string& name,
                                  const scalars& values,
                                  const std::string& colormap = "viridis");

    static MeshScalarFieldPtr Add(const Mesh::MeshPtr& mesh, const std::string& name,
                                  const Vec& values,
                                  const std::string& colormap = "viridis");

    // fraction of the intro spent fading the mesh colour to colormap(baseline)
    // before the quantity takes over and the field grows out of it ; without it
    // the surface colour pops. 0 disables the stage.
    scalar color_split = 0.35;

    void setBaseline(scalar v);

    Quantity* getQuantity() const {return q;}

    const scalars& getValues() const {return values;}

    // Primitive interface
public:
    void initPolyscope() override;
    bool isScreenSpace() const override {return false;}

protected:
    void draw(const TimeObject& t,const StateInSlide& sis) override;
    void playIntro(const TimeObject& t,const StateInSlide& sis) override;
    void playOutro(const TimeObject& t,const StateInSlide& sis) override;
    void forceEnable() override;
    void forceDisable() override;

private:
    Mesh::MeshPtr mesh;
    std::string name, colormap;
    scalars values, scratch;

    Quantity* q = nullptr;
    std::pair<double,double> range;
    scalar baseline = 0;
    bool baseline_set = false;

    glm::vec3 base_color = glm::vec3(0.f);
    glm::vec3 field_color = glm::vec3(0.f);

    scalar last_p = -1;

    void resolveColors();
    void apply(scalar u);
    void uploadField(scalar p);
};

}

#endif // MESHSCALARFIELD_H
