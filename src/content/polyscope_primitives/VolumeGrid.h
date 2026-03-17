#ifndef SLOPE_VOLUMEGRID_H
#define SLOPE_VOLUMEGRID_H

#include "PolyscopePrimitive.h"
#include "polyscope/volume_grid.h"


namespace slope {

class VolumeGrid : public PolyscopePrimitive
{
    int Nx,Ny,Nz;
    vec low,high;

public:
    VolumeGrid(int N,scalar l) : VolumeGrid(N,N,N,vec(-l,-l,-l),vec(l,l,l)) {

    }
    VolumeGrid(int Nx,int Ny,int Nz,vec l,vec h) : Nx(Nx),Ny(Ny),Nz(Nz),low(l),high(h){
    }

    using VolumeGridPtr = std::shared_ptr<VolumeGrid>;
    static VolumeGridPtr Add(int N,scalar l);
    static VolumeGridPtr Add(int Nx,int Ny,int Nz,vec l,vec h);

    polyscope::VolumeGrid* pc;

    int getNbVariables() const {
        return Nx*Ny*Nz;
    }

    int ix(int i,int j,int k) const {
        return k*Ny*Nx + j*Nx + i;
    }

    Vec eval(const std::function<scalar(vec)>& f) const;

    scalar interpolate(const vec& x,const Vec& f) const;

    // Primitive interface
public:
    void initPolyscope() {
        pc = polyscope::registerVolumeGrid(getPolyscopeName(),{Nx,Ny,Nz},{low(0),low(1),low(2)},{high(0),high(1),high(2)});
        initPolyscopeData(pc);
    }
};

} // namespace slope

#endif // SLOPE_VOLUMEGRID_H
