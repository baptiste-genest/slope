#include "slides/deck/items/DeckItem.h"
#include "content/screen_primitives/media/Image.h"
#include "content/screen_primitives/media/Video.h"
#include "content/screen_primitives/media/Webcam.h"
#include <filesystem>

namespace slope {

// a media item is referred to by its filename, the deck rarely needs an id
static std::string fileStem(const json& item, const char* type)
{
    return std::filesystem::path(item[type].get<std::string>()).stem().string();
}

std::vector<ItemSpec> mediaItemSpecs()
{
    std::vector<ItemSpec> specs;

    specs.push_back({
        "image", ItemSpec::Kind::Screen, {"scale"},
        [](const json& i) {
            return "image:" + i["image"].get<std::string>() + ":"
                 + std::to_string(i.value("scale", 1.));
        },
        [](const json& i) -> PrimitivePtr {
            return Image::Add(i["image"].get<std::string>(), i.value("scale", 1.));
        },
        nullptr,
        [](const json& i) { return fileStem(i, "image"); },
    });

    // every frame is held as a texture, so the fps and the loop flag shape it
    specs.push_back({
        "gif", ItemSpec::Kind::Screen, {"scale","fps","loop"},
        [](const json& i) {
            return "gif:" + i["gif"].get<std::string>() + ":"
                 + std::to_string(i.value("fps", 10)) + ":"
                 + std::to_string(i.value("scale", 1.))
                 + (i.value("loop", true) ? ":loop" : "");
        },
        [](const json& i) -> PrimitivePtr {
            return Gif::Add(i["gif"].get<std::string>(), i.value("fps", 10),
                            i.value("scale", 1.), i.value("loop", true));
        },
        nullptr,
        [](const json& i) { return fileStem(i, "gif"); },
    });

    specs.push_back({
        "video", ItemSpec::Kind::Screen,
        {"scale","decode_width","loop","autoplay","speed","stats"},
        // those three shape the decoder and belong in the key, the fields
        // below are re-applied to the cached primitive on every build
        [](const json& i) {
            return "video:" + i["video"].get<std::string>() + ":"
                 + std::to_string(i.value("decode_width", 0))
                 + (i.value("loop", true) ? ":loop" : "")
                 + (i.value("autoplay", true) ? ":auto" : "");
        },
        [](const json& i) -> PrimitivePtr {
            return Video::Add(i["video"].get<std::string>(), i.value("decode_width", 0),
                              i.value("loop", true), i.value("autoplay", true));
        },
        [](const PrimitivePtr& p, const json& i, const std::string&) {
            auto vid = std::static_pointer_cast<Video>(p);
            scalar sp = i.value("speed", 1.);
            vid->speed = [sp] { return sp; };
            vid->show_stats = i.value("stats", false);
            vid->scale = i.value("scale", 1.);
        },
        [](const json& i) { return fileStem(i, "video"); },
    });

    specs.push_back({
        "webcam", ItemSpec::Kind::Screen,
        {"scale","width","height","fps","input_format","stats"},
        // the device is in the key, a camera only opens once
        [](const json& i) {
            return "webcam:" + i["webcam"].get<std::string>() + ":"
                 + std::to_string(i.value("width", 1280)) + "x"
                 + std::to_string(i.value("height", 720)) + "@"
                 + std::to_string(i.value("fps", 30)) + ":"
                 + i.value("input_format", std::string("mjpeg"));
        },
        [](const json& i) -> PrimitivePtr {
            return Webcam::Add(i["webcam"].get<std::string>(), i.value("width", 1280),
                               i.value("height", 720), i.value("fps", 30),
                               i.value("input_format", std::string("mjpeg")));
        },
        [](const PrimitivePtr& p, const json& i, const std::string&) {
            auto cam = std::static_pointer_cast<Webcam>(p);
            cam->show_stats = i.value("stats", false);
            cam->scale = i.value("scale", 1.);
        },
        [](const json& i) { return fileStem(i, "webcam"); },
    });

    return specs;
}

}
