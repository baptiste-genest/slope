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

// An image stored as an OpenGL texture. The width and height are -1 when the image is not loaded.
struct ImageData {
    GLuint texture = 0;
    int width      = -1;
    int height     = -1;
    size_t assetId = 0;
};

// Loads an image file into a texture.
ImageData loadImage(path filename);

// Draws an image pasted on the plane of the slide state.
void DisplayImageOnPlane(const ImageData& data,const StateInSlide& sis,scalar scale,
                         const RGBA& tint,scalar y_offset);

// Gives the size in pixels of the image once pasted on its plane. Returns false when the plane is not visible.
bool PlaneScreenExtent(const StateInSlide& sis,const ImageData& data,scalar draw_scale,
                       scalar& px_w,scalar& px_h);

/// Exact area average of an RGBA buffer from size sw by sh to dw by dh.
/// It works on premultiplied alpha, so a transparent border does not leak into the ink.
std::vector<unsigned char> areaReduceRGBA(const unsigned char* src,int sw,int sh,int dw,int dh);

/// Copies the nearest ink color into the transparent texels.
/// GPU filters average RGB without weights and would put a black ring around the ink.
void bleedRGB(unsigned char* rgba,int w,int h,int radius = 16);

/// Same as loadImage, but stores the texture at the size where it will be drawn.
/// A bilinear sample of 2 by 2 texels is only correct up to a reduction of 2 to 1,
/// so a smaller size has to be filtered here and not by the sampler.
ImageData loadImage(path filename,double xscale,double yscale);
// Draws an image at the position of the state, with a color multiplier and a vertical shift in pixels.
void DisplayImage(const ImageData& data,const StateInSlide& sis,scalar scale = 1,const RGBA& tint = RGBA(1.f,1.f,1.f,1.f),scalar y_offset = 0);
// Draws a texture centered at `center`, rotated by angle in radians.
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle,const RGBA& color_mult);

// Loads every frame of a gif.
std::vector<ImageData> loadGif(path filename);


// An image drawn on the screen.
class Image : public ScreenPrimitive {
public:
    using ImagePtr = std::shared_ptr<Image>;

    Image() {}
    ~Image();
    // True when the image was loaded.
    bool isValid() {return data.width != -1;}
    // Draws the image with the state of the slide.
    void display(const StateInSlide& sis) const;

    // Loads an image from a file. The scale multiplies its size in pixels.
    static ImagePtr Add(std::string filename,scalar scale = 1);

    /// Builds an image of size w by h whose pixels come from code and not from a file.
    /// It is transparent until the first updateImage.
    static ImagePtr Blank(int w,int h);

    /// Replaces the pixels with RGBA values, the first row being the top.
    /// The texture is allocated again when the size changes, so a source can change resolution.
    /// A null pointer clears the image to transparent.
    void updateImage(const unsigned char* rgba,int w,int h);

    /// Same, from a file. It is a short way to show something that was just written to disk.
    /// A file that cannot be loaded leaves the image unchanged.
    void updateImage(const std::string& file);

    // Size in pixels of an image file, without loading it as a texture.
    static ImVec2 getSize(std::string filename);
    // Size in pixels of an image drawn with a scale.
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
    // Size in pixels.
    Size getSize() const override;
    bool canRotate() const override {return true;}
};

// An animated image. All frames are kept as textures.
class Gif : public ScreenPrimitive {
public:
    using GifPtr = std::shared_ptr<Gif>;

    // Builds a gif from its frames, played at fps frames per second.
    Gif(const std::vector<ImageData>& images,int fps,scalar scale,bool loop);
    // True when the frames were loaded.
    bool isValid();

    // Draws the current frame with the state of the slide.
    void display(const StateInSlide& sis) const;

    // Loads a gif file. When loop is false, the last frame stays after the end.
    static GifPtr Add(std::string filename,int fps = 10,scalar scale = 1.,bool loop = true);

    void draw(const TimeObject& t, const StateInSlide &sis) override;

    void playIntro(const TimeObject& t, const StateInSlide &sis) override;

    void playOutro(const TimeObject& t, const StateInSlide &sis) override;

    // Size in pixels.
    Size getSize() const override;

    bool canRotate() const override {return true;}

    // Index of the frame shown.
    int current_img = 0;

private:
    // Chooses the frame for the inner time.
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
