#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"

namespace slope {

// A set of points drawn as spheres.
class PointCloud : public PolyscopePrimitive {
public:
    PointCloud(const vecs& P, LiveScalar radius);

    using PointCloudPtr = std::shared_ptr<PointCloud>;

    // Builds a cloud from points. A negative radius keeps the default of polyscope.
    static PointCloudPtr Add(const vecs& P, LiveScalar radius = -1);
    // Builds a cloud from the vertices of a .ply file (ascii or binary) or of an .obj file.
    static PointCloudPtr Add(const std::string& file, LiveScalar radius = -1);
    // Returns a new cloud with phi applied to every point.
    PointCloudPtr apply(const mapping& phi);
    // Returns a new cloud where phi is applied to the original points every frame.
    PointCloudPtr applyDynamic(const VertexTimeMap& phi);

    // Radius in world units. A negative value keeps the default of polyscope.
    // A name that no snippet gives becomes a slider in the Tuner.
    void setRadius(const LiveScalar& r);
    const LiveScalar& getRadius() const { return radius; }

    // Structure of polyscope, for direct access.
    polyscope::PointCloud* pc = nullptr;
    const vecs& getPoints() const { return points; }
    // Moves the points to X.
    void updateCloud(const vecs& X) {
        points = X;
        pc->updatePointPositions(points);
    }

    // Centers the cloud and scales its longest side to 1.
    void normalize();

    size_t vertexCount() const override { return points.size(); }

protected:
    vec localVertex(size_t i) const override { return points[i]; }

private:
    vecs points, original_points;
    LiveScalar radius = -1;
    std::optional<scalar> applied_radius;

    // Sends the radius to polyscope only when it changed, so a radius set directly on pc is kept.
    void syncRadius();

    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
    void draw(const TimeObject& t, const StateInSlide& sis) override;
    void playIntro(const TimeObject& t, const StateInSlide& sis) override;
    void playOutro(const TimeObject& t, const StateInSlide& sis) override;
};

// Uses a scalar quantity of a point cloud as its transparency during the slides.
class PointCloudTransparencyQuantity : public Primitive {
public:
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
    void draw(const TimeObject& time, const StateInSlide& sis) override {
        q->parent.setTransparencyQuantity(q);
    }
    void playIntro(const TimeObject& t, const StateInSlide& sis) override {
        q->parent.setTransparencyQuantity(q);
    }
    void playOutro(const TimeObject& t, const StateInSlide& sis) override {
        q->parent.clearTransparencyQuantity();
    }
    void forceDisable() override { q->parent.clearTransparencyQuantity(); }
    bool isScreenSpace() const override { return false; }
};

} // namespace slope

#endif // POINTCLOUD_H
