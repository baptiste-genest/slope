#ifndef LIBSLOPE_H
#define LIBSLOPE_H

#include "common.hpp"
#include "extern/json_fwd.hpp"

// Mixing two Eigen versions or alignments in one binary corrupts fixed-size vectors without any error.
// Each file needs a symbol named after the Eigen it sees, and libslope defines only its own.
// A mismatch then fails to link and the error names the Eigen of this file.
#define SLOPE_EIGEN_ABI_NAME_(w, M, m, a) slope_built_with_another_eigen__this_file_sees_##w##_##M##_##m##_align##a
#define SLOPE_EIGEN_ABI_NAME(w, M, m, a) SLOPE_EIGEN_ABI_NAME_(w, M, m, a)
#define SLOPE_EIGEN_ABI_SYMBOL SLOPE_EIGEN_ABI_NAME(EIGEN_WORLD_VERSION, EIGEN_MAJOR_VERSION, \
                                                    EIGEN_MINOR_VERSION, EIGEN_MAX_STATIC_ALIGN_BYTES)
extern "C" const int SLOPE_EIGEN_ABI_SYMBOL;
[[gnu::used]] static const int* const slope_eigen_abi_check = &SLOPE_EIGEN_ABI_SYMBOL;

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

// Seconds from A to B.
inline TimeTypeSec TimeBetween(const TimeStamp& A,const TimeStamp& B){
    return DurationSec(B-A).count();
}

// Seconds from A to now.
inline TimeTypeSec TimeFrom(const TimeStamp& A){
    return DurationSec(Time::now()-A).count();
}


// A value in [0,1] that measures progress.
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

// A position with the index of the vertex it belongs to.
struct Vertex {
    vec pos;
    int id;
};

using Face = std::vector<size_t>;
using Faces = std::vector<Face>;

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

// Converts an Eigen 2D vector to an ImVec2.
inline ImVec2 toVec2(const vec2& x) {
    return ImVec2(x(0),x(1));
}

}

#endif // LIBSLOPE_H
