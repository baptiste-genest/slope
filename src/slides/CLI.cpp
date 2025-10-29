#include "CLI.h"

#include "../extern/CLI11.hpp"
#include "../content/Options.h"
#include "spdlog/spdlog.h"
#include <cassert>


int slope::parseCLI(int argc,char** argv) {
    CLI::App app("slope - 3D slides generator");
    bool clear_cache = false;


    using namespace slope;

    argv = app.ensure_utf8(argv);
    app.add_flag("--clear_cache",clear_cache,"cache will be cleared and every resources of all projects will have to be regenerated");
    app.add_flag("--ignore_cache",Options::ignore_cache,"cache will be ignored and every requested resource will be regenerated");

    std::string resolution = "1920x1080";
    app.add_option("--resolution",resolution,"screen resolution (default 1920x1080)");

    app.add_option("--project_path",Options::ProjectPath,"path to project")->required();

    std::string data_path;
    app.add_option("--data_path",data_path,"path to data (default same as project)");

    int seed = -1;
    app.add_option("--seed",seed,"seed for random generator");

//    app.add_flag("--export_pdf",Options::ExportMode,"if set then headless rendering add exports in pdf file");

    try {
        app.parse(argc,argv);
    } catch (const CLI::ParseError &e) {
        app.exit(e);
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
