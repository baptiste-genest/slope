#ifndef MESH_H
#define MESH_H
#include "../libslope.h"
#include "io.h"
#include "PolyscopePrimitive.h"
#include "../math/geometry.h"

namespace slope {
  
  class Mesh : public PolyscopePrimitive
  {
  public:
    
    
    using VertexMap = std::function<vec(const Vertex&)>;
    
    using MeshPtr = std::shared_ptr<Mesh>;
    Mesh() {}
    
    
    /// Add a mesh from its vertices and faces.
    /// @param vertices the list of vertex positions
    /// @param original_vertices the list of original vertex positions
    /// @param faces the list of faces
    /// @param smooth if true, enable smooth rendering (default: false)
    Mesh(const vecs &vertices,const vecs& original_vertices, const Faces &faces,bool smooth = false);
    
    
    static MeshPtr Add(const std::string& objfile,const vec& scale = vec(1.,1.,1.),bool smooth = true);
    
    static MeshPtr Add(const std::string& objfile,scalar scale,bool smooth = true)
    {
      return Add(objfile,vec::Ones()*scale,smooth);
    }
    
    static MeshPtr Add(const vecs& V,const Faces& F,bool smooth = true){
      return NewPrimitive<Mesh>(V,V,F,smooth);
    }
    
    MeshPtr apply(const VertexMap& phi,bool smooth = true) const;
   
    MeshPtr apply(const mapping& phi,bool smooth = true) const {
      return apply([phi](Vertex v){return phi(v.pos);},smooth);
    }
    MeshPtr applyDynamic(const VertexTimeMap& phi,bool smooth = true) const;
    
    void setSmooth(bool set);
    
    using scalar_func = std::function<scalar(const vec&)>;
    using vertex_func = std::function<scalar(const Vertex&)>;
    using vector_func = std::function<vec(const vec&)>;
    
    Vec eval(const scalar_func& f) const;
    Vec eval(const vertex_func& f) const;
    vecs eval(const vector_func& f) const;
    
    polyscope::SurfaceMesh* pc;
    
    const vecs& getVertices() const {return vertices;}
    const Faces& getFaces() const {return faces;}
    
    void updateMesh(const vecs& X);

    MeshPtr translate(const vec& x) const {
      return apply([x](const vec& v) {return vec(x+v);});
    }

    MeshPtr scale(scalar x) const {
      return apply([x](const vec& v) {return vec(x*v);});
    }

    MeshPtr copy() const { return translate(vec::Zero());}

    void normalize();

private:
    vecs vertices,original_vertices;
    bool smooth = false;
    Faces faces;

    // PolyscopePrimitive interface
public:
    virtual void initPolyscope() override;
  };

  }

#endif // MESH_H
