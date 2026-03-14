#include "PointCloud.h"

slope::PointCloud::PointCloud(const vecs &P,scalar r) : points(P),original_points(P),radius(r)
{
}

slope::PointCloud::PointCloudPtr slope::PointCloud::Add(const vecs &P,scalar radius)
{
    return NewPrimitive<PointCloud>(P,radius);
}

slope::PointCloud::PointCloudPtr slope::PointCloud::apply(const mapping &phi)
{
    auto NP = points;
    for (auto& x : NP)
        x = phi(x);
    return Add(NP,radius);
}

slope::PointCloud::PointCloudPtr slope::PointCloud::applyDynamic(const VertexTimeMap &phi)
{
    PointCloudPtr rslt = NewPrimitive<PointCloud>(original_points,radius);
    rslt->updater = [phi,rslt] (const TimeObject& t) {
        auto V = rslt->original_points;
        for (int i = 0;i<V.size();i++)
            V[i] = phi({V[i],i},t);
        rslt->updateCloud(V);
    };
    return rslt;
}

void slope::PointCloud::initPolyscope()
{
    pc = polyscope::registerPointCloud(getPolyscopeName(),points);
    if (radius > 0)
        pc->setPointRadius(radius,false);
    pc->setPointColor(getColor());
    initPolyscopeData(pc);
}
