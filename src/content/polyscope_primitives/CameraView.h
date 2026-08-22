#ifndef CAMERAVIEW_H
#define CAMERAVIEW_H

#include "content/polyscope_primitives/PolyscopePrimitive.h"
#include "content/config/Options.h"
#include <fstream>
#include <optional>
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
    CameraView(const glm::vec3 &from, const glm::vec3 &to, const glm::vec3 &up,
               std::optional<bool> fly = std::nullopt) : from(from),to(to),up(up),flyTo(fly) {
        fromFile = false;
    }

    CameraView(std::string json,std::optional<bool> fly) : jsonContent(json),flyTo(fly) {
        fromFile = true;
    }

    CameraView() {}
    // an unset flyTo leaves the choice to the show, which cuts to the first
    // camera and glides to one replacing another
    static CameraViewPtr Add(const vec& from,const vec& to,const vec& up = vec(0,1,0),
                             std::optional<bool> flyTo = std::nullopt);
    static CameraViewPtr Add(std::string json_file,std::optional<bool> flyTo = std::nullopt);

public:
    // allow_fly is false where the view must be reached at once, a jump
    // through the slide menu or a skipped frame. default_fly is what to do
    // when the camera itself did not say
    void enable(bool allow_fly = true, bool default_fly = false) {
        // a flight animates over several frames, an export would catch it
        // mid-flight, so jump to the view instead
        bool fly = allow_fly && flyTo.value_or(default_fly) && !Options::ExportMode;
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
    std::optional<bool> flyTo;
    float saved_fov = -1;

public:
    void setSavedFov(float f) {saved_fov = f;}
};



}

#endif // CAMERAVIEW_H
