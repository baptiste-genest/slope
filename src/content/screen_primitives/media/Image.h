#ifndef IMAGE_H
#define IMAGE_H

#include "content/screen_primitives/ScreenPrimitive.h"
// GL_CLAMP_TO_EDGE needs a modern header, the Windows SDK's gl.h stops at 1.1
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include "glad/glad.h"
#endif
#include "GLFW/glfw3.h"
#include "math/kernels.h"
#include "math/geometry.h"
#include "content/config/io.h"

namespace slope {

struct ImageData {
    GLuint texture = 0;
    int width      = -1;
    int height     = -1;
    size_t assetId = 0;
};

ImageData loadImage(path filename);

void DisplayImageOnPlane(const ImageData& data,const StateInSlide& sis,scalar scale,
                         const RGBA& tint,scalar y_offset);

bool PlaneScreenExtent(const StateInSlide& sis,const ImageData& data,scalar draw_scale,
                       scalar& px_w,scalar& px_h);

void DisplayImageOnPlane(const ImageData& data,const StateInSlide& sis,scalar scale,
                         const RGBA& tint,scalar y_offset);

bool PlaneScreenExtent(const StateInSlide& sis,const ImageData& data,scalar draw_scale,
                       scalar& px_w,scalar& px_h);

/// exact area average of an RGBA buffer, done on premultiplied alpha so that a
/// transparent border cannot bleed into the ink
std::vector<unsigned char> areaReduceRGBA(const unsigned char* src,int sw,int sh,int dw,int dh);

/// same as loadImage, but stores the texture at the size it will be drawn at.
/// A 2x2 bilinear tap only covers its footprint up to a 2:1 minification, so
/// anything smaller has to be filtered here rather than by the sampler
ImageData loadImage(path filename,double xscale,double yscale);
void DisplayImage(const ImageData& data,const StateInSlide& sis,scalar scale = 1,const RGBA& tint = RGBA(1.f,1.f,1.f,1.f),scalar y_offset = 0);
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle,const RGBA& color_mult);

std::vector<ImageData> loadGif(path filename);


class Image : public ScreenPrimitive {
public:
    using ImagePtr = std::shared_ptr<Image>;

    Image() {}
    ~Image();
    bool isValid() {return data.width != -1;}
    void display(const StateInSlide& sis) const;

    static ImagePtr Add(std::string filename,scalar scale = 1);

    /// an image whose pixels the code produces rather than a file, transparent
    /// until the first updateImage
    static ImagePtr Blank(int w,int h);

    /// replaces the pixels, RGBA with the first row at the top. It reallocates
    /// on a size change, so a source that switches resolution is fine, and a
    /// null pointer just clears to transparent
    void updateImage(const unsigned char* rgba,int w,int h);

    /// same from a file, which is the short way to show something that was
    /// just written to disk. A file that will not load leaves the image as is
    void updateImage(const std::string& file);


    static ImVec2 getSize(std::string filename);
    static Size getScaledSize(const ImageData& data,scalar scale);
    ImageData data;
    scalar scale = 1;

private:
    static std::vector<Image> images;
    static size_t count;
    bool owns_texture = false;


    // Primitive interface
public:
    void draw(const TimeObject&, const StateInSlide &sis) override;
    void playIntro(const TimeObject& t, const StateInSlide &sis) override;
    void playOutro(const TimeObject& t, const StateInSlide &sis) override;
    Size getSize() const override;
    bool canRotate() const override {return true;}
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

    bool canRotate() const override {return true;}

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
