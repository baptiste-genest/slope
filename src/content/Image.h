#ifndef IMAGE_H
#define IMAGE_H

#include "ScreenPrimitive.h"
#include "GLFW/glfw3.h"
#include "../math/kernels.h"
#include "../math/geometry.h"

namespace slope {

struct ImageData {
    GLuint texture = 0;
    int width      = -1;
    int height     = -1;
    size_t assetId = 0;
};

ImageData loadImage(path filename);
void DisplayImage(const ImageData& data,const StateInSlide& sis,scalar scale = 1);
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle,const RGBA& color_mult);

std::vector<ImageData> loadGif(path filename);

inline std::string formatPath(std::string path) {
    if (path[0] == '/') return path;
    return Options::ProjectPath + path;
}

class Image : public ScreenPrimitive {
public:
    using ImagePtr = std::shared_ptr<Image>;

    Image() {}
    bool isValid() {return data.width != -1;}
    void display(const StateInSlide& sis) const;

    static ImagePtr Add(std::string filename,scalar scale = 1);


    static ImVec2 getSize(std::string filename);
    static Size getScaledSize(const ImageData& data,scalar scale);
    ImageData data;
    scalar scale = 1;

private:
    static std::vector<Image> images;
    static size_t count;


    // Primitive interface
public:
    void draw(const TimeObject&, const StateInSlide &sis) override;
    void playIntro(const TimeObject& t, const StateInSlide &sis) override;
    void playOutro(const TimeObject& t, const StateInSlide &sis) override;
    Size getSize() const override;
};


class Gif : public ScreenPrimitive {
public:
    using GifPtr = std::shared_ptr<Gif>;

    Gif(const std::vector<ImageData>& images,int fps,scalar scale,bool loop) : images(images),fps(fps),scale(scale),loop(loop) {}
    bool isValid() {return images[0].width != -1;}

    void display(const StateInSlide& sis) const {
        anchor->updatePos(sis.getPosition());
        DisplayImage(images[current_img],sis,scale*sis.getScale());
    }

    static GifPtr Add(std::string filename,int fps = 10,scalar scale = 1.,bool loop = true);

    void draw(const TimeObject& t, const StateInSlide &sis) override;

    void playIntro(const TimeObject& t, const StateInSlide &sis) override {
        auto sist = sis;
        sist.alpha = smoothstep(t.transitionParameter)*sis.alpha;
        display(sist);
    }

    void playOutro(const TimeObject& t, const StateInSlide &sis) override
    {
        auto sist = sis;
        sist.alpha = smoothstep(1-t.transitionParameter)*sis.alpha;
        display(sist);
    }

    Size getSize() const override {
        return Image::getScaledSize(images[current_img],scale);
    }

    void restart() { current_img = 0; }

private:
    bool loop;
    int current_img = 0,fps = 24;
    std::vector<ImageData> images;
    scalar scale = 1;
};


}

#endif // IMAGE_H
