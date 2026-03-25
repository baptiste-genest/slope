#ifndef PCH_H
#define PCH_H


//#define EIGEN_MPL2_ONLY
#define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT

#ifdef __APPLE__
//#include "Eigen/Dense"
#include "Eigen/Sparse"
#include "Eigen/Core"
#include "Eigen/Geometry"
#else
//#include "eigen3/Eigen/Dense"
#include "Eigen/Sparse"
#include "Eigen/Core"
#include "Eigen/Geometry"
#endif

#include "polyscope/polyscope.h"
#include "polyscope/curve_network.h"
#include "polyscope/point_cloud.h"
#include "polyscope/surface_mesh.h"
#include "polyscope/structure.h"

#include "extern/json.hpp"
#include "polyscope/color_management.h"
#include <stdarg.h>

#include <string>

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>

#include <cmath>
#include <iostream>

#include <chrono>
#include <ratio>
#include <functional>

#include <filesystem>
#include <regex>

#include <glm/gtx/matrix_interpolation.hpp>

#include "extern/json.hpp"
#include "extern/stb_image.h"
#include "extern/svg.hpp"

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>

#include "polyscope/screenshot.h"
#ifndef __APPLE__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include <spdlog/spdlog.h>
#endif // PCH_H
