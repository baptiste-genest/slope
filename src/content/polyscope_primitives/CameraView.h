#ifndef CAMERAVIEW_H
#define CAMERAVIEW_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "content/config/Options.h"
#include <fstream>
#include "extern/json.hpp"

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
        // a flight animates over several frames, an export would catch it
        // mid-flight, so jump to the view instead
        bool fly = flyTo && !Options::ExportMode;
        if (fromFile) {
            // polyscope 2.6.1's setViewFromJson never restores "fov" (its
            // key check is inverted), so apply the saved fov beforehand, both
            // the direct path and the flyTo target then use it
            if (saved_fov > 0)
                polyscope::view::fov = saved_fov;
            polyscope::view::setCameraFromJson(jsonContent,fly);
        }
        else
            polyscope::view::lookAt(from,to,up,fly);
    }
    void disable() {
        polyscope::view::resetCameraToHomeView();
    }

private:
    bool fromFile = false;
    glm::vec3 from,to,up;
    std::string jsonContent;
    bool flyTo;
    float saved_fov = -1;

public:
    void setSavedFov(float f) {saved_fov = f;}
};



}

#endif // CAMERAVIEW_H
