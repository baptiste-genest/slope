#include "Mesh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "../../extern/tiny_obj_loader.h"

slope::Mesh::MeshPtr slope::Mesh::Add(const std::string &objfile,bool smooth)
{
    vecs V;
    Faces F;
    struct CallbackData {
        vecs* V;
        Faces* F;
    };

    CallbackData data{&V, &F};

    tinyobj::callback_t cb{};

    // --- vertex callback ---
    cb.vertex_cb = [](void* user_data, float x, float y, float z, float /*w*/) {
        auto* d = reinterpret_cast<CallbackData*>(user_data);
        d->V->emplace_back(x, y, z);
    };

    // --- face callback (polygonal) ---
    cb.index_cb = [](void* user_data, tinyobj::index_t* indices, int num_indices) {
        auto* d = reinterpret_cast<CallbackData*>(user_data);

        std::vector<unsigned long> face;
        face.reserve(num_indices);

        for (int i = 0; i < num_indices; ++i) {
            // OBJ is 1-based → convert to 0-based
            face.push_back(static_cast<unsigned long>(indices[i].vertex_index - 1));
        }

        d->F->push_back(std::move(face));
    };

    std::ifstream ifs(formatPath(objfile));

    std::string warn, err;

    bool ret = tinyobj::LoadObjWithCallback(
        ifs,
        cb,
        &data,
        nullptr,   // no materials
        &warn,
        &err);

    // if (!warn.empty()) spdlog::warn("WARN: {}", warn);
    if (!err.empty())  spdlog::error("ERROR: {}", err);
    if (!ret) {
        throw std::runtime_error("Failed to parse obj\n");
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
