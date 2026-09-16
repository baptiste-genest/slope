#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"

namespace slope {

class PointCloud : public PolyscopePrimitive
{
public:
    PointCloud(const vecs& P,LiveScalar radius);

    using PointCloudPtr = std::shared_ptr<PointCloud>;

    static PointCloudPtr Add(const vecs& P,LiveScalar radius = -1);
    // the vertices of a .ply (ascii or binary) or of a .obj
    static PointCloudPtr Add(const std::string& file,LiveScalar radius = -1);
    PointCloudPtr apply(const mapping& phi);
    PointCloudPtr applyDynamic(const VertexTimeMap& phi);

    // world units, < 0 for polyscope's default; a name nothing gives becomes a Tuner slider
    void setRadius(const LiveScalar& r);
    const LiveScalar& getRadius() const {return radius;}

    polyscope::PointCloud* pc = nullptr;
    const vecs& getPoints() const {return points;}
    void updateCloud(const vecs& X) {
        points = X;
        pc->updatePointPositions(points);
    }

    void normalize();
private:
    vecs points,original_points;
    LiveScalar radius = -1;
    std::optional<scalar> applied_radius;

    // pushed only when it moves, so code setting pc's radius directly keeps it
    void syncRadius();

    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
    void draw(const TimeObject& t, const StateInSlide &sis) override;
    void playIntro(const TimeObject& t, const StateInSlide &sis) override;
    void playOutro(const TimeObject& t, const StateInSlide &sis) override;
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
