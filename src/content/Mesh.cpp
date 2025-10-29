#include "Mesh.h"

slope::Mesh::MeshPtr slope::Mesh::Add(const std::string &objfile,const vec& scale,bool smooth)
{

    std::vector<std::array<double,3>> V;
    Faces F;
    std::unique_ptr<geometrycentral::surface::ManifoldSurfaceMesh> mesh;
    std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geometry;
    std::tie(mesh, geometry) = geometrycentral::surface::readManifoldSurfaceMesh(formatPath(objfile));

    //load faces
    for (auto f : mesh->getFaceVertexList())
        F.push_back(f);

    vecs verts(mesh->nVertices());
    for (auto v : mesh->vertices()){
        auto x = geometry->vertexPositions[v];
        verts[v.getIndex()] = vec(x[0]*scale(0),x[1]*scale(1),x[2]*scale(2));
    }

    MeshPtr rslt = NewPrimitive<Mesh>(verts,verts,F,smooth);
    return rslt;
}

slope::Mesh::MeshPtr slope::Mesh::apply(const VertexMap &phi,bool smooth) const
{
    auto Vphi = vertices;
    for (int i = 0;i<Vphi.size();i++)
        Vphi[i] = phi({Vphi[i],i});
    MeshPtr rslt = NewPrimitive<Mesh>(Vphi,original_vertices,faces,smooth);
    return rslt;
}

slope::Mesh::MeshPtr slope::Mesh::applyDynamic(const VertexTimeMap &phi,bool smooth) const
{
    MeshPtr rslt = NewPrimitive<Mesh>(vertices,original_vertices,faces,smooth);
    rslt->updater = [phi] (const TimeObject& t,Primitive* ptr) {
        auto M = Primitive::get<Mesh>(ptr->pid);
        auto V = M->original_vertices;
        for (int i = 0;i<V.size();i++)
            V[i] = phi({V[i],i},t);
        M->updateMesh(V);
    };
    return rslt;
}

void slope::Mesh::setSmooth(bool set)
{
    if (set){
        pc->setSmoothShade(true);
        pc->setEdgeWidth(0);
    }
    else {
        pc->setSmoothShade(false);
        pc->setEdgeWidth(1);
    }
}

slope::Vec slope::Mesh::eval(const scalar_func &f) const
{
    Vec X(vertices.size());
    for (int i = 0;i<vertices.size();i++)
        X[i] = f(vertices[i]);
    return X;
}

slope::Vec slope::Mesh::eval(const vertex_func &f) const
{
    Vec X(vertices.size());
    for (int i = 0;i<vertices.size();i++)
        X[i] = f(Vertex{vertices[i],i});
    return X;
}


slope::vecs slope::Mesh::eval(const vector_func &f) const
{
    vecs X(vertices.size());
    for (int i = 0;i<vertices.size();i++)
        X[i] = f(vertices[i]);
    return X;
}

void slope::Mesh::updateMesh(const vecs &X)
{
    vertices = X;
    pc->updateVertexPositions(vertices);
}

void slope::Mesh::normalize()
{
    //scale bounding box to fit in unit cube
    vec minv = vec::Constant(std::numeric_limits<double>::max());
    vec maxv = vec::Constant(std::numeric_limits<double>::lowest());
    for (const auto& v : vertices) {
        minv = minv.cwiseMin(v);
        maxv = maxv.cwiseMax(v);
    }
    vec center = 0.5 * (minv + maxv);
    double max_extent = (maxv - minv).maxCoeff();
    for (auto& v : vertices) {
        v = (v - center) / max_extent;
    }
    updateMesh(vertices);
    original_vertices = vertices;
}

void slope::Mesh::initPolyscope()
{
    pc = polyscope::registerSurfaceMesh(getPolyscopeName(),vertices,faces);
    pc->setBackFacePolicy(polyscope::BackFacePolicy::Identical);
    initPolyscopeData(pc);
    setSmooth(smooth);
    pc->setSurfaceColor(getColor());
}


slope::Mesh::Mesh(const vecs &vertices, const vecs &original_vertices, const Faces &faces, bool smooth) : vertices(vertices),
    original_vertices(original_vertices),
    faces(faces),smooth(smooth)
{
}
