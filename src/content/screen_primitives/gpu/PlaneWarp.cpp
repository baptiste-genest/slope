#include "content/screen_primitives/gpu/PlaneWarp.h"
#include "content/screen_primitives/media/Image.h"
#include <polyscope/polyscope.h>
#include <map>

namespace slope {

Frame3D FrameFromTransform(const Transform &T, scalar aspect)
{
    const mat R = T.getRotationEigen();
    Frame3D f;
    f.origin = vec(T.translation.x,T.translation.y,T.translation.z);
    f.u = R.col(0)*T.scale.x;
    f.v = R.col(1)*(T.scale.x/std::max(aspect,1e-9));
    return f;
}

Transform TransformFromFrame(const Frame3D &f)
{
    const vec x = f.u.normalized();
    const vec down = f.v.normalized();
    mat R; R.col(0) = x; R.col(1) = down; R.col(2) = x.cross(down);
    return Transform::ScalePositionRotate(vec::Constant(f.u.norm()),f.origin,R);
}

Transform TransformFromWidth(const vec &origin, const vec &u, const vec &n)
{
    Frame3D f;
    f.origin = origin;
    f.u = u;
    f.v = u.normalized().cross(n.normalized()).normalized()*u.norm();
    return TransformFromFrame(f);
}

Transform FoldInPlane(const Transform &T, scalar k, scalar angle)
{
    Transform R = T;
    R.scale *= (float)k;
    if (std::abs(angle) > 1e-9){
        mat Rz;
        Rz << std::cos(angle),-std::sin(angle),0,
              std::sin(angle), std::cos(angle),0,
              0,0,1;
        R = Transform::ScalePositionRotate(vec(R.scale.x,R.scale.y,R.scale.z),
                                           vec(R.translation.x,R.translation.y,R.translation.z),
                                           T.getRotationEigen()*Rz);
    }
    return R;
}

CameraProjector CameraProjector::Current()
{
    CameraProjector c;
    glm::mat4 M = polyscope::view::getCameraPerspectiveMatrix() * polyscope::view::viewMat;
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            c.M[col*4+row] = M[col][row];
    glm::mat4 Mi = glm::inverse(M);
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            c.Minv[col*4+row] = Mi[col][row];

    glm::vec3 e = polyscope::view::getCameraWorldPosition();
    c.eye = vec(e.x,e.y,e.z);
    const glm::mat4& V = polyscope::view::viewMat;
    c.forward = vec(-V[0][2],-V[1][2],-V[2][2]).normalized();
    return c;
}

bool CameraProjector::project(const vec &p, vec2 &screen) const
{
    scalar x=0,y=0,w=0;
    const scalar in[4] = {p(0),p(1),p(2),1};
    for (int col = 0; col < 4; col++){
        x += M[col*4+0]*in[col];
        y += M[col*4+1]*in[col];
        w += M[col*4+3]*in[col];
    }
    if (w <= 1e-6)
        return false;
    screen = vec2((x/w + 1)*0.5, 1 - (y/w + 1)*0.5);
    return true;
}

vec CameraProjector::unproject(const vec2 &screen, scalar depth) const
{
    // a near plane point fixes the ray, the depth says where to stop on it
    const scalar ndc[4] = {screen(0)*2 - 1, 1 - screen(1)*2, -1, 1};
    scalar o[4] = {0,0,0,0};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            o[row] += Minv[col*4+row]*ndc[col];
    if (std::abs(o[3]) < 1e-12)
        return eye + forward*depth;

    vec onNear(o[0]/o[3],o[1]/o[3],o[2]/o[3]);
    vec dir = (onNear - eye);
    const scalar along = dir.dot(forward);
    if (std::abs(along) < 1e-12)
        return eye + forward*depth;
    return eye + dir*(depth/along);
}

scalar DefaultPlaneDepth()
{
    const CameraProjector cam = CameraProjector::Current();
    glm::vec3 c = polyscope::state::center();
    const scalar d = cam.viewDepth(vec(c.x,c.y,c.z));
    return (d > 1e-3) ? d : polyscope::state::lengthScale;
}

static std::unordered_map<std::string,Transform> drawn_planes;

void NotePlaneDrawn(const std::string &label, const Transform &T)
{
    if (!label.empty())
        drawn_planes[label] = T;
}

std::optional<Transform> LastPlaneDrawn(const std::string &label)
{
    auto it = drawn_planes.find(label);
    if (it == drawn_planes.end())
        return std::nullopt;
    return it->second;
}

Frame3D BillboardFrame(const vec2 &pos, scalar half_w, scalar half_h, scalar angle, scalar depth)
{
    const CameraProjector cam = CameraProjector::Current();
    const scalar ca = std::cos(angle), sa = std::sin(angle);
    const vec2 du(half_w*ca,half_w*sa);
    const vec2 dv(-half_h*sa,half_h*ca);

    Frame3D f;
    f.origin = cam.unproject(pos,depth);
    f.u = cam.unproject(pos + du,depth) - f.origin;
    f.v = cam.unproject(pos + dv,depth) - f.origin;
    f.double_sided = true;
    return f;
}

static bool projectCorners(const Frame3D& f,const CameraProjector& cam,vec2 c[4])
{
    for (int i = 0; i < 4; i++){
        const scalar s = (i == 1 || i == 2) ? 1. : 0.;
        const scalar t = (i >= 2) ? 1. : 0.;
        if (!cam.project(f.origin + (2*s-1)*f.u + (2*t-1)*f.v,c[i]))
            return false;
    }
    return true;
}

bool PlaneScreenExtent(const Frame3D &f, scalar &px_w, scalar &px_h)
{
    const CameraProjector cam = CameraProjector::Current();
    vec2 c[4];
    if (!projectCorners(f,cam,c))
        return false;

    auto W = ImGui::GetWindowSize();
    auto px = [&](int a,int b){
        return std::hypot((c[a](0)-c[b](0))*W.x,(c[a](1)-c[b](1))*W.y);
    };
    // the near edge is the one that needs the texels
    px_w = std::max(px(0,1),px(3,2));
    px_h = std::max(px(0,3),px(1,2));
    return true;
}

void DrawTexturedPlane(const Frame3D &f, const ImageData &data, const RGBA &tint, scalar alpha)
{
    const CameraProjector cam = CameraProjector::Current();
    const vec n = f.v.cross(f.u);
    if (n.norm() < 1e-12)
        return;
    if (!f.double_sided && n.dot(cam.eye - f.origin) <= 0)
        return;

    auto W = ImGui::GetWindowSize();
    auto toPixels = [&](const vec2& s){return ImVec2((float)(s(0)*W.x),(float)(s(1)*W.y));};

    vec2 c[4];
    if (!projectCorners(f,cam,c))
        return;
    scalar maxEdge = 0;
    for (int i = 0; i < 4; i++){
        ImVec2 p = toPixels(c[i]), q = toPixels(c[(i+1)%4]);
        maxEdge = std::max<scalar>(maxEdge,std::hypot(p.x-q.x,p.y-q.y));
    }
    const int N = std::clamp((int)std::ceil(maxEdge/24.),1,32);

    const ImU32 col = ImGui::ColorConvertFloat4ToU32(
        ImVec4(tint.Value.x,tint.Value.y,tint.Value.z,tint.Value.w*alpha));

    const int nv = (N+1)*(N+1);
    const int ni = N*N*6;

    // projected up front, a late failure must not leave the buffers half written
    std::vector<ImVec2> grid;
    grid.reserve(nv);
    for (int j = 0; j <= N; j++){
        const scalar t = scalar(j)/N;
        for (int i = 0; i <= N; i++){
            const scalar s = scalar(i)/N;
            vec2 sp;
            if (!cam.project(f.origin + (2*s-1)*f.u + (2*t-1)*f.v,sp))
                return;
            grid.push_back(toPixels(sp));
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushTexture((ImTextureID)(intptr_t)data.texture);
    dl->PrimReserve(ni,nv);
    const unsigned int base = dl->_VtxCurrentIdx;

    for (int j = 0; j <= N; j++)
        for (int i = 0; i <= N; i++)
            dl->PrimWriteVtx(grid[j*(N+1)+i],ImVec2((float)i/N,(float)j/N),col);
    for (int j = 0; j < N; j++){
        for (int i = 0; i < N; i++){
            const unsigned int a0 = base + j*(N+1) + i;
            const unsigned int a1 = a0 + 1;
            const unsigned int a2 = a0 + (N+1);
            const unsigned int a3 = a2 + 1;
            dl->PrimWriteIdx((ImDrawIdx)a0); dl->PrimWriteIdx((ImDrawIdx)a1); dl->PrimWriteIdx((ImDrawIdx)a3);
            dl->PrimWriteIdx((ImDrawIdx)a0); dl->PrimWriteIdx((ImDrawIdx)a3); dl->PrimWriteIdx((ImDrawIdx)a2);
        }
    }
    dl->PopTexture();
}

// an unknown or failing snippet reads as zero, so hold the last good frame
std::optional<Transform> EvalLivePlane(const LivePlane &l)
{
    static std::map<std::string,Transform> last_good;
    const vec o = l.origin.value(), u = l.u.value(), n = l.normal.value();
    const std::string k = l.origin.key()+"|"+l.u.key()+"|"+l.normal.key();

    if (u.norm() > 1e-9 && n.norm() > 1e-9
        && u.normalized().cross(n.normalized()).norm() > 1e-6){
        Transform T = TransformFromWidth(o,u,n);
        last_good[k] = T;
        return T;
    }
    auto it = last_good.find(k);
    if (it != last_good.end())
        return it->second;
    return std::nullopt;
}

}
