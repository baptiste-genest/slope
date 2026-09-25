#ifndef SLOPE_VOLUMEGRID_H
#define SLOPE_VOLUMEGRID_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "polyscope/volume_grid.h"


namespace slope {

// A regular 3D grid of samples in the box from low to high.
class VolumeGrid : public PolyscopePrimitive
{
    int Nx,Ny,Nz;
    vec low,high;

public:
    // Grid of N points per axis in the cube [-l,l]^3.
    VolumeGrid(int N,scalar l) : VolumeGrid(N,N,N,vec(-l,-l,-l),vec(l,l,l)) {

    }
    // Grid of Nx by Ny by Nz points in the box from l to h.
    VolumeGrid(int Nx,int Ny,int Nz,vec l,vec h) : Nx(Nx),Ny(Ny),Nz(Nz),low(l),high(h){
    }

    using VolumeGridPtr = std::shared_ptr<VolumeGrid>;
    // Same as the constructors, registering the primitive.
    static VolumeGridPtr Add(int N,scalar l);
    static VolumeGridPtr Add(int Nx,int Ny,int Nz,vec l,vec h);

    // Structure of polyscope, for direct access.
    polyscope::VolumeGrid* pc;

    // Total number of grid points.
    int getNbVariables() const {
        return Nx*Ny*Nz;
    }

    // Index of the grid point (i,j,k) in a flat array of values.
    int ix(int i,int j,int k) const {
        return k*Ny*Nx + j*Nx + i;
    }

    // Evaluates f at every grid point, in the order given by ix.
    Vec eval(const std::function<scalar(vec)>& f) const;

    // Trilinear interpolation at x of values f given at the grid points.
    scalar interpolate(const vec& x,const Vec& f) const;

    // Primitive interface
public:
    void initPolyscope() {
        pc = polyscope::registerVolumeGrid(getPolyscopeName(),{Nx,Ny,Nz},{low(0),low(1),low(2)},{high(0),high(1),high(2)});
        initPolyscopeData(pc, false);
    }
};

} // namespace slope

#endif // SLOPE_VOLUMEGRID_H
