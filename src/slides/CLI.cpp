#include "CLI.h"

// #include "../extern/CLI11.hpp"
#include "../content/Options.h"
#include "../content/io.h"
#include "spdlog/spdlog.h"
#include <cassert>


int slope::parseCLI(int argc,char** argv) {
    using namespace slope;

    bool clear_cache = false;
    Options::ignore_cache = false;

    std::string resolution = "1920x1080";
    std::string data_path;
    int seed = -1;

    // Simple manual parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--clear_cache") {
            clear_cache = true;
        }
        else if (arg == "--ignore_cache") {
            Options::ignore_cache = true;
        }
        else if (arg == "--resolution" && i + 1 < argc) {
            resolution = argv[++i];
        }
        else if (arg == "--project_path" && i + 1 < argc) {
            Options::ProjectPath = argv[++i];
        }
        else if (arg == "--data_path" && i + 1 < argc) {
            data_path = argv[++i];
        }
        else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        }
        else if (arg == "--export") {
            Options::ExportMode = true;
        }
        else if (arg == "--check_labels") {
            Options::CheckLabels = true;
        }
        else if (arg == "--rehearse") {
            Options::Rehearse = true;
        }
        else if (arg == "--no_slide_numbers") {
            Options::HideSlideNumbers = true;
        }
        else {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            return 1;
        }
    }

    // Required argument check
    if (Options::ProjectPath.empty()) {
        std::cerr << "--project_path is required\n";
        return 1;
    }
    Options::ProjectPath = normalizedDir(Options::ProjectPath);

    slope::Options::ProjectViewsPath = normalizedDir(slope::path(slope::Options::ProjectPath) / "views");

    slope::Options::CachePath = normalizedDir(std::filesystem::path(argv[0]).parent_path() / "slope_cache");

    // ProjectPath and ProjectViewsPath used to be created lazily, only as a
    // side effect of specific features writing into them for the first time
    // (LabelAnchor::writeAtLabel, TimeTracker::save, Params::save) ; a deck
    // that never happened to touch any of those never got a views/ folder at
    // all. Create both upfront instead, the same way CachePath already is.
    // ProjectDataPath is deliberately left alone : it holds input assets the
    // deck reads from, and silently creating it would mask a typo'd path.
    std::filesystem::create_directories(slope::Options::ProjectPath);
    std::filesystem::create_directories(slope::Options::ProjectViewsPath);
    std::filesystem::create_directories(slope::Options::CachePath);

    // LogPath defaults to "/tmp/slope.log", which does not exist on Windows :
    // every latex/convert command redirects into it (">> <log> 2>&1"), so an
    // unwritable path there fails the command before the tool even runs.
    // temp_directory_path() is /tmp on Linux and macOS, so this keeps the
    // existing location on those platforms and picks %TEMP% on Windows.
    {
        std::error_code ec;
        path tmp = std::filesystem::temp_directory_path(ec);
        if (!ec)
            slope::Options::LogPath = (tmp / "slope.log").string();
    }
    std::ofstream(slope::Options::LogPath,std::ios::trunc);

    if (data_path.empty())
        slope::Options::ProjectDataPath = slope::Options::ProjectPath;
    else
        slope::Options::ProjectDataPath = data_path;

    if (seed == -1)
        srand(time(NULL));
    else{
        srand(seed);
    }

    if (clear_cache){
        spdlog::info("clearing cache");
        // std::filesystem rather than "rm -rf", which does not exist on Windows
        std::error_code ec;
        for (const char* dir : {"cache", "formulas"}) {
            std::filesystem::remove_all(Options::ProjectDataPath + dir, ec);
            std::filesystem::create_directories(Options::ProjectDataPath + dir, ec);
        }
    }

    auto pos = resolution.find('x');
    if (pos == std::string::npos){
        std::cerr << "invalid resolution format" << std::endl;
        assert(false);
    }
    Options::ScreenResolutionWidth = std::stoi(resolution.substr(0,pos));
    Options::ScreenResolutionHeight = std::stoi(resolution.substr(pos+1));

    return 0; //ok

}
