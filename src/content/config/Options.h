#ifndef OPTIONS_H
#define OPTIONS_H

#define STRING(x) #x
#define TOSTRING(x) STRING(x)
#include <string>

#include "libslope.h"

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

/// Samples per slide change for --export_transitions, 0 for one still per slide
static int ExportTransitionSamples;

/// Report unused/duplicated/defaulted anchor labels at startup (--check_labels)
static bool CheckLabels;

/// Rehearsal timer, records timings and compares against the previous run
static bool Rehearse;

/// Hide the slide number in the bottom right corner (--no_slide_numbers)
static bool HideSlideNumbers;

/// Latex paths
static std::string PathToPDFLATEX;
static std::string PathToCONVERT;

/// Video decoding, optional, a bare name still resolves through PATH
static std::string PathToFFMPEG;
static std::string PathToFFPROBE;

/// TTF a Code listing is drawn with, empty for polyscope's monospace font
static std::string CodeFont;

/// Where a shader's "#include <...>" finds the shader stdlib
static std::string ShaderPath;
// one <language>.scm per declared language
static std::string QueryPath;

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

/// (x, y) gap the TOP_LEFT / ... edge anchors leave to the window border
static vec2 ScreenMargin;

// compiled defaults, the value a dropped "config:" key falls back to
static constexpr double DefaultTitleScale       = 1.5;
static constexpr double DefaultLatexScaleValue  = 1.0;
static constexpr float  DefaultBoxRoundnessValue = 1.f;
static constexpr double DefaultScreenMargin     = 0.06;

};
}
#endif //OPTIONS_H
