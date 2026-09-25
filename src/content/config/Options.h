#ifndef OPTIONS_H
#define OPTIONS_H

#define STRING(x) #x
#define TOSTRING(x) STRING(x)
#include <string>

#include "libslope.h"

namespace slope {
struct Options {

    /// Folder of the project data, where relative paths are resolved
    static std::string ProjectDataPath;
    static std::string ProjectName;
    static std::string ProjectPath;
    /// Folder of the .pos view files
    static std::string ProjectViewsPath;
    static std::string CachePath;
    static std::string LogPath;

    /// Render the slides to images and quit
    static bool ExportMode;

    /// Samples per slide change for --export_transitions, 0 for one still per slide
    static int ExportTransitionSamples;

    /// Render every slide to a continuous frame sequence, encode <ProjectName>.mp4 (--record)
    static bool RecordMode;
    /// Frames per second for --record
    static int RecordFPS;
    /// Seconds each settled slide is held, clocks still running, for --record
    static double RecordDwell;

    /// Report unused, duplicated or defaulted anchor labels at startup (--check_labels)
    static bool CheckLabels;

    /// Rehearsal timer that records timings and compares them with the previous run
    static bool Rehearse;

    /// Hide the slide number in the bottom right corner (--no_slide_numbers)
    static bool HideSlideNumbers;

    /// Latex paths
    static std::string PathToPDFLATEX;
    static std::string PathToCONVERT;

    /// Video decoding, optional. A bare name is searched in PATH
    static std::string PathToFFMPEG;
    static std::string PathToFFPROBE;

    /// TTF font used to draw code blocks, empty for the monospace font of polyscope
    static std::string CodeFont;

    /// Folder searched by the "#include <...>" of a shader, holding the shader standard library
    static std::string ShaderPath;
    /// Folder with one <language>.scm file per declared language
    static std::string QueryPath;

    ///Window size in pixels
    static size_t ScreenResolutionWidth;
    static size_t ScreenResolutionHeight;

    ///Density for the PDF to PNG conversion
    static size_t PDFtoPNGDensity;

    /// Corner roundness of boxes
    static float DefaultBoxRoundness;

    /// Rebuild cached files even when they exist
    static bool ignore_cache;

    /// Copy a label to the clipboard for every newly placed item (--auto-suggest)
    static bool AutoSuggest;

    /// Height of the title relative to the slide
    static double TitleScale;
    /// Scale of LaTeX items when none is given
    static double DefaultLatexScale;

    /// (x, y) gap left between the edge anchors (TOP_LEFT and others) and the window border
    static vec2 ScreenMargin;

    // Compiled defaults, used when a "config:" key is removed
    static constexpr double DefaultTitleScale = 1.5;
    static constexpr double DefaultLatexScaleValue = 1.0;
    static constexpr float DefaultBoxRoundnessValue = 1.f;
    static constexpr double DefaultScreenMargin = 0.06;
};
} // namespace slope
#endif //OPTIONS_H
