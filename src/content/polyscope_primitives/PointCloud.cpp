#include "content/polyscope_primitives/PointCloud.h"
#include "content/authoring/Params.h"
#include "math/utils.h"
#include "happly.h"
#include <spdlog/spdlog.h>
#include <set>

namespace {

slope::vecs readPly(const std::string& file)
{
    happly::PLYData ply(file);
    slope::vecs P;
    for (const auto& x : ply.getVertexPositions())
        P.emplace_back(x[0], x[1], x[2]);
    return P;
}

slope::vecs readObjVertices(const std::string& file)
{
    std::ifstream in(file);
    if (!in)
        throw std::runtime_error("cannot open point cloud file " + file);
    slope::vecs P;
    std::string line, tag;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        double x, y, z;
        if (iss >> tag && tag == "v" && iss >> x >> y >> z)
            P.emplace_back(x, y, z);
    }
    return P;
}

}

slope::PointCloud::PointCloud(const vecs &P,LiveScalar r) : points(P),original_points(P),radius(r)
{
}

slope::PointCloud::PointCloudPtr slope::PointCloud::Add(const vecs &P,LiveScalar radius)
{
    return NewPrimitive<PointCloud>(P,radius);
}

slope::PointCloud::PointCloudPtr slope::PointCloud::Add(const std::string &file,LiveScalar radius)
{
    const std::string full = formatPath(file);
    std::string ext = path(file).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    vecs P;
    if (ext == ".ply")
        P = readPly(full);
    else if (ext == ".obj")
        P = readObjVertices(full);
    else
        throw std::runtime_error("point cloud file " + file + " must be a .ply or a .obj");
    if (P.empty())
        throw std::runtime_error("point cloud file " + file + " has no vertices");
    return Add(P,radius);
}

void slope::PointCloud::normalize()
{
    normalizeToUnitCube(points);
    original_points = points;
    pc->updatePointPositions(points);
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

void slope::PointCloud::setRadius(const LiveScalar &r)
{
    radius = r;
    applied_radius.reset();
    syncRadius();
}

void slope::PointCloud::syncRadius()
{
    if (!pc)
        return;
    scalar r = radius.fixed;
    if (radius.live()) {
        const Snippet::Value v = Snippet::get(radius.snippet);
        if (v.n == 1)
            r = v.v[0];
        // sections have not run before the first frame, and may own the name
        else if (!v.valid() && Snippet::ready())
            r = Params::AddAtLeast(radius.snippet, pc->getPointRadius(), 0);
        else {
            static std::set<std::string> said;
            if (v.valid() && said.insert(radius.snippet).second)
                spdlog::error("cloud radius : \"{}\" gives {} numbers", radius.snippet, v.n);
            return;
        }
    }
    if (r <= 0 || (applied_radius && *applied_radius == r))
        return;
    pc->setPointRadius(r,false);
    applied_radius = r;
}

void slope::PointCloud::initPolyscope()
{
    pc = polyscope::registerPointCloud(getPolyscopeName(),points);
    applied_radius.reset();
    syncRadius();
    initPolyscopeData(pc);
}

void slope::PointCloud::draw(const TimeObject &t, const StateInSlide &sis)
{
    syncRadius();
    PolyscopePrimitive::draw(t,sis);
}

void slope::PointCloud::playIntro(const TimeObject &t, const StateInSlide &sis)
{
    syncRadius();
    PolyscopePrimitive::playIntro(t,sis);
}

void slope::PointCloud::playOutro(const TimeObject &t, const StateInSlide &sis)
{
    syncRadius();
    PolyscopePrimitive::playOutro(t,sis);
}
