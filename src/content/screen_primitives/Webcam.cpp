#include "Webcam.h"

#include "../Options.h"

#include <limits>

namespace slope {

WebcamPtr Webcam::Add(const std::string& device, int w, int h, int fps,
                      const std::string& input_format)
{
    VideoInfo info;
    info.width   = w;
    info.height  = h;
    info.fps     = fps;
    info.fps_str = std::to_string(fps) + "/1";
    return NewPrimitive<Webcam>(device, info, input_format);
}

// loop true, so a device that stops feeding is opened again
Webcam::Webcam(const std::string& device, const VideoInfo& info,
               const std::string& input_format)
    : Video(device, info, info.width, info.height, true, true),
      input_format_(input_format)
{}

std::vector<std::string> Webcam::inputArgs(int64_t) const
{
    std::vector<std::string> args{Options::PathToFFMPEG, "-nostdin", "-loglevel", "error",
                                  "-f", "v4l2"};
    if (!input_format_.empty()) {
        args.push_back("-input_format");
        args.push_back(input_format_);
    }
    args.insert(args.end(), {"-framerate",  std::to_string(int(info_.fps)),
                             "-video_size", std::to_string(w_) + "x" + std::to_string(h_),
                             "-i", path_, "-an", "-sn", "-dn"});
    return args;
}

int64_t Webcam::wantedFrame(const TimeObject&)
{
    return std::numeric_limits<int64_t>::max();
}

}
