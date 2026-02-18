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

    template<class func>
    Vec eval(func f) const {
        Vec rslt(getNbVariables());
        int idx = 0;
        for(int i=0;i<Nx;++i) {
            for(int j=0;j<Ny;++j) {
                for(int k=0;k<Nz;++k) {
                    vec pos = low + vec(i/(scalar)(Nx-1),j/(scalar)(Ny-1),k/(scalar)(Nz-1))*(high-low);
                    rslt(idx++) = f(pos);
                }
            }
        }
        return rslt;
    }

    // Primitive interface
public:
    void initPolyscope() {
        pc = polyscope::registerVolumeGrid(getPolyscopeName(),{Nx,Ny,Nz},{low(0),low(1),low(2)},{high(0),high(1),high(2)});
        initPolyscopeData(pc);
    }
};

} // namespace slope

#endif // SLOPE_VOLUMEGRID_H
