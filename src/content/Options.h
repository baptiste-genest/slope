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
static   std::string LogPath;

static bool ExportMode;

/// Report unused/duplicated/defaulted anchor labels at startup (--check_labels)
static bool CheckLabels;

/// Enable the rehearsal timer : record timings, offer to save them on quit,
/// and compare against the previous run (--rehearse)
static bool Rehearse;

/// Latex paths
static std::string PathToPDFLATEX;
static std::string PathToCONVERT;

/// Where a shader's "#include <...>" finds the shader stdlib
static std::string ShaderPath;

///Window size
static size_t ScreenResolutionWidth;
static size_t ScreenResolutionHeight;

///Density for the PDF -> PNG export
static size_t PDFtoPNGDensity;

static float DefaultBoxRoundness;

static bool ignore_cache;

/// Height ratio for title
static double TitleScale;
static double DefaultLatexScale;

};
}
#endif //OPTIONS_H
