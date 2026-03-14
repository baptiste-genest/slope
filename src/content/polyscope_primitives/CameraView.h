#ifndef CAMERAVIEW_H
#define CAMERAVIEW_H

#include "PolyscopePrimitive.h"
#include <fstream>

namespace slope {


std::string formatCameraFilename(std::string file);
std::string removeResolutionFromCamfile(std::string json);

class CameraView;
using CameraViewPtr = std::shared_ptr<CameraView>;
class CameraView
{
public:
    static glm::vec3 toVec3(const vec& x) {
        return glm::vec3(x(0),x(1),x(2));
    }
    CameraView(const glm::vec3 &from, const glm::vec3 &to, const glm::vec3 &up,bool fly = false) : from(from),to(to),up(up),flyTo(fly) {
        fromFile = false;
    }

    CameraView(std::string json,bool fly) : jsonContent(json),flyTo(fly) {
        fromFile = true;
    }

    CameraView() {}
    static CameraViewPtr Add(const vec& from,const vec& to,const vec& up = vec(0,1,0),bool flyTo = false);
    static CameraViewPtr Add(std::string json_file,bool flyTo = false);

public:
    void enable() {
        if (fromFile)
            polyscope::view::setCameraFromJson(jsonContent,flyTo);
        else
            polyscope::view::lookAt(from,to,up,flyTo);
    }
    void disable() {
        polyscope::view::resetCameraToHomeView();
    }

private:
    bool fromFile = false;
    glm::vec3 from,to,up;
    std::string jsonContent;
    bool flyTo;
};



}

#endif // CAMERAVIEW_H
