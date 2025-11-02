#ifndef LIBSLOPE_H
#define LIBSLOPE_H

#include "PCH.h"

namespace slope {

class Widget;
class StateInSlide;

using WidgetPtr = std::shared_ptr<Widget>;

using Vec2 = ImVec2;
using RGBA = ImColor;


using Time = std::chrono::high_resolution_clock;
using TimeStamp = std::chrono::time_point<std::chrono::high_resolution_clock>;
using TimeTypeSec = float;
using DurationSec = std::chrono::duration<TimeTypeSec>;

inline TimeTypeSec TimeBetween(const TimeStamp& A,const TimeStamp& B){
    return DurationSec(B-A).count();
}

inline TimeTypeSec TimeFrom(const TimeStamp& A){
    return DurationSec(Time::now()-A).count();
}


using parameter = double;


using Animation = std::function<void(TimeTypeSec,const StateInSlide&)>;

using index = size_t;

using scalar = double;
using scalars = std::vector<scalar>;
constexpr scalar TAU = 2*M_PI;
using vec = Eigen::Vector<scalar,3>;
using vec2 = Eigen::Vector<scalar,2>;
using Vec = Eigen::Vector<scalar,-1>;
using Mat = Eigen::Matrix<scalar,-1,-1>;
using SMat = Eigen::SparseMatrix<scalar>;
using mat2 = Eigen::Matrix<scalar,2,2>;
using mat = Eigen::Matrix<scalar,3,3>;
using vecs = std::vector<vec>;

using colors = vecs;

struct Vertex {
    vec pos;
    int id;
};

using Face = std::vector<size_t>;
using Faces = std::vector<Face>;

using FontID = int;
using FontSize = int;
struct Font {
    FontID id = -1;
    FontSize size = 16;
};

struct Primitive;
using PrimitivePtr = std::shared_ptr<Primitive>;
using PrimitiveID = long;
using PrimitiveSet = std::set<PrimitivePtr>;
using Primitives = std::vector<PrimitivePtr>;


using mapping = std::function<vec(const vec&)>;
using scalar_function = std::function<scalar(scalar)>;
struct TimeObject;
using time_mapping = std::function<vec(const vec&,const TimeObject&)>;

using curve_param = std::function<vec(scalar)>;
using dynamic_curve_param = std::function<vec(scalar,const TimeObject&)>;

using path = std::filesystem::path;

using json = nlohmann::json;

inline ImVec2 toVec2(const vec2& x) {
    return ImVec2(x(0),x(1));
}

}

#endif // LIBSLOPE_H
