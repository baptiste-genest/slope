#ifndef CURVE3D_H
#define CURVE3D_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"

namespace slope {

// A polyline drawn as a tube. A negative radius is relative to the size of the scene, as in polyscope.
class Curve3D : public PolyscopePrimitive
{
public:
    Curve3D() {}
    using Curve3DPtr = std::shared_ptr<Curve3D>;

    // Builds a curve through the nodes, closed if loop is true.
    static Curve3DPtr Add(const vecs& nodes, bool loop = false, scalar r = -0.01);
    // Builds a set of separate segments, where nodes 2i and 2i+1 are joined.
    static Curve3DPtr AddSegments(const vecs& nodes, scalar r = -0.01);
    // Builds a curve from a parametrization sampled at N points on [0,1].
    static Curve3DPtr Add(const curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    // Same, where the parametrization also depends on time and moves.
    static Curve3DPtr Add(const dynamic_curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    using edge = std::array<int,2>;
    using edges = std::vector<edge>;


    // Structure of polyscope, for direct access.
    polyscope::CurveNetwork* pc;

    // Returns a new curve with phi applied to every node.
    Curve3DPtr apply(const mapping& phi,bool loop = false) const;

    // Moves the nodes to X.
    void updateNodes(const vecs& X) {
        nodes = X;
        pc->updateNodePositions(nodes);
    }

    const vecs& getNodes() const {return nodes;}

    // Tube radius.
    scalar radius = 0.01;

    size_t vertexCount() const override { return nodes.size(); }
protected:
    vec localVertex(size_t i) const override { return nodes[i]; }

    bool loop;
    vecs nodes;

    // PolyscopePrimitive interface
public:
    Curve3D(const vecs &nodes,bool loop,scalar r);
    Curve3D(const vecs &nodes,scalar r);
    Curve3D(const curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    virtual void initPolyscope() override;
};

// A set of nodes joined by arbitrary edges.
class CurveNetwork : public Curve3D {
protected:
    edges E;

    // Primitive interface
public:

    using CurveNetworkPtr = std::shared_ptr<CurveNetwork>;

    virtual void initPolyscope() override;
    // Builds a network from nodes and edges given as pairs of node indices.
    static CurveNetworkPtr Add(const vecs& nodes,const edges& E,scalar r = -0.01);
    // Builds separate segments, where nodes 2i and 2i+1 are joined.
    static CurveNetworkPtr AddSegments(const vecs& nodes,scalar r = -0.01);
    CurveNetwork(const vecs &nodes,const edges& E,scalar r);
    CurveNetwork(const vecs &nodes,scalar r);

    // Replaces the edges and registers the structure again.
    void updateEdges(const edges& E) {
        this->E = E;
        pc = polyscope::registerCurveNetwork(getPolyscopeName(),nodes,E);
        polyscope_ptr = pc;
        reapplyColor();
    }

    // Replaces the nodes by V and joins them two by two.
    void updateSegments(const vecs& V) {
        nodes = V;
        int n = V.size();
        E.resize(n/2);
        for (int i = 0;i<n/2;i++)
            E[i] = {i*2,i*2+1};
        updateEdges(E);
    }
};


}

#endif // CURVE3D_H
