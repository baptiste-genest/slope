#include "content/polyscope_primitives/Mesh.h"
#include "math/utils.h"

slope::Mesh::MeshPtr slope::Mesh::Add(const std::string& objfile, bool smooth) {
    vecs V;
    Faces F;

    std::ifstream in(formatPath(objfile));
    if (!in)
        throw std::runtime_error("mesh: cannot open \"" + objfile + "\"");

    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        // --- vertex ---
        if (tag == "v") {
            double x, y, z;
            if (!(iss >> x >> y >> z))
                throw std::runtime_error("mesh \"" + objfile + "\": bad vertex line \"" + line + "\"");
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

                long vi;
                try {
                    vi = std::stol(v_str);
                } catch (const std::exception&) {
                    throw std::runtime_error("mesh \"" + objfile + "\": bad face line \"" + line + "\"");
                }

                // OBJ supports negative indices
                if (vi < 0)
                    vi = static_cast<long>(V.size()) + vi;
                else
                    vi = vi - 1; // 1-based → 0-based

                if (vi < 0 || vi >= static_cast<long>(V.size()))
                    throw std::runtime_error("mesh \"" + objfile + "\": face index out of range in \"" + line + "\"");
                face.push_back(vi);
            }

            if (!face.empty())
                F.push_back(std::move(face));
        }
    }

    if (V.empty() || F.empty())
        throw std::runtime_error("mesh \"" + objfile + "\": no vertices or no faces");

    MeshPtr rslt = NewPrimitive<Mesh>(V, F, smooth);
    return rslt;
}

void slope::Mesh::setSmooth(bool set) {
    if (set) {
        pc->setSmoothShade(true);
        pc->setEdgeWidth(0);
    } else {
        pc->setSmoothShade(false);
        pc->setEdgeWidth(1);
    }
}

slope::Vec slope::Mesh::eval(const scalar_func& f) const {
    Vec X(vertices.size());
    for (int i = 0; i < vertices.size(); i++)
        X[i] = f(vertices[i]);
    return X;
}

slope::Vec slope::Mesh::eval(const vertex_func& f) const {
    Vec X(vertices.size());
    for (int i = 0; i < vertices.size(); i++)
        X[i] = f(Vertex{vertices[i], i});
    return X;
}

slope::vecs slope::Mesh::eval(const vector_func& f) const {
    vecs X(vertices.size());
    for (int i = 0; i < vertices.size(); i++)
        X[i] = f(vertices[i]);
    return X;
}

void slope::Mesh::updateMesh(const vecs& X) {
    vertices = X;
    pc->updateVertexPositions(vertices);
}

void slope::Mesh::normalize() {
    normalizeToUnitCube(vertices);
    updateMesh(vertices);
}

void slope::Mesh::initPolyscope() {
    pc = polyscope::registerSurfaceMesh(getPolyscopeName(), vertices, faces);
    pc->setBackFacePolicy(polyscope::BackFacePolicy::Identical);
    initPolyscopeData(pc);
    setSmooth(smooth);
}

slope::Mesh::Mesh(const vecs& vertices, const Faces& faces, bool smooth) : vertices(vertices), faces(faces), smooth(smooth) {
}
