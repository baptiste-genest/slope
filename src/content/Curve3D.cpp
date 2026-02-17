#include "Curve3D.h"

slope::Curve3D::Curve3DPtr slope::Curve3D::Add(const vecs &nodes, bool loop,scalar r)
{
    return NewPrimitive<Curve3D>(nodes,loop,r);
}

slope::Curve3D::Curve3DPtr slope::Curve3D::Add(const curve_param &param,int N, bool loop,scalar r)
{
    return NewPrimitive<Curve3D>(param,N,loop,r);
}

slope::Curve3D::Curve3DPtr slope::Curve3D::Add(const dynamic_curve_param &param,int N, bool loop,scalar r)
{
    auto C = NewPrimitive<Curve3D>(vecs(N,vec::Zero()),loop,r);
    C->updater = [param,loop,N](const TimeObject& time,Primitive* ptr) {
        auto C = Primitive::get<Curve3D>(ptr->pid);
        auto X = C->nodes;
        scalar dt = loop ? 1./N : 1./(N-1);
        for (int i = 0;i<X.size();i++){
            scalar t = i*dt;
            X[i] = param(t,time);
        }
        C->updateNodes(X);
    };
    return C;
}

slope::Curve3D::Curve3DPtr slope::Curve3D::apply(const mapping &phi,bool loop) const
{
    auto X = nodes;
    for (auto& x : X)
        x = phi(x);
    auto C = NewPrimitive<Curve3D>(X,loop,radius);
    return C;
}

void slope::Curve3D::initPolyscope()
{
    if (loop)
        pc = polyscope::registerCurveNetworkLoop(getPolyscopeName(),nodes);
    else
        pc = polyscope::registerCurveNetworkLine(getPolyscopeName(),nodes);
    if (radius > 0 ) {
        pc->setRadius(radius,false);
        pc->setColor(getColor());
    }
    initPolyscopeData(pc);
}


namespace slope {
Curve3D::Curve3D(const vecs &nodes,bool loop,scalar r) : loop(loop),
    nodes(nodes),radius(r)
{
}

Curve3D::Curve3D(const curve_param &param, int N, bool loop,scalar r) : loop(loop),radius(r)
{
    nodes.resize(N);
    scalar dt = loop ? 1./N : 1./(N-1);

    for (int i  = 0;i<N;i++){
        auto t = i*dt;
        nodes[i] = param(t);
    }
}

void CurveNetwork::initPolyscope()
{
    pc = polyscope::registerCurveNetwork(getPolyscopeName(),nodes,E);
    if (radius > 0)
        pc->setRadius(radius,false);
    initPolyscopeData(pc);
}

CurveNetwork::CurveNetworkPtr CurveNetwork::Add(const vecs &nodes, const edges &E, scalar r)
{
    return NewPrimitive<CurveNetwork>(nodes,E,r);
}
CurveNetwork::CurveNetworkPtr CurveNetwork::AddSegments(const vecs &nodes, scalar r)
{
    int n = nodes.size();
    edges E(n/2);
    for (int i = 0;i<n/2;i++)
        E[i] = {i*2,i*2+1};
    return NewPrimitive<CurveNetwork>(nodes,E,r);
}

CurveNetwork::CurveNetwork(const vecs &nodes,const edges& E, scalar r) : E(E){
    Curve3D::nodes = nodes;
    Curve3D::radius = r;
}


}
