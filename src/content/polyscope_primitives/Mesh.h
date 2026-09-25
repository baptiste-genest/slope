#ifndef MESH_H
#define MESH_H
#include "libslope.h"
#include "content/config/io.h"
#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "math/geometry.h"

namespace slope {

// A triangle or polygon mesh.
class Mesh : public PolyscopePrimitive
  {
  public:

    // Gives a new position for a vertex.
    using VertexMap = std::function<vec(const Vertex&)>;

    using MeshPtr = std::shared_ptr<Mesh>;
    Mesh() {}


    /// Builds a mesh from its vertices and faces.
    /// @param vertices the vertex positions
    /// @param faces the faces, as lists of vertex indices
    /// @param smooth if true, uses smooth shading
    Mesh(const vecs &vertices, const Faces &faces,bool smooth = false);


    // Loads a mesh from an .obj file.
    static MeshPtr Add(const std::string& objfile,bool smooth = true);

    // Builds a mesh from vertices and faces.
    static MeshPtr Add(const vecs& V,const Faces& F,bool smooth = true){
      return NewPrimitive<Mesh>(V,F,smooth);
    }

    // Turns smooth shading on or off.
    void setSmooth(bool set);

    using scalar_func = std::function<scalar(const vec&)>;
    using vertex_func = std::function<scalar(const Vertex&)>;
    using vector_func = std::function<vec(const vec&)>;

    // Evaluates f at every vertex, by position or by vertex.
    Vec eval(const scalar_func& f) const;
    Vec eval(const vertex_func& f) const;
    // Evaluates a vector function at every vertex position.
    vecs eval(const vector_func& f) const;

    // Structure of polyscope, for direct access.
    polyscope::SurfaceMesh* pc;

    const vecs& getVertices() const {return vertices;}
    const Faces& getFaces() const {return faces;}

    // Moves the vertices to X, which must have the same size.
    void updateMesh(const vecs& X);

    // Centers the mesh and scales its longest side to 1.
    void normalize();

    size_t vertexCount() const override { return vertices.size(); }

protected:
    vec localVertex(size_t i) const override { return vertices[i]; }

    vecs vertices;
    bool smooth = false;
    Faces faces;

    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
  };

  }

#endif // MESH_H
