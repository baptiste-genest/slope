#include "CameraView.h"
#include "../../extern/json.hpp"

slope::CameraViewPtr slope::CameraView::Add(const vec& f,const vec& t, const vec &up,bool flyTo)
{
    return std::make_shared<CameraView>(toVec3(f),toVec3(t),toVec3(up),flyTo);
}

slope::CameraViewPtr slope::CameraView::Add(std::string file, bool flyTo)
{
    file = formatCameraFilename(file);
    std::ifstream camfile(file);
    if (!camfile.is_open()){
        std::cerr << "invalid path " << file << std::endl;
        throw ;
    }
    std::string str((std::istreambuf_iterator<char>(camfile)),
                    std::istreambuf_iterator<char>());
    str = removeResolutionFromCamfile(str);
  
    return std::make_shared<CameraView>(str,flyTo);
}

std::string slope::formatCameraFilename(std::string file)
{
    if (file[0] != '/'){
        file = Options::ProjectViewsPath + file + ".json";
    }
    return file;
}

std::string slope::removeResolutionFromCamfile(std::string str)
{
    using json = nlohmann::json;
    json j = json::parse(str);
    {
        auto it = j.find("windowHeight");
        if (it != j.end())
            j.erase(it);
    }
    {
        auto it = j.find("windowWidth");
        if (it != j.end())
            j.erase(it);
    }
    return j.dump();
}
