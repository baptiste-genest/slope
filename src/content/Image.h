#ifndef IMAGE_H
#define IMAGE_H

#include "ScreenPrimitive.h"
#include "GLFW/glfw3.h"
#include "../math/kernels.h"
#include "../math/geometry.h"
#include "io.h"

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

    Gif(const std::vector<ImageData>& images,int fps,scalar scale,bool loop);
    bool isValid();

    void display(const StateInSlide& sis) const;

    static GifPtr Add(std::string filename,int fps = 10,scalar scale = 1.,bool loop = true);

    void draw(const TimeObject& t, const StateInSlide &sis) override;

    void playIntro(const TimeObject& t, const StateInSlide &sis) override;

    void playOutro(const TimeObject& t, const StateInSlide &sis) override;

    Size getSize() const override;

    int current_img = 0;


private:
    void upframe(const TimeObject& t) {
        if (loop)
            current_img = (int)std::floor(t.inner_time*fps) % int(images.size());
        else
            current_img = std::min((int)std::floor(t.inner_time*fps),int(images.size())-1);
    }
    bool loop;
    int fps = 24;
    std::vector<ImageData> images;
    scalar scale = 1;
};


}

#endif // IMAGE_H
