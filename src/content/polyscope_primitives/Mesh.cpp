#include "Mesh.h"


slope::Mesh::MeshPtr slope::Mesh::Add(const std::string &objfile,bool smooth)
{
    vecs V;
    Faces F;

    std::ifstream in(formatPath(objfile));

    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        // --- vertex ---
        if (tag == "v") {
            double x, y, z;
            iss >> x >> y >> z;
            V.emplace_back(x, y, z);
        }

        // --- face ---
        else if (tag == "f") {
            Face face;
            std::string token;

            while (iss >> token) {
                std::istringstream tss(token);
                std::string v_str;

                // read only vertex index before '/'
                std::getline(tss, v_str, '/');

                long vi = std::stol(v_str);

                // OBJ supports negative indices
                if (vi < 0)
                    vi = static_cast<long>(V.size()) + vi;
                else
                    vi = vi - 1; // 1-based → 0-based

                face.push_back(vi);
            }

            if (!face.empty())
                F.push_back(std::move(face));
        }
    }

    MeshPtr rslt = NewPrimitive<Mesh>(V,F,smooth);
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
}

void slope::Mesh::initPolyscope()
{
    pc = polyscope::registerSurfaceMesh(getPolyscopeName(),vertices,faces);
    pc->setBackFacePolicy(polyscope::BackFacePolicy::Identical);
    initPolyscopeData(pc);
    setSmooth(smooth);
    pc->setSurfaceColor(getColor());
}


slope::Mesh::Mesh(const vecs &vertices, const Faces &faces, bool smooth) : vertices(vertices),faces(faces),smooth(smooth)
{
}
