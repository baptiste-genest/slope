#ifndef PCH_H
#define PCH_H

// heavy headers (json, svg, stb, regex, X11) live in the few TUs that use them


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

#include <glm/gtx/matrix_interpolation.hpp>


#include <iostream>
#include <fstream>
#include <iostream>
#include <signal.h>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include "polyscope/screenshot.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include <spdlog/spdlog.h>
#endif // PCH_H
