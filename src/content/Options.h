#ifndef OPTIONS_H
#define OPTIONS_H

#define STRING(x) #x
#define TOSTRING(x) STRING(x)
#include <string>

#include "../libslope.h"

namespace slope {
struct Options{

/// Global slope build prefix
//static   std::string SlopePath;
static   std::string ProjectDataPath;
static   std::string ProjectName;
static   std::string ProjectPath;
static   std::string ProjectViewsPath;
static   std::string CachePath;

static bool ExportMode;

/// Latex paths
static std::string PathToPDFLATEX;
static std::string PathToCONVERT;

///Window size
static size_t ScreenResolutionWidth;
static size_t ScreenResolutionHeight;

///Density for the PDF -> PNG export
static size_t PDFtoPNGDensity;

static Eigen::Vector3d DefaultBackgroundColor;

static bool ignore_cache;

/// Height ratio for title
static double TitleScale;
static double DefaultLatexScale;

};
}
#endif //OPTIONS_H
