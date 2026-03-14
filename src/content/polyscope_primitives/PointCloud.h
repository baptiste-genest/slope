#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "PolyscopePrimitive.h"

namespace slope {

class PointCloud : public PolyscopePrimitive
{
public:
    PointCloud(const vecs& P,scalar radius);

    using PointCloudPtr = std::shared_ptr<PointCloud>;

    static PointCloudPtr Add(const vecs& P,scalar radius = -1);
    PointCloudPtr apply(const mapping& phi);
    PointCloudPtr applyDynamic(const VertexTimeMap& phi);

    polyscope::PointCloud* pc;
    const vecs& getPoints() const {return points;}
    void updateCloud(const vecs& X) {
        points = X;
        pc->updatePointPositions(points);
    }
private:
    vecs points,original_points;
    scalar radius = -1;

    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
};

class PointCloudTransparencyQuantity : public Primitive
{
public :
    using T = polyscope::PointCloudScalarQuantity;
    using Ptr = std::shared_ptr<PointCloudTransparencyQuantity>;
    static Ptr Add(T* ptr) {
        auto rslt = NewPrimitive<PointCloudTransparencyQuantity>();
        rslt->q = ptr;
        return rslt;
    }

    // Primitive interface
public:

    T* q;
    void draw(const TimeObject &time, const StateInSlide &sis) override {
        q->parent.setTransparencyQuantity(q);
    }
    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        q->parent.setTransparencyQuantity(q);
    }
    void playOutro(const TimeObject& t, const StateInSlide &sis) override {
        q->parent.clearTransparencyQuantity();
    }
    void forceDisable() override {q->parent.clearTransparencyQuantity();}
    bool isScreenSpace() const override {return false;}
};

}

#endif // POINTCLOUD_H
