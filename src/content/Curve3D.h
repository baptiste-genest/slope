#ifndef CURVE3D_H
#define CURVE3D_H

#include "PolyscopePrimitive.h"
#include "../math/Parametrization.h"

namespace slope {

class Curve3D : public PolyscopePrimitive
{
public:
    Curve3D() {}
    using Curve3DPtr = std::shared_ptr<Curve3D>;

    static Curve3DPtr Add(const vecs& nodes, bool loop = false, scalar r = -0.01);
    static Curve3DPtr AddSegments(const vecs& nodes, scalar r = -0.01);
    static Curve3DPtr Add(const curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    static Curve3DPtr Add(const dynamic_curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    using edge = std::array<int,2>;
    using edges = std::vector<edge>;


    polyscope::CurveNetwork* pc;


    Curve3DPtr apply(const mapping& phi,bool loop = false) const;

    void updateNodes(const vecs& X) {
        nodes = X;
        pc->updateNodePositions(nodes);
    }

    const vecs& getNodes() const {return nodes;}

    scalar radius = 0.01;
protected:
    bool loop;
    vecs nodes;

    // PolyscopePrimitive interface
public:
    Curve3D(const vecs &nodes,bool loop,scalar r);
    Curve3D(const vecs &nodes,scalar r);
    Curve3D(const curve_param& param,int N = 100,bool loop = false,scalar r = -0.01);
    virtual void initPolyscope() override;
};

class CurveNetwork : public Curve3D {
protected:
    edges E;

    // Primitive interface
public:

    using CurveNetworkPtr = std::shared_ptr<CurveNetwork>;

    virtual void initPolyscope() override;
    static CurveNetworkPtr Add(const vecs& nodes,const edges& E,scalar r = -0.01);
    static CurveNetworkPtr AddSegments(const vecs& nodes,scalar r = -0.01);
    CurveNetwork(const vecs &nodes,const edges& E,scalar r);
    CurveNetwork(const vecs &nodes,scalar r);

    void updateEdges(const edges& E) {
        this->E = E;
        pc = polyscope::registerCurveNetwork(getPolyscopeName(),nodes,E);
        polyscope_ptr = pc;
    }

    void updateSegments(const vecs& V) {
        nodes = V;
        E.clear();
        for (int i = 0;i<V.size()-1;i++)
            E.push_back({i,i+1});
        updateEdges(E);
    }
};


}

#endif // CURVE3D_H
