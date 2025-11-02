#ifndef POINT_H
#define POINT_H

#include "PolyscopePrimitive.h"
#include "../math/Parametrization.h"

namespace slope {

using DynamicParam = std::function<vec(const TimeObject&)>;

class Point : public PolyscopePrimitive
{
public:
    using VectorQuantity = PolyscopeQuantity<polyscope::PointCloudVectorQuantity>;
    using VectorQuantityPtr = VectorQuantity::PCQuantityPtr;
    Point(const DynamicParam &phi, scalar radius);

    Point(const vec &x,scalar radius) : x(x),radius(radius) {
        phi = [x](TimeObject){return x;};
        updater = [](const TimeObject& t,Primitive* ptr){
            auto p = Primitive::get<Point>(ptr->pid);
            p->updateVectors(t);
        };
    }

    Point() {}
    using PointPtr = std::shared_ptr<Point>;
    
    static PointPtr Add(const curve_param& phi,scalar rad = 0.05);
    static PointPtr Add(const DynamicParam& phi,scalar rad = 0.05);
    static PointPtr Add(const vec& x,scalar rad = 0.05);
    
    VectorQuantityPtr addVector(const vec& v,scalar rad= 0.02);
    VectorQuantityPtr addVector(const curve_param& phi,scalar rad= 0.02);
    VectorQuantityPtr addVector(const DynamicParam& phi,scalar rad = 0.02);
    polyscope::PointCloud* pc;

    void setPos(const vec& x);

    void updateVectors(const TimeObject& t);
    PointPtr apply(const mapping& f) const;
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
