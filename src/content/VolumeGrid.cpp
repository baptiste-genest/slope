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


scalar VolumeGrid::interpolate(const vec& x, const Vec& f) const
{
    // trilinear interpolation
    vec s = (x-low).cwiseQuotient(high-low);
    int i = std::min((int)(s(0)*(Nx-1)),Nx-2);
    int j = std::min((int)(s(1)*(Ny-1)),Ny-2);
    int k = std::min((int)(s(2)*(Nz-1)),Nz-2);

    scalar xd = s(0)*(Nx-1) - i;
    scalar yd = s(1)*(Ny-1) - j;
    scalar zd = s(2)*(Nz-1) - k;

    scalar c00 = f(ix(i,j,k))*(1-xd) + f(ix(i+1,j,k))*xd;
    scalar c01 = f(ix(i,j,k+1))*(1-xd) + f(ix(i+1,j,k+1))*xd;
    scalar c10 = f(ix(i,j+1,k))*(1-xd) + f(ix(i+1,j+1,k))*xd;
    scalar c11 = f(ix(i,j+1,k+1))*(1-xd) + f(ix(i+1,j+1,k+1))*xd;

    scalar c0 = c00*(1-yd) + c10*yd;
    scalar c1 = c01*(1-yd) + c11*yd;
    return c0*(1-zd) + c1*zd;
}

} // namespace slope
