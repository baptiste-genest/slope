#ifndef POINT_H
#define POINT_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"

namespace slope {

// Gives a position at a given time.
using DynamicParam = std::function<vec(const TimeObject&)>;

// A single point drawn as a sphere, which can move and carry vectors.
class Point : public PolyscopePrimitive
{
public:
    // A vector attached to the point.
    using VectorQuantity = PolyscopeQuantity<polyscope::PointCloudVectorQuantity>;
    using VectorQuantityPtr = VectorQuantity::PCQuantityPtr;
    Point(const DynamicParam &phi, scalar radius);

    Point(const vec &x,scalar radius) : x(x),radius(radius) {
        phi = [x](TimeObject){return x;};
        updater = [&](const TimeObject& t){
            updateVectors(t);
        };
    }

    Point() {}
    using PointPtr = std::shared_ptr<Point>;
    
    // Builds a point with sphere radius rad, either fixed, moving with the inner time, or moving with a TimeObject.
    static PointPtr Add(const curve_param& phi,scalar rad = 0.05);
    static PointPtr Add(const DynamicParam& phi,scalar rad = 0.05);
    static PointPtr Add(const vec& x,scalar rad = 0.05);
    
    // Attaches a vector to the point, either fixed, moving with the inner time, or moving with a TimeObject.
    // The radius is the thickness of the arrow.
    VectorQuantityPtr addVector(const vec& v,scalar rad= 0.02);
    VectorQuantityPtr addVector(const curve_param& phi,scalar rad= 0.02);
    VectorQuantityPtr addVector(const DynamicParam& phi,scalar rad = 0.02);
    // Structure of polyscope, for direct access.
    polyscope::PointCloud* pc;

    // Moves the point.
    void setPos(const vec& x);

    // Recomputes the attached vectors.
    void updateVectors(const TimeObject& t);
    // Returns a new point that follows f applied to the position of this one.
    PointPtr apply(const mapping& f) const;
    // Last position set.
    vec getCurrentPos() const {return x;}
    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
private:
    vec x;
    DynamicParam phi;
    scalar radius;
    std::vector<std::pair<VectorQuantityPtr,DynamicParam>> vectors;
    std::vector<scalar> vector_radiuses;

};

}

#endif // POINT_H
