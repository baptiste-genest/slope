#include "CLI.h"

// #include "../extern/CLI11.hpp"
#include "../content/Options.h"
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
    Options::ProjectPath += std::string("/");

    slope::Options::ProjectViewsPath = slope::Options::ProjectPath+std::string("views/");

    slope::Options::CachePath = std::filesystem::path(argv[0]).parent_path().string()+std::string("/slope_cache/");
    if (!std::filesystem::exists(slope::Options::CachePath))
        system(("mkdir " + slope::Options::CachePath).data());

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
        system(("rm -rf " + Options::ProjectDataPath + "cache/*").data());
        system(("rm -rf " + Options::ProjectDataPath + "formulas/*").data());
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
