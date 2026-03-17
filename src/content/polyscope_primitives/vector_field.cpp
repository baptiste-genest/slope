#include "vector_field.h"


namespace slope {
VectorField::VectorField(const vecs &X, const vecs &V,double l) : V(V),
    X(X),length(l)
{
}

VectorField::VectorFieldPtr VectorField::AddOnGrid(const vecs &V)
{
    int n = std::cbrt(V.size());
    if (n*n*n != V.size()){
        std::cerr << "VectorField::AddOnGrid: V.size() must be a perfect cube " << std::endl;
        exit(1);
    }
    auto coord = buildRangeMapper(0,n-1,-0.5,0.5);
    vecs X;
    for (int i = 0;i<n;i++)
        for (int j = 0;j<n;j++)
            for (int k = 0;k<n;k++)
                X.push_back(vec(coord(i),coord(j),coord(k)));
    return Add(X,V,std::sqrt(2)/(n-1));
}

VectorField::VectorFieldPtr VectorField::EvalOnGrid(const std::function<vec (vec)> &vf, int n,scalar l)
{
    auto coord = buildRangeMapper(0,n-1,-0.5*l,l*0.5);
    vecs X,V;
    for (int i = 0;i<n;i++)
        for (int j = 0;j<n;j++)
            for (int k = 0;k<n;k++){
                V.push_back(vf(vec(coord(i),coord(j),coord(k))));
                X.push_back(vec(coord(i),coord(j),coord(k)));
            }
    return Add(X,V,std::sqrt(2)*l/(n-1));

}

void VectorField::initPolyscope()
{
    pc = polyscope::registerPointCloud(getPolyscopeName(),X);
    pc->setPointRadius(0);
    pc->setEnabled(false);
    pq = pc->addVectorQuantity("V",V);
    pq->setVectorRadius(length/20,false);
}

}
