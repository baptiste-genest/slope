#include "VolumeGrid.h"

namespace slope {

VolumeGrid::VolumeGridPtr VolumeGrid::Add(int N, scalar l)
{
    return NewPrimitive<VolumeGrid>(N,l);
}

VolumeGrid::VolumeGridPtr VolumeGrid::Add(int Nx, int Ny, int Nz, vec l, vec h)
{
    return NewPrimitive<VolumeGrid>(Nx,Ny,Nz,l,h);
}

} // namespace slope
