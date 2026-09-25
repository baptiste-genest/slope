#ifndef VECTORFIELD_H
#define VECTORFIELD_H
#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "math/utils.h"

namespace slope {

// Arrows drawn at points. Their length grows from zero during the intro.
class VectorField : public PolyscopePrimitive {
private:
    vecs V, X;
    scalar length = 1;
    polyscope::PointCloud* pc;
    polyscope::PointCloudVectorQuantity* pq;

    // Primitive interface
public:
    // Field with the vector V[i] at the point X[i], and arrow length scale l0.
    VectorField(const vecs& X, const vecs& V, double l0 = 0.1);
    virtual void draw(const TimeObject& time, const StateInSlide& sis) override {
        pc->setEnabled(true);
        pq->setEnabled(true);
        pq->setVectorLengthScale(length, false);
    }
    virtual void playIntro(const TimeObject& t, const StateInSlide& sis) override {
        pq->setVectorLengthScale(t.transition_parameter * length, false);
    }
    virtual void playOutro(const TimeObject& t, const StateInSlide& sis) override {
        pq->setVectorLengthScale((1 - t.transition_parameter) * length, false);
    }
    virtual void forceDisable() override {
        pc->setEnabled(false);
        pq->setEnabled(false);
    }
    virtual void forceEnable() override {
        pc->setEnabled(true);
        pq->setEnabled(true);
    }

    using VectorFieldPtr = std::shared_ptr<VectorField>;
    // Builds a field with arrow length scale l.
    inline static VectorFieldPtr Add(const vecs& X, const vecs& V, scalar l) {
        return NewPrimitive<VectorField>(X, V, l);
    }
    // Builds a field on a regular grid in [-0.5,0.5]^3. The number of vectors must be a cube.
    static VectorFieldPtr AddOnGrid(const vecs& V);
    // Builds a field by evaluating V on an n by n by n grid in the cube of side l centered at the origin.
    static VectorFieldPtr EvalOnGrid(const std::function<vec(vec)>& V, int n = 10, scalar l = 1);

    // Primitive interface
public:
    virtual void initPolyscope() override;
};

} // namespace slope

#endif // VECTORFIELD_H
